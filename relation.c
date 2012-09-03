/*
Copyright 2008-2012 Ostap Cherkashin
Copyright 2008-2011 Julius Chrobak

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

#include "config.h"
#include "array.h"
#include "system.h"
#include "memory.h"
#include "string.h"
#include "head.h"
#include "value.h"
#include "tuple.h"
#include "volume.h"
#include "expression.h"
#include "summary.h"
#include "variable.h"
#include "index.h"
#include "relation.h"
#include "environment.h"

typedef struct {
    Rel *left;
    Rel *right;

    /* load, call (function name) */
    char name[MAX_NAME];

    /* join, union, diff, project, binary sum */
    struct {
        int len;
        int lpos[MAX_ATTRS];
        int rpos[MAX_ATTRS];
    } e, j;

    /* rename */
    int acnt;
    int apos[MAX_ATTRS];

    /* select, extend */
    int ecnt;
    Expr *exprs[MAX_ATTRS];

    /* unary & binary sum */
    int scnt;
    Sum *sums[MAX_ATTRS];

    /* function call */
    int slen;
    Rel *stmts[MAX_STMTS];
    struct {
        int len;
        char *names[MAX_VARS];
    } r, w, t;
} Ctxt;

extern void rel_eval(Rel *r, Vars *v, Arg *a)
{
    if (r->eval != NULL)
        r->eval(r, v, a);
    if (r->body != NULL)
        tbuf_reset(r->body);
}

extern void rel_free(Rel *r)
{
    r->free(r);
    if (r->body != NULL)
        tbuf_free(r->body);

    mem_free(r->head);
    mem_free(r);
}

static void free(Rel *r)
{
    Ctxt *c = r->ctxt;
    if (c->left != NULL)
        rel_free(c->left);
    if (c->right != NULL)
        rel_free(c->right);

    for (int i = 0; i < c->ecnt; ++i)
        expr_free(c->exprs[i]);
    for (int i = 0; i < c->scnt; ++i)
        mem_free(c->sums[i]);
}

static Rel *alloc(void (*eval)(Rel *r, Vars *s, Arg *a))
{
    Rel *r = mem_alloc(sizeof(Rel) + sizeof(Ctxt));
    r->head = NULL;
    r->body = NULL;
    r->ctxt = r + 1;
    r->eval = eval;
    r->free = free;

    Ctxt *c = r->ctxt;
    c->left = NULL;
    c->right = NULL;
    c->name[0] = '\0';
    c->e.len = 0;
    c->j.len = 0;
    c->r.len = 0;
    c->w.len = 0;
    c->t.len = 0;
    c->acnt = 0;
    c->ecnt = 0;
    c->scnt = 0;
    c->slen = 0;

    return r;
}

static void eval_load(Rel *r, Vars *v, Arg *a)
{
    Ctxt *c = r->ctxt;
    r->body = tbuf_new();

    int pos = array_scan(v->names, v->len, c->name);
    tbuf_reset(v->vals[pos]);

    Tuple *t;
    while ((t = tbuf_next(v->vals[pos])) != NULL)
        tbuf_add(r->body, tuple_cpy(t));
}

extern Rel *rel_load(Head *head, const char *name)
{
    Rel *res = alloc(eval_load);
    res->head = head_cpy(head);

    Ctxt *c = res->ctxt;
    str_cpy(c->name, name);

    return res;
}

static void eval_join(Rel *r, Vars *v, Arg *a)
{
    Ctxt *c = r->ctxt;
    r->body = tbuf_new();

    rel_eval(c->left, v, a);
    rel_eval(c->right, v, a);

    TBuf *lb = c->left->body;
    index_sort(lb, c->e.lpos, c->e.len);

    Tuple *lt, *rt;
    while ((rt = tbuf_next(c->right->body)) != NULL) {
        TBuf *m = index_match(lb, rt, c->e.lpos, c->e.rpos, c->e.len);

        if (m != NULL) {
            while ((lt = tbuf_next(m)) != NULL) {
                Tuple *t = tuple_join(lt, rt, c->j.lpos, c->j.rpos, c->j.len);
                tbuf_add(r->body, t);
            }

            tbuf_free(m);
        }

        tuple_free(rt);
    }

    tbuf_clean(lb);
}

extern Rel *rel_join(Rel *l, Rel *r)
{
    Rel *res = alloc(eval_join);

    Ctxt *c = res->ctxt;
    res->head = head_join(l->head, r->head, c->j.lpos, c->j.rpos, &c->j.len);
    c->e.len = head_common(l->head, r->head, c->e.lpos, c->e.rpos);
    c->left = l;
    c->right = r;

    return res;
}

static void eval_union(Rel *r, Vars *v, Arg *a)
{
    Ctxt *c = r->ctxt;
    r->body = tbuf_new();

    rel_eval(c->left, v, a);
    rel_eval(c->right, v, a);

    TBuf *rb = c->right->body;
    index_sort(rb, c->e.rpos, c->e.len);

    Tuple *lt, *rt;
    while ((lt = tbuf_next(c->left->body)) != NULL)
        if (index_has(rb, lt, c->e.rpos, c->e.lpos, c->e.len))
            tuple_free(lt);
        else
            tbuf_add(r->body, lt);

    tbuf_reset(rb);
    while ((rt = tbuf_next(rb)) != NULL)
        tbuf_add(r->body, rt);
}

extern Rel *rel_union(Rel *l, Rel *r)
{
    Rel *res = alloc(eval_union);
    res->head = head_cpy(l->head);

    Ctxt *c = res->ctxt;
    c->e.len = head_common(l->head, r->head, c->e.lpos, c->e.rpos);
    c->left = l;
    c->right = r;

    return res;
}

static void eval_diff(Rel *r, Vars *v, Arg *a)
{
    Ctxt *c = r->ctxt;
    r->body = tbuf_new();

    rel_eval(c->left, v, a);
    rel_eval(c->right, v, a);

    TBuf *rb = c->right->body;
    index_sort(rb, c->e.rpos, c->e.len);

    Tuple *lt;
    while ((lt = tbuf_next(c->left->body)) != NULL)
        if (index_has(rb, lt, c->e.rpos, c->e.lpos, c->e.len))
            tuple_free(lt);
        else
            tbuf_add(r->body, lt);

    tbuf_clean(rb);
}

extern Rel *rel_diff(Rel *l, Rel *r)
{
    Rel *res = alloc(eval_diff);
    res->head = head_cpy(l->head);

    Ctxt *c = res->ctxt;
    c->e.len = head_common(l->head, r->head, c->e.lpos, c->e.rpos);
    c->left = l;
    c->right = r;

    return res;
}

static void eval_project(Rel *r, Vars *v, Arg *a)
{
    Ctxt *c = r->ctxt;
    r->body = tbuf_new();

    rel_eval(c->left, v, a);
    index_sort(c->left->body, c->e.lpos, c->e.len);

    Tuple *t;
    while ((t = tbuf_next(c->left->body)) != NULL) {
        if (!index_has(r->body, t, c->e.rpos, c->e.lpos, c->e.len))
            tbuf_add(r->body, tuple_reord(t, c->e.lpos, c->e.len));

        tuple_free(t);
    }
}

extern Rel *rel_project(Rel *r, char *names[], int len)
{
    Rel *res = alloc(eval_project);
    res->head = head_project(r->head, names, len);

    Ctxt *c = res->ctxt;
    c->left = r;
    c->e.len = head_common(r->head, res->head, c->e.lpos, c->e.rpos);

    return res;
}

static void eval_rename(Rel *r, Vars *v, Arg *a)
{
    Ctxt *c = r->ctxt;
    r->body = tbuf_new();

    rel_eval(c->left, v, a);

    Tuple *t;
    while ((t = tbuf_next(c->left->body)) != NULL) {
        tbuf_add(r->body, tuple_reord(t, c->apos, c->acnt));
        tuple_free(t);
    }
}

extern Rel *rel_rename(Rel *r, char *from[], char *to[], int len)
{
    Rel *res = alloc(eval_rename);

    Ctxt *c = res->ctxt;
    res->head = head_rename(r->head, from, to, len, c->apos, &c->acnt);
    c->left = r;

    return res;
}

static void eval_select(Rel *r, Vars *v, Arg *a)
{
    Ctxt *c = r->ctxt;
    r->body = tbuf_new();

    rel_eval(c->left, v, a);

    Tuple *t;
    while ((t = tbuf_next(c->left->body)) != NULL)
        if (expr_bool_val(c->exprs[0], t, a))
            tbuf_add(r->body, t);
        else
            tuple_free(t);
}

extern Rel *rel_select(Rel *r, Expr *bool_expr)
{
    Rel *res = alloc(eval_select);
    res->head = head_cpy(r->head);

    Ctxt *c = res->ctxt;
    c->left = r;
    c->ecnt = 1;
    c->exprs[0] = bool_expr;

    return res;
}

static void eval_extend(Rel *r, Vars *v, Arg *a)
{
    Ctxt *c = r->ctxt;
    r->body = tbuf_new();

    rel_eval(c->left, v, a);

    Tuple *t;
    while ((t = tbuf_next(c->left->body)) != NULL) {
        Value vals[c->ecnt];
        for (int i = 0; i < c->ecnt; ++i)
            vals[i] = expr_new_val(c->exprs[i], t, a);

        Tuple *e = tuple_new(vals, c->ecnt);
        Tuple *res = tuple_join(t, e, c->j.lpos, c->j.rpos, c->j.len);
        tbuf_add(r->body, res);

        tuple_free(e);
        tuple_free(t);
    }
}

extern Rel *rel_extend(Rel *r, char *names[], Expr *e[], int len)
{
    Rel *res = alloc(eval_extend);
    Ctxt *c = res->ctxt;
    c->left = r;
    c->ecnt = len;
    
    Type types[len];
    int map[len];
    array_sort(names, len, map);
    for (int i = 0; i < len; ++i) {
        c->exprs[i] = e[map[i]];
        types[i] = e[map[i]]->type;
    }
    Head *h = head_new(names, types, len);
    res->head = head_join(r->head, h, c->j.lpos, c->j.rpos, &c->j.len);
    mem_free(h);

    return res;
}

static void eval_sum(Rel *r, Vars *v, Arg *a)
{
    Ctxt *c = r->ctxt;
    r->body = tbuf_new();

    rel_eval(c->left, v, a);
    rel_eval(c->right, v, a);

    int scnt = c->scnt;

    TBuf *lb = c->left->body;
    index_sort(lb, c->e.lpos, c->e.len);

    Tuple *lt, *rt;
    while ((rt = tbuf_next(c->right->body)) != NULL) {
        for (int i = 0; i < scnt; ++i)
            sum_reset(c->sums[i]);

        TBuf *m = index_match(lb, rt, c->e.lpos, c->e.rpos, c->e.len);
        if (m != NULL) {
            while ((lt = tbuf_next(m)) != NULL)
                for (int i = 0; i < scnt; ++i)
                    sum_update(c->sums[i], lt);

            tbuf_free(m);
        }

        Value vals[scnt];
        for (int i = 0; i < scnt; ++i)
            vals[i] = sum_value(c->sums[i]);

        Tuple *st = tuple_new(vals, scnt);
        Tuple *res = tuple_join(rt, st, c->j.lpos, c->j.rpos, c->j.len);

        tbuf_add(r->body, res);

        tuple_free(st);
        tuple_free(rt);
    }

    tbuf_clean(lb);
}

extern Rel *rel_sum(Rel *r,
                    Rel *per,
                    char *names[],
                    Type types[],
                    Sum *sums[],
                    int len)
{
    Rel *res = alloc(eval_sum);

    Ctxt *c = res->ctxt;
    c->e.len = head_common(r->head, per->head, c->e.lpos, c->e.rpos);
    c->scnt = len;
    c->left = r;
    c->right = per;

    Type stypes[len];
    int map[len];
    array_sort(names, len, map);
    for (int i = 0; i < len; ++i) {
        c->sums[i] = sums[map[i]];
        stypes[i] = types[map[i]];
    }
    Head *h = head_new(names, stypes, len);

    res->head = head_join(per->head, h, c->j.lpos, c->j.rpos, &c->j.len);
    mem_free(h);

    return res;
}

static void eval_sum_unary(Rel *r, Vars *v, Arg *a)
{
    Ctxt *c = r->ctxt;
    r->body = tbuf_new();

    rel_eval(c->left, v, a);

    for (int i = 0; i < c->scnt; ++i)
        sum_reset(c->sums[i]);

    Tuple *t;
    while ((t = tbuf_next(c->left->body)) != NULL) {
        for (int i = 0; i < c->scnt; ++i)
            sum_update(c->sums[i], t);

        tuple_free(t);
    }

    Value vals[c->scnt];
    for (int i = 0; i < c->scnt; ++i)
        vals[i] = sum_value(c->sums[i]);

    tbuf_add(r->body, tuple_new(vals, c->scnt));
}

extern Rel *rel_sum_unary(Rel *r,
                          char *names[],
                          Type types[],
                          Sum *sums[],
                          int len)
{
    Rel *res = alloc(eval_sum_unary);

    Ctxt *c = res->ctxt;
    c->scnt = len;
    c->left = r;

    Type stypes[len];
    int map[len];
    array_sort(names, len, map);
    for (int i = 0; i < len; ++i) {
        c->sums[i] = sums[map[i]];
        stypes[i] = types[map[i]];
    }
    res->head = head_new(names, stypes, len);

    return res;
}

static void eval_store(Rel *r, Vars *v, Arg *a)
{
    Ctxt *c = r->ctxt;
    rel_eval(c->left, v, a);

    int idx = array_scan(v->names, v->len, c->name);
    if (v->vals[idx] != NULL) {
        tbuf_clean(v->vals[idx]);
        tbuf_free(v->vals[idx]);
    }
    v->vals[idx] = c->left->body;
    c->left->body = NULL;
}

extern Rel *rel_store(const char *name, Rel *r)
{
    Rel *res = alloc(eval_store);
    res->head = head_cpy(r->head);

    Ctxt *c = res->ctxt;
    c->left = r;
    str_cpy(c->name, name);

    return res;
}

extern int rel_eq(Rel *l, Rel *r)
{
    if (!head_eq(l->head, r->head))
        return 0;

    int rcnt = 0, all_ok = 1;
    int lpos[MAX_ATTRS], rpos[MAX_ATTRS];
    int len = head_common(l->head, r->head, lpos, rpos);

    index_sort(l->body, lpos, len);

    Tuple *rt;
    while (all_ok && (rt = tbuf_next(r->body)) != NULL) {
        all_ok = all_ok && index_has(l->body, rt, lpos, rpos, len);
        tuple_free(rt);
        rcnt++;
    }

    all_ok = all_ok && (l->body->len == rcnt);

    tbuf_clean(l->body);
    while ((rt = tbuf_next(r->body)) != NULL)
        tuple_free(rt);

    return all_ok;
}

static void vars_move(Vars *dest, Vars *src, char **names, int len)
{
    for (int i = 0; i < len; ++i) {
        int dest_pos = array_scan(dest->names, dest->len, names[i]);
        if (dest_pos > -1)
            continue;

        if (src != NULL) {
            int src_pos = array_scan(src->names, src->len, names[i]);
            vars_add(dest, names[i], 0, src->vals[src_pos]);
            src->vals[src_pos] = NULL;
        } else
            vars_add(dest, names[i], 0, NULL);
    }
}

static void eval_call(Rel *r, Vars *v, Arg *a)
{
    Ctxt *c = r->ctxt;

    /* prepare primitive arguments */
    Arg *na = mem_alloc(sizeof(Arg));
    for (int i = 0; i < c->ecnt; ++i) {
        Value v = expr_new_val(c->exprs[i], NULL, a);
        Type t = c->exprs[i]->type;
        if (t == Int)
            na->vals[i].v_int = val_int(v);
        else if (t == Real)
            na->vals[i].v_real = val_real(v);
        else if (t == Long)
            na->vals[i].v_long = val_long(v);
        else if (t == String)
            str_cpy(na->vals[i].v_str, val_str(v));
    }

    /* prepare variables */
    Vars *nv = vars_new(0);
    vars_move(nv, v, c->r.names, c->r.len);
    vars_move(nv, NULL, c->w.names, c->w.len);
    vars_move(nv, NULL, c->t.names, c->t.len);
    if (c->left != NULL) {
        rel_eval(c->left, v, a);
        vars_add(nv, c->name, 0, c->left->body);
    }

    /* evaluate the function body */
    for (int i = 0; i < c->slen; ++i)
        rel_eval(c->stmts[i], nv, na);

    /* copy the return value (if any) */
    if (r->head != NULL) {
        r->body = tbuf_new();

        TBuf *ret = c->stmts[c->slen - 1]->body;

        Tuple *t;
        tbuf_reset(ret);
        while ((t = tbuf_next(ret)) != NULL)
            tbuf_add(r->body, tuple_cpy(t));
        tbuf_reset(ret);
    }

    /* move the overwritten variables back to the calling function */
    for (int i = 0; i < c->w.len; ++i) {
        int vpos = array_scan(v->names, v->len, c->w.names[i]);
        int nvpos = array_scan(nv->names, nv->len, c->w.names[i]);

        if (v->vals[vpos] != NULL) {
            tbuf_clean(v->vals[vpos]);
            tbuf_free(v->vals[vpos]);
        }

        v->vals[vpos] = nv->vals[nvpos];
        nv->vals[nvpos] = NULL;
    }

    /* garbage collect */
    for (int i = 0; i < c->t.len; ++i) {
        int pos = array_scan(nv->names, nv->len, c->t.names[i]);
        tbuf_clean(nv->vals[pos]);
        tbuf_free(nv->vals[pos]);
    }
    vars_free(nv);
    mem_free(na);
}

extern Rel *rel_call(char **r, int rlen,
                     char **w, int wlen,
                     char **t, int tlen,
                     Rel **stmts, int slen,
                     Expr **pexprs, int plen,
                     Rel *rexpr, char *rname,
                     Head *ret)
{
    Rel *res = alloc(eval_call);
    res->head = ret == NULL ? NULL : head_cpy(ret);

    Ctxt *c = res->ctxt;
    c->left = rexpr;
    c->r.len = rlen;
    c->w.len = wlen;
    c->t.len = tlen;
    c->ecnt = plen;
    c->slen = slen;
    if (rname != NULL)
        str_cpy(c->name, rname);
    for (int i = 0; i < rlen; ++i)
        c->r.names[i] = r[i];
    for (int i = 0; i < wlen; ++i)
        c->w.names[i] = w[i];
    for (int i = 0; i < tlen; ++i)
        c->t.names[i] = t[i];
    for (int i = 0; i < plen; ++i)
        c->exprs[i] = pexprs[i];
    for (int i = 0; i < slen; ++i)
        c->stmts[i] = stmts[i];

    return res;
}
