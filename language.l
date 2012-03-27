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
/* TODO: get rid of [f]lex */
#include <stdarg.h>

#include "config.h"
#include "memory.h"
#include "system.h"
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

#include "y.tab.h"

extern Env *grammar_init();
extern void yyerror(const char*, ...);
extern void yyparse();

static struct {
    const char *path;
    const char *buf;
    int pos;
    int len;
    int line;
} gsrc;

static int feed_lex(void *buf, int max_size)
{
    int avail = gsrc.len - gsrc.pos;
    if (avail <= 0)
        return YY_NULL;

    int size = (avail > max_size) ? max_size : avail;
    mem_cpy(buf, gsrc.buf + gsrc.pos, size);
    gsrc.pos += size;

    return size;
}

#define YY_INPUT(buf, result, max_size) \
    do { result = feed_lex(buf, max_size); } while (0)

%}

%%

"int"               { return TK_INT; }
"long"              { return TK_LONG; }
"real"              { return TK_REAL; }
"string"            { return TK_STRING; }
"void"              { return TK_VOID; }
"fn"                { return TK_FN; }
"var"               { return TK_VAR; }
"type"              { return TK_TYPE; }
"return"            { return TK_RETURN; }
"project"           { return TK_PROJECT; }
"rename"            { return TK_RENAME; }
"select"            { return TK_SELECT; }
"extend"            { return TK_EXTEND; }
"summary"           { return TK_SUMMARY; }
"join"              { return TK_JOIN; }
"minus"             { return TK_MINUS; }
"union"             { return TK_UNION; }
"time"              { return TK_TIME; }

                    /* TODO: hex & octal numbers? */
[0-9]+              {
                      int error;
                      yylval.val.v_int = str_uint(yytext, &error); 
                      if (error)
                          yyerror("bad integer literal '%s'", yytext);

                      return TK_INT_VAL;
                    }
[0-9]+[lL]          {
                      int error;
                      yytext[yyleng - 1] = '\0';
                      yylval.val.v_long = str_ulong(yytext, &error);
                      if (error)
                          yyerror("bad long literal '%s'", yytext);

                      return TK_LONG_VAL;
                    }
[0-9]*\.[0-9]+      {
                      int error;
                      yylval.val.v_real = str_real(yytext, &error);
                      if (error)
                          yyerror("bad real literal '%s'", yytext);

                      return TK_REAL_VAL;
                    }
\"(\\.|[^\\"])*\"   {
                      if (yyleng >= MAX_STRING)
                          yyerror("string literal exceeds max %d", MAX_STRING);

                      yytext[yyleng - 1] = '\0';
                      str_cpy(yylval.val.v_str, yytext + 1);
                      return TK_STRING_VAL;
                    }
[_a-zA-Z0-9]+       {
                      if (yyleng >= MAX_NAME)
                          yyerror("identifier '%s' is too long", yytext);

                      str_cpy(yylval.name, yytext);
                      return TK_NAME;
                    }

"=="                { return TK_EQ; }
"!="                { return TK_NEQ; }
"&&"                { return TK_AND; }
"||"                { return TK_OR; }
"-"                 { return '-'; }
"+"                 { return '+'; }
"*"                 { return '*'; }
"/"                 { return '/'; }
"<"                 { return '<'; }
"<="                { return TK_LTE; }
">"                 { return '>'; }
">="                { return TK_GTE; }
"{"                 { return '{'; }
"}"                 { return '}'; }
"("                 { return '('; }
")"                 { return ')'; }
"]"                 { return ']'; }
"["                 { return '['; }
";"                 { return ';'; }
","                 { return ','; }
"!"                 { return '!'; }
"="                 { return '='; }

[ \t\r\n]           { if (*yytext == '\n') gsrc.line++; }
"#".*\n             { gsrc.line++; }
.                   { yyerror("invalid character '%c'", *yytext); }

%%

extern int yywrap()
{
    return 1;
}

extern Env *env_new(const char *path, const char *program)
{
    gsrc.path = path;
    gsrc.buf = program;
    gsrc.len = str_len(program);
    gsrc.pos = 0;
    gsrc.line = 1;

    Env *env = grammar_init();
    yyparse();

    /* FIXME: yylex_destroy(); */

    return env;
}

extern void yyerror(const char *msg, ...)
{
    char m[MAX_FILE_PATH + str_len(msg)];
    str_print(m, "%s:%d: %s\n", gsrc.path, gsrc.line, msg);

    va_list ap;

    va_start(ap, msg);
    vfprintf(stderr, m, ap);
    fflush(stderr);
    va_end(ap);

    sys_exit(PROC_FAIL);
}
