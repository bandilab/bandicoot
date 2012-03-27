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

%{
#include "config.h"
#include "string.h"
#include "system.h"
#include "memory.h"

extern void conv_error(const char *, ...);
extern int conv_lex();
extern char *cat(char *fmt, ...);
%}

%union {
    char *val;
};

%token <val> TK_NAME
%token <val> TK_INT_VAL TK_LONG_VAL TK_REAL_VAL TK_STRING_VAL
%token <val> TK_REL TK_FN TK_RETURN
%token <val> TK_INT TK_LONG TK_REAL TK_STRING
%token <val> TK_PROJECT TK_RENAME TK_SELECT TK_EXTEND TK_SUMMARY
%token <val> TK_EQ TK_NEQ TK_AND TK_OR TK_LTE TK_GTE
%token <val> '-' '+' '*' '/' '<' '>' '{' '}' '(' ')' ']' '[' ':' ';' ',' '!' '='

%type <val> rel_decl relvar_decl func_decl
%type <val> func_name func_res func_params func_param func_body
%type <val> stmt_return stmt_assigns stmt_assign
%type <val> rel_attr rel_attrs rel_attrs_names
%type <val> project_attr project_attrs
%type <val> rename_attr rename_attrs
%type <val> extend_attr extend_attrs
%type <val> sum_attrs sum_attr
%type <val> rel_post_expr rel_prim_expr rel_mul_expr rel_expr
%type <val> prim_const_expr prim_simple_expr prim_top_expr prim_unary_expr
%type <val> prim_mul_expr prim_add_expr prim_bool_cmp_expr prim_expr
%type <val> sum_func
%type <val> rel_head

%%

program:
      program rel_decl      { sys_print("%s\n\n", $2); mem_free($2); }
    | program relvar_decl   { sys_print("%s\n\n", $2); mem_free($2); }
    | program func_decl     { sys_print("%s\n\n", $2); mem_free($2); }
    |
    ;

rel_decl:
      TK_REL TK_NAME rel_head   { $$ = cat("type %s %s", $2, $3); }
    ;

rel_head:
      '{' rel_attrs '}'         { $$ = cat("%s%s%s", $1, $2, $3); }
    | '{' rel_attrs ',' '}'     { $$ = cat("%s%s%s", $1, $2, $4); }
    ;

rel_attrs:
      rel_attr                  { $$ = cat("%s", $1); }
    | rel_attrs ',' rel_attr    { $$ = cat("%s%s %s", $1, $2, $3); }
    ;

rel_attrs_names:
      TK_NAME                       { $$ = cat("%s", $1); }
    | rel_attrs_names ',' TK_NAME   { $$ = cat("%s %s", $1, $3); }
    ;

rel_attr:
      rel_attrs_names ':' TK_INT    { $$ = cat("%s %s", $1, $3); }
    | rel_attrs_names ':' TK_LONG   { $$ = cat("%s %s", $1, $3); }
    | rel_attrs_names ':' TK_REAL   { $$ = cat("%s %s", $1, $3); }
    | rel_attrs_names ':' TK_STRING { $$ = cat("%s %s", $1, $3); }
    ;

relvar_decl:
      TK_NAME ':' TK_NAME ';'           { $$ = cat("var %s %s%s", $1, $3, $4); }
    | TK_NAME ':' TK_REL rel_head ';'   { $$ = cat("var %s %s%s", $1, $4, $5); }
    ;

func_decl:
      TK_FN func_name '(' func_params ')' func_res '{' func_body '}'
      { $$ = cat("%s %s%s%s%s %s\n%s%s%s", $1, $2, $3, $4, $5, $6, $7, $8, $9); }
    ;

func_name:
      TK_NAME   { $$ = cat("%s", $1); }
    ;

func_res:
      ':' TK_NAME           { $$ = cat("%s", $2); }
    | ':' TK_REL rel_head   { $$ = cat("%s", $3); }
    |                       { $$ = cat("void"); }
    ;

func_params:
      func_param                    { $$ = cat("%s", $1); }
    | func_params ',' func_param    { $$ = cat("%s%s %s", $1, $2, $3); }
    |                               { $$ = cat(""); }
    ;

func_param:
      rel_attr                              { $$ = cat("%s", $1); }
    | rel_attrs_names ':' TK_NAME           { $$ = cat("%s %s", $1, $3); }
    | rel_attrs_names ':' TK_REL rel_head   { $$ = cat("%s %s", $1, $4); }
    ;

func_body:
      stmt_return               { $$ = cat("\n%s\n", $1); }
    | stmt_assigns              { $$ = cat("\n%s\n", $1); }
    | stmt_assigns stmt_return  { $$ = cat("\n%s\n\n%s\n", $1, $2); }
    |                           { $$ = cat(""); }
    ;

stmt_return:
      TK_RETURN rel_expr ';'    { $$ = cat("\t%s %s%s", $1, $2, $3); }
    ;

stmt_assigns:
      stmt_assign               { $$ = cat("%s", $1); }
    | stmt_assigns stmt_assign  { $$ = cat("%s\n%s", $1, $2); }
    ;

stmt_assign:
      TK_NAME '=' rel_expr ';'
      { $$ = cat("\t%s %s %s%s", $1, $2, $3, $4); }
    | TK_NAME '+' '=' rel_expr ';'
      { $$ = cat("\t%s %s%s %s%s", $1, $2, $3, $4, $5); }
    | TK_NAME '-' '=' rel_expr ';'
      { $$ = cat("\t%s %s%s %s%s", $1, $2, $3, $4, $5); }
    | TK_NAME '*' '=' rel_expr ';'
      { $$ = cat("\t%s %s%s %s%s", $1, $2, $3, $4, $5); }
    | TK_NAME ':' '=' rel_expr ';'
      { $$ = cat("\tvar %s %s %s%s", $1, $3, $4, $5); }
    ;

rel_prim_expr:
      TK_NAME           { $$ = cat("%s", $1); }
    | '(' rel_expr ')'  { $$ = cat("%s", $2); }
    ;

rel_post_expr:
      rel_prim_expr
      { $$ = cat("%s", $1); }
    | rel_post_expr TK_PROJECT '(' project_attrs ')'
      { $$ = cat("(%s %s %s)", $2, $4, $1); }
    | rel_post_expr TK_RENAME '(' rename_attrs ')'
      { $$ = cat("(%s %s %s)", $2, $4, $1); }
    | rel_post_expr TK_SELECT '(' prim_expr ')'
      { $$ = cat("(%s %s %s)", $2, $4, $1); }
    | rel_post_expr TK_EXTEND '(' extend_attrs ')'
      { $$ = cat("(%s %s %s)", $2, $4, $1); }
    | rel_post_expr TK_SUMMARY '(' sum_attrs ')'
      { $$ = cat("(%s %s %s)", $2, $4, $1); }
    | '(' rel_post_expr ',' rel_post_expr ')' TK_SUMMARY '(' sum_attrs ')'
      { $$ = cat("(%s %s %s %s)", $6, $8, $2, $4); }
    ;

rel_mul_expr:
      rel_post_expr                     { $$ = cat("%s", $1); }
    | rel_mul_expr '*' rel_post_expr    { $$ = cat("(join %s %s)", $1, $3); }
    ;

rel_expr:
      rel_mul_expr                      { $$ = cat("%s", $1); }
    | rel_expr '+' rel_mul_expr         { $$ = cat("(union %s %s)", $1, $3); }
    | rel_expr '-' rel_mul_expr         { $$ = cat("(minus %s %s)", $1, $3); }
    ;

project_attrs:
      project_attr                      { $$ = cat("%s", $1); }
    | project_attrs ',' project_attr    { $$ = cat("%s%s %s", $1, $2, $3); }
    ;

project_attr:
      TK_NAME                           { $$ = cat("%s", $1); }
    ;

rename_attrs:
      rename_attr                       { $$ = cat("%s", $1); }
    | rename_attrs ',' rename_attr      { $$ = cat("%s%s %s", $1, $2, $3); }
    ;

rename_attr:
      TK_NAME '=' TK_NAME               { $$ = cat("%s %s %s", $1, $2, $3); }
    ;

extend_attrs:
      extend_attr                       { $$ = cat("%s", $1); }
    | extend_attrs ',' extend_attr      { $$ = cat("%s%s %s", $1, $2, $3); }
    ;

extend_attr:
      TK_NAME '=' prim_expr             { $$ = cat("%s %s %s", $1, $2, $3); }
    ;

sum_attrs:
      sum_attr                          { $$ = cat("%s", $1); }
    | sum_attrs ',' sum_attr            { $$ = cat("%s%s %s", $1, $2, $3); }
    ;

sum_attr:
      TK_NAME '=' sum_func              { $$ = cat("%s %s %s", $1, $2, $3); }
    ;

sum_func:
      TK_NAME '(' ')'
      { $$ = cat("%s", $1); }
    | TK_NAME '(' TK_NAME ',' prim_expr ')'
      { $$ = cat("%s%s %s %s%s", $2, $1, $3, $5, $6); }
    ;

prim_const_expr:
      TK_INT_VAL                        { $$ = cat("%s", $1); }
    | TK_LONG_VAL                       { $$ = cat("%s", $1); }
    | TK_REAL_VAL                       { $$ = cat("%s", $1); }
    | TK_STRING_VAL                     { $$ = cat("%s", $1); }
    ;

prim_simple_expr:
      prim_const_expr
      { $$ = cat("%s", $1); }
    | '(' prim_expr ')'
      { $$ = cat("%s%s%s", $1, $2, $3); }
    | TK_INT '(' prim_expr ')'
      { $$ = cat("%s%s %s%s", $2, $1, $3, $4); }
    | TK_REAL '(' prim_expr ')'
      { $$ = cat("%s%s %s%s", $2, $1, $3, $4); }
    | TK_LONG '(' prim_expr ')'
      { $$ = cat("%s%s %s%s", $2, $1, $3, $4); }
    | TK_NAME '(' prim_expr ')'
      { $$ = cat("%s%s %s%s", $2, $1, $3, $4); }
    ;

prim_top_expr:
      prim_simple_expr                 { $$ = cat("%s", $1); }
    | TK_NAME                          { $$ = cat("%s", $1); }
    ;

prim_unary_expr:
      prim_top_expr                    { $$ = cat("%s", $1); }
    | '!' prim_unary_expr              { $$ = cat("%s%s", $1, $2); }
    | '-' prim_simple_expr             { $$ = cat("%s%s", $1, $2); }
    | '+' prim_simple_expr             { $$ = cat("%s%s", $1, $2); }
    ;

prim_mul_expr:
      prim_unary_expr                   { $$ = cat("%s", $1); }
    | prim_mul_expr '*' prim_unary_expr { $$ = cat("%s %s %s", $1, $2, $3); }
    | prim_mul_expr '/' prim_unary_expr { $$ = cat("%s %s %s", $1, $2, $3); }
    ;

prim_add_expr:
      prim_mul_expr                     { $$ = cat("%s", $1); }
    | prim_add_expr '+' prim_mul_expr   { $$ = cat("%s %s %s", $1, $2, $3); }
    | prim_add_expr '-' prim_mul_expr   { $$ = cat("%s %s %s", $1, $2, $3); }
    ;

prim_bool_cmp_expr:
      prim_add_expr
      { $$ = cat("%s", $1); }
    | prim_bool_cmp_expr TK_EQ prim_add_expr
      { $$ = cat("%s %s %s", $1, $2, $3); }
    | prim_bool_cmp_expr TK_NEQ prim_add_expr
      { $$ = cat("%s %s %s", $1, $2, $3); }
    | prim_bool_cmp_expr '>' prim_add_expr
      { $$ = cat("%s %s %s", $1, $2, $3); }
    | prim_bool_cmp_expr '<' prim_add_expr
      { $$ = cat("%s %s %s", $1, $2, $3); }
    | prim_bool_cmp_expr TK_LTE prim_add_expr
      { $$ = cat("%s %s %s", $1, $2, $3); }
    | prim_bool_cmp_expr TK_GTE prim_add_expr
      { $$ = cat("%s %s %s", $1, $2, $3); }
    ;

prim_expr:
      prim_bool_cmp_expr
      { $$ = cat("%s", $1); }
    | prim_expr TK_AND prim_bool_cmp_expr
      { $$ = cat("%s %s %s", $1, $2, $3); }
    | prim_expr TK_OR prim_bool_cmp_expr
      { $$ = cat("%s %s %s", $1, $2, $3); }
    ;

%%
