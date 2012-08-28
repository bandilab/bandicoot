/*
Copyright 2008-2012 Ostap Cherkashin
Copyright 2008-2012 Julius Chrobak

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

%{
#include "config.h"
#include "array.h"
#include "system.h"
#include "memory.h"
#include "number.h"
#include "string.h"
#include "head.h"
#include "value.h"
#include "tuple.h"
#include "expression.h"
#include "summary.h"
#include "relation.h"

#include "language.h"
#include "environment.h"

/* TODO: make line numbers reported in yyerror more accurate, especially
         for p_convert() which are currently called on
         function reduction */
extern void yyerror(const char *, ...);
extern int yylex();

static int gposition = 0;
static Func *gfunc = NULL;
static Env *genv = NULL;

static L_Attrs attr_name(const char *name);
static L_Attrs attr_decl(L_Attrs attrs, Type type);
static L_Attrs attr_rename(const char *from, const char *to);
static L_Attrs attr_extend(const char *name, L_Expr *e);
static L_Attrs attr_sum(const char *name, L_Sum sum);
static L_Attrs attr_merge(L_Attrs l, L_Attrs r);
static void attr_free(L_Attrs attrs);

static Head *rel_head(L_Attrs attrs);
static Head *inline_rel(const char *name, Head *head);

static Rel *r_load(const char *name);
static Rel *r_project(Rel *r, L_Attrs attrs);
static Rel *r_rename(Rel *r, L_Attrs attrs);
static Rel *r_select(Rel *r, L_Expr *expr);
static Rel *r_extend(Rel *r, L_Attrs attrs);
static Rel *r_sum(Rel *l, Rel *r, L_Attrs attrs);
static Rel *r_join(Rel *l, Rel *r);
static Rel *r_union(Rel *l, Rel *r);
static Rel *r_diff(Rel *l, Rel *r);

static L_Expr *p_attr(const char *name);
static L_Expr *p_value(L_Value val, Type t);
static L_Expr *p_op(L_Expr *l, L_Expr *r, L_Expr_Type node_type);
static L_Expr *p_func(const char *name, L_Expr *e);
static void p_free(L_Expr *e);
static int is_constant(L_Expr *e);

static void stmt_create(L_Stmt_Type type, const char *name, Rel *r);
static void stmt_short(const char *name, Rel *r);

static L_Sum sum_create(const char *func, const char *attr, L_Expr *def);

static void add_head(const char *name, Head *head);
static void add_relvar(const char *rel, L_Attrs vars);
static void add_relvar_inline(Head *head, L_Attrs vars);

static void fn_start(const char *name);
static void fn_rel_params(L_Attrs names, Head *h);
static void fn_prim_params(L_Attrs attrs);
static void fn_result(Head *h);
static void fn_add();
%}

%union {
    char name[MAX_NAME];
    Head *head;

    L_Value val;
    L_Expr *expr;
    L_Attrs attrs;
    L_Sum sum;
    Rel *rel;
};

%token TK_TYPE TK_VAR TK_FN TK_RETURN
%token TK_INT TK_LONG TK_REAL TK_STRING TK_TIME TK_VOID
%token TK_PROJECT TK_RENAME TK_SELECT TK_EXTEND TK_SUMMARY
%token TK_JOIN TK_UNION TK_MINUS
%token TK_EQ TK_NEQ TK_AND TK_OR TK_LTE TK_GTE

%token <name> TK_NAME
%token <val> TK_INT_VAL TK_LONG_VAL TK_REAL_VAL TK_STRING_VAL

%type <attrs> rel_attr rel_attrs attr_names
%type <attrs> project_attr project_attrs
%type <attrs> rename_attr rename_attrs
%type <attrs> extend_attr extend_attrs
%type <attrs> sum_attrs sum_attr
%type <rel> rel_prim_expr rel_simple_expr rel_mul_expr rel_expr
%type <expr> prim_const_expr prim_simple_expr prim_top_expr prim_unary_expr
%type <expr> prim_mul_expr prim_add_expr prim_bool_cmp_expr prim_expr
%type <sum> sum_func
%type <head> rel_head

%start program
%%

program:
      program type_decl
    | program relvar_decl
    | program func_decl
    |
    ;

type_decl:
      TK_TYPE TK_NAME rel_head  { add_head($2, $3); }
    ;

rel_head:
      '{' rel_attrs '}'         { $$ = rel_head($2); }
    | '{' rel_attrs ',' '}'     { $$ = rel_head($2); }
    ;

rel_attrs:
      rel_attr                  { $$ = $1; }
    | rel_attrs ',' rel_attr    { $$ = attr_merge($1, $3);}
    ;

attr_names:
      TK_NAME               { $$ = attr_name($1); }
    | attr_names TK_NAME    { $$ = attr_merge($1, attr_name($2)); }
    ;

rel_attr:
      attr_names TK_INT     { $$ = attr_decl($1, Int); }
    | attr_names TK_LONG    { $$ = attr_decl($1, Long); }
    | attr_names TK_REAL    { $$ = attr_decl($1, Real); }
    | attr_names TK_STRING  { $$ = attr_decl($1, String); }
    ;

relvar_decl:
      TK_VAR attr_names TK_NAME ';'     { add_relvar($3, $2); }
    | TK_VAR attr_names rel_head ';'    { add_relvar_inline($3, $2); }
    ;

func_decl:
      TK_FN func_name '(' func_params ')' func_res '{' func_body '}'
        { fn_add(); }
    ;

func_name:
      TK_NAME   { fn_start($1); }
    ;

func_res:
      TK_VOID   { fn_result(inline_rel(NULL, NULL)); }
    | TK_NAME   { fn_result(inline_rel($1, NULL)); }
    | rel_head  { fn_result(inline_rel(NULL, $1)); }
    ;

func_params:
      func_param
    | func_params ',' func_param
    |
    ;

func_param:
      rel_attr
        { fn_prim_params($1); }
    | attr_names TK_NAME
        { fn_rel_params($1, inline_rel($2, NULL)); }
    | attr_names rel_head
        { fn_rel_params($1, inline_rel(NULL, $2)); }
    ;

func_body:
      stmt_return
    | stmt_assigns
    | stmt_assigns stmt_return
    |
    ;

stmt_return:
      TK_RETURN rel_expr ';'    { stmt_create(RETURN, "", $2); }
    ;

stmt_assigns:
      stmt_assign
    | stmt_assigns stmt_assign
    ;

stmt_assign:
      TK_NAME '=' rel_expr ';'
        { stmt_create(ASSIGN, $1, $3); }
    | TK_NAME '+' '=' rel_expr ';'
        { stmt_create(ASSIGN, $1, r_union(r_load($1), $4)); }
    | TK_NAME '-' '=' rel_expr ';'
        { stmt_create(ASSIGN, $1, r_diff(r_load($1), $4)); }
    | TK_NAME '*' '=' rel_expr ';'
        { stmt_create(ASSIGN, $1, r_join(r_load($1), $4)); }
    | TK_VAR TK_NAME '=' rel_expr ';'
        { stmt_create(TEMP, $2, $4); }
    ;

rel_prim_expr:
      TK_NAME           { $$ = r_load($1); }
    | '(' rel_expr ')'  { $$ = $2; }
    ;

rel_simple_expr:
      rel_prim_expr
        { $$ = $1; }
    | TK_SELECT prim_expr rel_prim_expr
        { $$ = r_select($3, $2); }
    | TK_EXTEND extend_attrs rel_prim_expr
        { $$ = r_extend($3, $2); }
    | TK_PROJECT project_attrs rel_prim_expr
        { $$ = r_project($3, $2); }
    | TK_RENAME rename_attrs rel_prim_expr
        { $$ = r_rename($3, $2); }
    | TK_JOIN rel_prim_expr rel_prim_expr
        { $$ = r_join($2, $3); }
    | TK_UNION rel_prim_expr rel_prim_expr
        { $$ = r_union($2, $3); }
    | TK_MINUS rel_prim_expr rel_prim_expr
        { $$ = r_diff($2, $3); }
    | TK_SUMMARY sum_attrs rel_prim_expr
        { $$ = r_sum($3, NULL, $2); }
    | TK_SUMMARY sum_attrs rel_prim_expr rel_prim_expr
        { $$ = r_sum($3, $4, $2); }
    ;

rel_mul_expr:
      rel_simple_expr                    { $$ = $1; }
    | rel_mul_expr '*' rel_simple_expr   { $$ = r_join($1, $3); }
    ;

rel_expr:
      rel_mul_expr                       { $$ = $1; }
    | rel_expr '+' rel_mul_expr          { $$ = r_union($1, $3); }
    | rel_expr '-' rel_mul_expr          { $$ = r_diff($1, $3); }
    ;

project_attrs:
      project_attr                       { $$ = $1; }
    | project_attrs ',' project_attr     { $$ = attr_merge($1, $3); }
    | '(' project_attrs ')'              { $$ = $2; }
    ;

project_attr:
      TK_NAME                            { $$ = attr_name($1); }
    ;

rename_attrs:
      rename_attr                        { $$ = $1; }
    | rename_attrs ',' rename_attr       { $$ = attr_merge($1, $3); }
    | '(' rename_attrs ')'               { $$ = $2; }
    ;

rename_attr:
      TK_NAME '=' TK_NAME                { $$ = attr_rename($3, $1); }
    ;

extend_attrs:
      extend_attr                        { $$ = $1; }
    | extend_attrs ',' extend_attr       { $$ = attr_merge($1, $3); }
    | '(' extend_attrs ')'               { $$ = $2; }
    ;

extend_attr:
      TK_NAME '=' prim_expr              { $$ = attr_extend($1, $3); }
    ;

sum_attrs:
      sum_attr                  { $$ = $1; }
    | sum_attrs ',' sum_attr    { $$ = attr_merge($1, $3); }
    | '(' sum_attrs ')'         { $$ = $2; }
    ;

sum_attr:
      TK_NAME '=' sum_func      { $$ = attr_sum($1, $3); }
    ;

sum_func:
      TK_NAME
        { $$ = sum_create($1, "", NULL); }
    | '(' TK_NAME ')'
        { $$ = sum_create($2, "", NULL); }
    | '(' TK_NAME TK_NAME prim_expr ')'
        { $$ = sum_create($2, $3, $4); }
    ;

/* TODO: func calls (rounding, floor, ceiling, abs) */
prim_const_expr:
      TK_INT_VAL                                { $$ = p_value($1, Int); }
    | TK_LONG_VAL                               { $$ = p_value($1, Long); }
    | TK_REAL_VAL                               { $$ = p_value($1, Real); }
    | TK_STRING_VAL                             { $$ = p_value($1, String); }
    ;

prim_simple_expr:
      prim_const_expr                           { $$ = $1; }
    | '(' prim_expr ')'                         { $$ = $2; }
    | '(' TK_INT prim_expr ')'                  { $$ = p_func("int", $3); }
    | '(' TK_REAL prim_expr ')'                 { $$ = p_func("real", $3); }
    | '(' TK_LONG prim_expr ')'                 { $$ = p_func("long", $3); }
    ;

prim_top_expr:
      prim_simple_expr                          { $$ = $1; }
    | TK_NAME                                   { $$ = p_attr($1); }
    ;

prim_unary_expr:
      prim_top_expr                             { $$ = $1; }
    | '!' prim_unary_expr                       { $$ = p_op($2, NULL, NOT); }
    | '-' prim_simple_expr                      { $$ = p_op($2, NULL, NEG); }
    | '+' prim_simple_expr                      { $$ = $2; }
    ;

prim_mul_expr:
      prim_unary_expr                           { $$ = $1; }
    | prim_mul_expr '*' prim_unary_expr         { $$ = p_op($1, $3, MUL); }
    | prim_mul_expr '/' prim_unary_expr         { $$ = p_op($1, $3, DIV); }
    ;

prim_add_expr:
      prim_mul_expr                             { $$ = $1; }
    | prim_add_expr '+' prim_mul_expr           { $$ = p_op($1, $3, SUM); }
    | prim_add_expr '-' prim_mul_expr           { $$ = p_op($1, $3, SUB); }
    ;

prim_bool_cmp_expr:
      prim_add_expr                             { $$ = $1; }
    | prim_bool_cmp_expr TK_EQ prim_add_expr    { $$ = p_op($1, $3, EQ); }
    | prim_bool_cmp_expr TK_NEQ prim_add_expr   { $$ = p_op($1, $3, NEQ); }
    | prim_bool_cmp_expr '>' prim_add_expr      { $$ = p_op($1, $3, GT); }
    | prim_bool_cmp_expr '<' prim_add_expr      { $$ = p_op($1, $3, LT); }
    | prim_bool_cmp_expr TK_LTE prim_add_expr   { $$ = p_op($1, $3, LTE); }
    | prim_bool_cmp_expr TK_GTE prim_add_expr   { $$ = p_op($1, $3, GTE); }
    ;

prim_expr:
      prim_bool_cmp_expr                        { $$ = $1; }
    | prim_expr TK_AND prim_bool_cmp_expr       { $$ = p_op($1, $3, AND); }
    | prim_expr TK_OR prim_bool_cmp_expr        { $$ = p_op($1, $3, OR); }
    ;

%%

/* FIXME: the tbuf_clean needs to called for TEMP and PARAM relations
    ideally this is not exposed and is handled within the relations themselves
 */
extern void env_free(Env *env)
{
    for (int i = 0; i < env->vars.len; ++i) {
        mem_free(env->vars.names[i]);
        mem_free(env->vars.heads[i]);
    }

    for (int k = 0; k < env->types.len; ++k) {
        mem_free(env->types.names[k]);
        mem_free(env->types.heads[k]);
    }

    for (int x = 0; x < env->fns.len; ++x) {
        Func *fn = env->fns.funcs[x];
        for (int j = 0; j < fn->r.len; ++j)
            mem_free(fn->r.names[j]);
        for (int j = 0; j < fn->w.len; ++j) {
            mem_free(fn->w.names[j]);
            rel_free(fn->w.rels[j]);
        }
        for (int j = 0; j < fn->t.len; ++j) {
            mem_free(fn->t.names[j]);
            Rel *r = fn->t.rels[j];
            if (r->body != NULL)
                tbuf_clean(r->body);
            rel_free(r);
        }
        if (fn->rp.name != NULL) {
            mem_free(fn->rp.name);
            Rel *r = fn->rp.rel;
            if (r->body != NULL)
                tbuf_clean(r->body);
            rel_free(r);
        }
        for (int j = 0; j < fn->pp.len; ++j)
            mem_free(fn->pp.names[j]);
        if (fn->ret != NULL)
            rel_free(fn->ret);

        mem_free(fn->head);
        mem_free(fn);
    }

    mem_free(env);
}

extern Env *grammar_init()
{
    genv = mem_alloc(sizeof(Env));
    genv->types.len = 0;
    genv->vars.len = 0;
    genv->fns.len = 0;

    return genv;
}

extern Head *env_head(Env *env, const char *var)
{
    int i = array_scan(env->vars.names, env->vars.len, var);
    if (i < 0)
        return NULL;

    return env->vars.heads[i];
}

extern Func *env_func(Env *env, const char *name)
{
    int idx = array_scan(env->fns.names, env->fns.len, name);
    return idx < 0 ? NULL : env->fns.funcs[idx];
}

extern Func **env_funcs(Env *env, const char *name, int *cnt)
{
    int len = str_len(name);
    if (len == 0)
        *cnt = env->fns.len;
    else
        *cnt = array_freq(env->fns.names, env->fns.len, name);

    if (*cnt == 0)
        return NULL;

    Func **res = mem_alloc(sizeof(Func*) * (*cnt));
    for (int j = 0, i = 0; i < env->fns.len; ++i)
        if (len == 0 || str_cmp(name, env->fns.names[i]) == 0)
            res[j++] = env->fns.funcs[i];

    return res;
}

static L_Attrs attr_name(const char *name)
{
    L_Attrs res = {.len = 1,
                   .names[0] = str_dup(name),
                   .renames[0] = NULL,
                   .types[0] = -1,
                   .exprs[0] = NULL,
                   .sums[0].def = NULL};
    return res;
}

static L_Attrs attr_decl(L_Attrs attrs, Type type)
{
    for (int i = 0; i < attrs.len; ++i)
        attrs.types[i] = type;

    return attrs;
}

static L_Attrs attr_rename(const char *from, const char *to)
{
    L_Attrs res = attr_name(from);
    res.renames[0] = str_dup(to);
    return res;
}

static L_Attrs attr_extend(const char *name, L_Expr *e)
{
    L_Attrs res = attr_name(name);
    res.exprs[0] = e;
    return res;
}

static L_Attrs attr_sum(const char *name, L_Sum sum)
{
    L_Attrs res = attr_name(name);
    res.types[0] = sum.def->type;
    res.sums[0] = sum;
    return res;
}

static L_Attrs attr_merge(L_Attrs l, L_Attrs r)
{
    if (l.len + r.len > MAX_ATTRS)
        yyerror("number of attributes exceeds the maximum (%d)", MAX_ATTRS);

    for (int i = 0; i < r.len && l.len < MAX_ATTRS; ++i) {
        if (array_scan(l.names, l.len, r.names[i]) > -1)
            yyerror("attribute '%s' is already used", r.names[i]);

        l.names[l.len] = r.names[i];
        l.renames[l.len] = r.renames[i];
        l.types[l.len] = r.types[i];
        l.exprs[l.len] = r.exprs[i];
        l.sums[l.len] = r.sums[i];

        l.len++;
    }

    return l;
}

static L_Sum sum_create(const char *func, const char *attr, L_Expr *def)
{
    L_Sum res;
    if (str_cmp(func, "cnt") == 0) {
        res.sum_type = CNT;
        L_Value v = {.v_int = 0};
        def = p_value(v, Int);
    } else if (str_cmp(func, "avg") == 0)
        res.sum_type = AVG;
    else if (str_cmp(func, "max") == 0)
        res.sum_type = MAX;
    else if (str_cmp(func, "min") == 0)
        res.sum_type = MIN;
    else if (str_cmp(func, "add") == 0)
        res.sum_type = ADD;
    else
        yyerror("unkown function in summary operator '%s'", func);

    if (def == NULL)
        yyerror("missing default value for the summary operator '%s'", func);

    str_cpy(res.attr, attr);
    res.def = def;

    return res;
}

static void attr_free(L_Attrs a) {
    for (int i = 0; i < a.len; ++i) {
        mem_free(a.names[i]);
        if (a.renames[i] != NULL)
            mem_free(a.renames[i]);
        if (a.exprs[i] != NULL)
            p_free(a.exprs[i]);
        if (a.sums[i].def != NULL)
            mem_free(a.sums[i].def);
    }
}

static Head *rel_head(L_Attrs attrs)
{
    Head *res = head_new(attrs.names, attrs.types, attrs.len);
    attr_free(attrs);

    return res;
}

static void add_head(const char *name, Head *head)
{
    if (array_scan(genv->types.names, genv->types.len, name) > -1)
        yyerror("type '%s' is already defined", name);
    else if (genv->types.len == MAX_TYPES)
        yyerror("number of type declarations "
                "exceeds the maximum (%d)", MAX_TYPES);
    else {
        int i = genv->types.len++;
        genv->types.names[i] = str_dup(name);
        genv->types.heads[i] = head;
    }
}

static void add_relvar_inline(Head *head, L_Attrs vars)
{
    for (int i = 0; i < vars.len; ++i) {
        const char *var = vars.names[i];
        if (array_scan(genv->vars.names, genv->vars.len, var) > -1)
            yyerror("variable '%s' is already defined", var);
        else if (array_scan(genv->types.names, genv->types.len, var) > -1)
            yyerror("type '%s' cannot be used as a variable name", var);
        else if (genv->vars.len == MAX_VARS)
            yyerror("number of global variables exceeds the maximum (%d)",
                    MAX_VARS);
        else {
            int j = genv->vars.len++;
            genv->vars.names[j] = str_dup(var);
            genv->vars.heads[j] = head_cpy(head);
        }
    }
    mem_free(head);
    attr_free(vars);
}

static void add_relvar(const char *rel, L_Attrs vars)
{
    int i = array_scan(genv->types.names, genv->types.len, rel);
    if (i < 0) {
        attr_free(vars);
        yyerror("unknown type '%s'", rel);
    } else
        add_relvar_inline(head_cpy(genv->types.heads[i]), vars);
}

static int func_param(Func *fn, char *name, int *pos, Type *type) {
    int res = 0;
    *pos = array_scan(fn->pp.names, fn->pp.len, name);

    if (*pos > -1) {
        res = 1;
        *type = fn->pp.types[*pos];
    }

    return res;
}

static Expr *p_convert(Head *h, Func *fn, L_Expr *e, L_Expr_Type parent_type)
{
    char hstr[MAX_HEAD_STR];
    head_to_str(hstr, h);

    Expr *res = NULL, *l = NULL, *r = NULL;
    L_Expr_Type t = e->node_type;

    if (e->left != NULL)
        l = p_convert(h, fn, e->left, t);
    if (e->right != NULL)
        r = p_convert(h, fn, e->right, t);

    if (t == ATTR) {
        int pos;
        Type type;
        if (!head_attr(h, e->name, &pos, &type)) {
            if (!func_param(fn, e->name, &pos, &type))
                yyerror("unknown attribute '%s' in %s", e->name, hstr);

            res = expr_param(pos, type);
        } else {
            res = expr_attr(pos, type);
        }
    } else if (t == VALUE && e->type == Int) {
        int i = e->val.v_int;
        if (int_overflow((parent_type == NEG ? -1 : 1), i))
            yyerror("int constant value overflow %u", i);

        res = expr_int(i);
    } else if (t == VALUE && e->type == Long) {
        long long l = e->val.v_long;
        if (long_overflow((parent_type == NEG ? -1 : 1), l))
            yyerror("long constant value overflow %llu", l);
    
        res = expr_long(l);
    } else if (t == VALUE && e->type == Real)
        res = expr_real(e->val.v_real);
    else if (t == VALUE && e->type == String)
        res = expr_str(e->val.v_str);
    else if (t == NOT) {
        if (l->type != Int)
            yyerror("expressions must be of type 'int', found '%s'",
                    type_to_str(l->type));
        res = expr_not(l);
    } else if (t == NEG) {
        if (l->type == Int)
            res = expr_mul(l, expr_int(-1));
        else if (l->type == Real)
            res = expr_mul(l, expr_real(-1));
        else if (l->type == Long)
            res = expr_mul(l, expr_long(-1));
        else
            yyerror("NEG operator does not support '%s' type",
                    type_to_str(l->type));
    } else if (t == FUNC) {
        Type to = 0;
        if (str_cmp("int", e->name) == 0)
            to = Int;
        else if (str_cmp("real", e->name) == 0)
            to = Real;
        else if (str_cmp("long", e->name) == 0)
            to = Long;
        else
            yyerror("unknown function '%s' in primitive expression", e->name);

        if (l->type == String)
            yyerror("conversion from string is not supported");

        res = expr_conv(l, to);
    } else {
        if (l->type != r->type)
            yyerror("expressions must be of the same type, found '%s' and '%s'",
                    type_to_str(l->type), type_to_str(r->type));

        if (t == EQ) {
            res = expr_eq(l, r);
        } else if (t == NEQ) {
            res = expr_not(expr_eq(l, r));
        } else if (t == AND) {
            if (l->type != Int)
                yyerror("AND operator expects int but found %s",
                        type_to_str(l->type));
            res = expr_and(l, r);
        } else if (t == OR) {
            if (l->type != Int)
                yyerror("OR operator expects int but found %s",
                        type_to_str(l->type));
            res = expr_or(l, r);
        } else if (t == GT) {
            res = expr_gt(l, r);
        } else if (t == LT) {
            res = expr_lt(l, r);
        } else if (t == GTE) {
            res = expr_gte(l, r);
        } else if (t == LTE) {
            res = expr_lte(l, r);
        } else if (t == SUM) {
            if (l->type == String)
                yyerror("SUM operator does not support String type");
            res = expr_sum(l, r);
        } else if (t == SUB) {
            if (l->type == String)
                yyerror("SUB operator does not support String type");
            res = expr_sub(l, r);
        } else if (t == MUL) {
            if (l->type == String)
                yyerror("MUL operator does not support String type");
            res = expr_mul(l, r);
        } else if (t == DIV) {
            if (l->type == String)
                yyerror("DIV operator does not support String type");
            res = expr_div(l, r);
        }
    }

    return res;
}

static void stmt_create(L_Stmt_Type type, const char *name, Rel *r)
{
    Func *fn = gfunc;

    if (fn->w.len + fn->t.len >= MAX_STMTS)
        yyerror("number of statements exceeds the maximum (%d)", MAX_STMTS);

    if (type == ASSIGN) {
        int idx = array_scan(fn->w.names, fn->w.len, name);
        if (idx != -1)
            yyerror("cannot reassign variable '%s'", name);

        idx = array_scan(genv->vars.names, genv->vars.len, name);
        if (idx < 0)
            yyerror("unknown variable '%s'", name);

        Head *wh = genv->vars.heads[idx];
        char wstr[MAX_HEAD_STR], bstr[MAX_HEAD_STR];
        head_to_str(wstr, wh);
        head_to_str(bstr, r->head);

        if (!head_eq(r->head, wh))
            yyerror("invalid type in assignment, expects %s, found %s",
                    wstr, bstr);

        fn->w.names[fn->w.len] = str_dup(name);
        fn->w.rels[fn->w.len++] = r;
    } else if (type == RETURN) {
        if (fn->head == NULL)
            yyerror("unexpected return in function with void result type");

        Head *h = r->head;

        char hstr[MAX_HEAD_STR], res_str[MAX_HEAD_STR];
        head_to_str(hstr, h);
        head_to_str(res_str, fn->head);

        int pos;
        Type t;
        for (int i = 0; i < fn->head->len; ++i)
            if (!head_attr(h, fn->head->names[i], &pos, &t) ||
                t != fn->head->types[i])
                yyerror("invalid type in return, expects %s, found %s",
                        res_str, hstr);

        fn->ret = rel_project(r, fn->head->names, fn->head->len);
    } else if (type == TEMP) {
        if ((array_scan(fn->t.names, fn->t.len, name) != -1) ||
            (array_scan(genv->vars.names, genv->vars.len, name) != -1))
            yyerror("variable '%s' is already defined", name);

        fn->t.names[fn->t.len] = str_dup(name);
        fn->t.rels[fn->t.len++] = r;
    }
}

static Head *inline_rel(const char *name, Head *head)
{
    Head *res = NULL;
    if (name == NULL && head != NULL) {
        res = head;
    } else if (name != NULL) {
        int idx = array_scan(genv->types.names, genv->types.len, name);
        if (idx < 0)
            yyerror("unknown type '%s'", name);

        res = head_cpy(genv->types.heads[idx]);
    }

    return res;
}

static void fn_result(Head *h)
{
    gfunc->head = h;
}

static void fn_rel_params(L_Attrs names, Head *h)
{
    if (names.len > 1 || gfunc->rp.name != NULL)
        yyerror("only one relational parameter is supported");

    if (array_scan(genv->vars.names, genv->vars.len, names.names[0]) > -1)
        yyerror("identifier '%s' is already used for a global variable",
                names.names[0]);

    gfunc->rp.name = str_dup(names.names[0]);
    gfunc->rp.rel = rel_param(h);
    gfunc->rp.position = ++gposition;
    mem_free(h); /* FIXME: it is duplicated in rel_param */
    attr_free(names);
}

static void fn_prim_params(L_Attrs attrs)
{
    if (gfunc->pp.len + attrs.len > MAX_ATTRS)
        yyerror("number of primitive parameters exceeds the maximum (%d)",
                MAX_ATTRS);

    for (int i = 0; i < attrs.len; ++i) {
        if (array_scan(gfunc->pp.names, gfunc->pp.len, attrs.names[i]) > -1)
            yyerror("primitive parameter '%s' is already defined",
                    attrs.names[i]);

        int pos = gfunc->pp.len++;
        gfunc->pp.types[pos] = attrs.types[i];
        gfunc->pp.names[pos] = str_dup(attrs.names[i]);
        gfunc->pp.positions[pos] = ++gposition;
    }
    attr_free(attrs);
}

static void fn_start(const char *name)
{
    if (array_scan(genv->fns.names, genv->fns.len, name) > -1)
        yyerror("function '%s' is already defined", name);

    gposition = 0;
    gfunc = mem_alloc(sizeof(Func));
    str_cpy(gfunc->name, name);
    gfunc->head = NULL;
    gfunc->ret = NULL;
    gfunc->r.len = 0;
    gfunc->w.len = 0;
    gfunc->t.len = 0;
    gfunc->pp.len = 0;
    gfunc->rp.name = NULL;
    gfunc->rp.rel = NULL;
    gfunc->rp.position = 0;
}

static void fn_add()
{
    if (gfunc->head != NULL && gfunc->ret == NULL)
        yyerror("function '%s' is missing a return statement", gfunc->name);

    if (genv->fns.len + 1 >= MAX_VARS)
        yyerror("number of functions exceeds maximum (%d)", MAX_VARS);

    int len = genv->fns.len++;
    genv->fns.names[len] = gfunc->name;
    genv->fns.funcs[len] = gfunc;

    gfunc = NULL;
}

static Rel *r_load(const char *name)
{
    Func *fn = gfunc;

    int i = -1;
    Rel *res = NULL;
    if (fn->rp.name != NULL && str_cmp(fn->rp.name, name) == 0)
        res = rel_clone(fn->rp.rel);
    else if ((i = array_scan(fn->t.names, fn->t.len, name)) > -1)
        res = rel_clone(fn->t.rels[i]);
    else if ((i = array_scan(genv->vars.names, genv->vars.len, name)) > -1) {
        if (array_scan(fn->w.names, fn->w.len, name) > -1)
            yyerror("variable '%s' cannot be read (it was modified by "
                    "one of the previous statements)", name);
        if (array_scan(fn->r.names, fn->r.len, name) < 0)
            fn->r.names[fn->r.len++] = str_dup(name);

        res = rel_load(genv->vars.heads[i], name);
    } else
        yyerror("unknown variable '%s'", name);

    return res;
}

static Rel *r_project(Rel *r, L_Attrs attrs)
{
    char hstr[MAX_HEAD_STR];
    head_to_str(hstr, r->head);

    for (int i = 0; i < attrs.len; ++i)
        if (!head_find(r->head, attrs.names[i]))
            yyerror("unknown attribute '%s' in %s", attrs.names[i], hstr);

    Rel *res = rel_project(r, attrs.names, attrs.len);
    attr_free(attrs);

    return res;
}

static Rel *r_rename(Rel *r, L_Attrs attrs)
{
    char hstr[MAX_HEAD_STR];
    head_to_str(hstr, r->head);

    char **renames = attrs.renames;

    for (int i = 0; i < attrs.len; ++i) {
        int j = array_scan(renames, attrs.len, renames[i]);

        /* TODO: below checks are too strict, we should be able to do
                  x { a, b, c } - x rename(a = b, b = c, c = d); */
        if (!head_find(r->head, attrs.names[i]))
            yyerror("unknown attribute '%s' in %s", attrs.names[i], hstr);
        if (head_find(r->head, renames[i]) || (j > -1 && j != i))
            yyerror("attribute '%s' already exists in %s",
                    renames[i], hstr);
    }

    Rel *res = rel_rename(r, attrs.names, renames, attrs.len);
    attr_free(attrs);

    return res;
}

static Rel *r_select(Rel *r, L_Expr *expr)
{
    Rel *res = rel_select(r, p_convert(r->head, gfunc, expr, POS));
    p_free(expr);

    return res;
}

static Rel *r_extend(Rel *r, L_Attrs attrs)
{
    char hstr[MAX_HEAD_STR];
    head_to_str(hstr, r->head);

    Expr *extends[MAX_ATTRS];
    for (int i = 0; i < attrs.len; ++i) {
        if (head_find(r->head, attrs.names[i]))
            yyerror("attribute '%s' already exists in %s",
                    attrs.names[i], hstr);

        extends[i] = p_convert(r->head, gfunc, attrs.exprs[i], POS);
    }

    if (r->head->len + attrs.len > MAX_ATTRS)
        yyerror("extend result type exceeds the maximum number "
                "of attributes (%d)", MAX_ATTRS);

    Rel *res = rel_extend(r, attrs.names, extends, attrs.len);
    attr_free(attrs);

    return res;
}

static Rel *r_sum(Rel *l, Rel *r, L_Attrs attrs)
{
    char lhstr[MAX_HEAD_STR], rhstr[MAX_HEAD_STR];
    head_to_str(lhstr, l->head);
    if (r != NULL)
        head_to_str(rhstr, r->head);

    Rel *res = NULL;
    Sum *sums[MAX_ATTRS];
    for (int i = 0; i < attrs.len; ++i) {
        L_Sum s = attrs.sums[i];

        int lpos[MAX_ATTRS], rpos[MAX_ATTRS];
        if (r != NULL) {
            if (head_common(l->head, r->head, lpos, rpos) == 0)
                yyerror("use of summary with no commmon attributes "
                        "(%s and %s)", lhstr, rhstr);

            if (head_find(r->head, attrs.names[i]))
                yyerror("attribute '%s' already exists in per %s",
                        attrs.names[i], rhstr);
        }

        if (s.sum_type == CNT)
            sums[i] = sum_cnt();
        else {
            if (!is_constant(s.def))
                yyerror("only constant expressions are allowed for "
                        "default values");
            int pos;
            Type stype;
            if (!head_attr(l->head, s.attr, &pos, &stype))
                yyerror("unknown attribute '%s' in %s", s.attr, lhstr);

            Type exp_type = stype;
            if (s.sum_type == AVG)
                exp_type = Real;

            Expr *def = p_convert(l->head, gfunc, s.def, POS);
            if (def->type != exp_type)
                yyerror("invalid type of default value, expected '%s', "
                        "found %s",
                        type_to_str(exp_type),
                        type_to_str(def->type));

            if (def->type == String || stype == String)
                yyerror("invalid type of default value: string");

            Value v = expr_new_val(def, NULL, NULL);
            if (s.sum_type == AVG)
                sums[i] = sum_avg(pos, stype, v);
            else if (s.sum_type == MIN)
                sums[i] = sum_min(pos, stype, v);
            else if (s.sum_type == MAX)
                sums[i] = sum_max(pos, stype, v);
            else if (s.sum_type == ADD)
                sums[i] = sum_add(pos, stype, v);

            expr_free(def);
        }
    }

    if (r == NULL)
        res = rel_sum_unary(l, attrs.names, attrs.types, sums, attrs.len);
    else {
        if (r->head->len + attrs.len > MAX_ATTRS)
            yyerror("summary result type exceeds the maximum number "
                    "of attributes (%d)", MAX_ATTRS);

        res = rel_sum(l, r, attrs.names, attrs.types, sums, attrs.len);
    }
    attr_free(attrs);

    return res;
}

static Rel *r_join(Rel *l, Rel *r)
{
    char lhstr[MAX_HEAD_STR];
    head_to_str(lhstr, l->head);
    int num_attrs = l->head->len + r->head->len;

    for (int i = 0; i < l->head->len; ++i) {
        char *n = l->head->names[i];
        Type t; int pos;
        if (head_attr(r->head, n, &pos, &t)) {
            num_attrs--;

            if (t != l->head->types[i])
                yyerror("attribute '%s' is of different type in right %s",
                        n, lhstr);
        }
    }

    if (num_attrs > MAX_ATTRS)
        yyerror("join result type exceeds the maximum number "
                "of attributes (%d)", MAX_ATTRS);

    return rel_join(l, r);
}

static Rel *r_union(Rel *l, Rel *r)
{
    char lhstr[MAX_HEAD_STR], rhstr[MAX_HEAD_STR];
    head_to_str(lhstr, l->head);
    head_to_str(rhstr, r->head);

    if (!head_eq(l->head, r->head))
        yyerror("use of union with different types (%s and %s)",
                lhstr, rhstr);

    return rel_union(l, r);
}

static Rel *r_diff(Rel *l, Rel *r)
{
    char lhstr[MAX_HEAD_STR], rhstr[MAX_HEAD_STR];
    head_to_str(lhstr, l->head);
    head_to_str(rhstr, r->head);

    int lpos[MAX_ATTRS], rpos[MAX_ATTRS];
    if (head_common(l->head, r->head, lpos, rpos) == 0)
        yyerror("use of semidiff with no commmon attributes (%s and %s)",
                lhstr, rhstr);

    return rel_diff(l, r);
}

static void p_free(L_Expr *e)
{
    if (e->left != NULL)
        p_free(e->left);
    if (e->right != NULL)
        p_free(e->right);

    mem_free(e);
}

static L_Expr *p_alloc(L_Expr_Type t)
{
    L_Expr *res = mem_alloc(sizeof(L_Expr));
    res->node_type = t;
    res->type = 0;
    res->left = NULL;
    res->right = NULL;
    res->val.v_long = 0;
    res->name[0] = '\0';
    res->is_const = 1;

    return res;
}

static L_Expr *p_attr(const char *name)
{
    L_Expr *res = p_alloc(ATTR);
    str_cpy(res->name, name);
    res->is_const = 0;

    return res;
}

static L_Expr *p_func(const char *name, L_Expr *e)
{
    L_Expr *res = p_alloc(FUNC);
    str_cpy(res->name, name);
    res->is_const = 0;
    res->left = e;

    return res;
}

static L_Expr *p_value(L_Value val, Type t)
{
    L_Expr *res = p_alloc(VALUE);
    res->type = t;
    res->val = val;

    return res;
}

static L_Expr *p_op(L_Expr *l, L_Expr *r, L_Expr_Type node_type)
{
    L_Expr *res = p_alloc(node_type);
    res->left = l;
    res->right = r;
    res->type = l->type;

    return res;
}

static int is_constant(L_Expr *e)
{
    int res = e->is_const;

    if (e->left != NULL)
        res = res && is_constant(e->left);
    if (e->right != NULL)
        res = res && is_constant(e->right);
        
    return res;    
}

extern int env_compat(Env *old, Env *new)
{
    for (int i = 0; i < old->vars.len; ++i) {
        Head *h = env_head(new, old->vars.names[i]);
        if (h != NULL && !head_eq(h, old->vars.heads[i]))
            return 0;
    }

    return 1;
}
