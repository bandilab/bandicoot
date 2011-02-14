/*
Copyright 2008-2010 Ostap Cherkashin
Copyright 2008-2010 Julius Chrobak

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

static const char *src_path = "";
static int line = 1;
%}

%%

"int"               { return TK_INT; }
"long"              { return TK_LONG; }
"real"              { return TK_REAL; }
"string"            { return TK_STRING; }
"rel"               { return TK_REL; }
"fn"                { return TK_FN; }
"return"            { return TK_RETURN; }
"project"           { return TK_PROJECT; }
"rename"            { return TK_RENAME; }
"select"            { return TK_SELECT; }
"extend"            { return TK_EXTEND; }
"summary"           { return TK_SUMMARY; }

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
">"                 { return '>'; }
"{"                 { return '{'; }
"}"                 { return '}'; }
"("                 { return '('; }
")"                 { return ')'; }
"]"                 { return ']'; }
"["                 { return '['; }
":"                 { return ':'; }
";"                 { return ';'; }
","                 { return ','; }
"!"                 { return '!'; }
"="                 { return '='; }

[ \t\r\n]           { if (*yytext == '\n') line++; }
"#".*\n             { line++; }
.                   { yyerror("invalid character '%c'", *yytext); }

%%

extern int yywrap()
{
    return 1;
}

extern Env *env_new(const char *file)
{
    yyin = fopen(file, "r");
    if (yyin == 0)
        sys_die("sys: cannot open source file '%s'\n", file);

    src_path = file;

    Env *env = grammar_init();
    yyparse();
    fclose(yyin);

    /* FIXME: yylex_destroy(); */

    return env;
}

extern void yyerror(const char *msg, ...)
{
    char m[MAX_FILE_PATH + str_len(msg)];
    str_print(m, "%s:%d: %s\n", src_path, line, msg);

    va_list ap;

    va_start(ap, msg);
    vfprintf(stderr, m, ap);
    fflush(stderr);
    va_end(ap);

    sys_exit(PROC_FAIL);
}
