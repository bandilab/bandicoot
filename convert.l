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
#include <stdarg.h>
#include "config.h"
#include "system.h"
#include "memory.h"
#include "string.h"
#include "convert.h"

YYSTYPE conv_lval;

extern char *cat(const char *fmt, ...);
extern void conv_error(const char *msg, ...);
static int line;

#define ret(t) do { if (str_cmp("type", conv_text) == 0 || \
                        str_cmp("var", conv_text) == 0 || \
                        str_cmp("union", conv_text) == 0 || \
                        str_cmp("minus", conv_text) == 0 || \
                        str_cmp("time", conv_text) == 0 || \
                        str_cmp("join", conv_text) == 0) \
                        conv_lval.val = cat("___%s", str_dup(conv_text)); \
                    else \
                        conv_lval.val = str_dup(conv_text); \
                    return (t); } while (0)
%}

%%

"int"               { ret(TK_INT); }
"long"              { ret(TK_LONG); }
"real"              { ret(TK_REAL); }
"string"            { ret(TK_STRING); }
"rel"               { ret(TK_REL); }
"fn"                { ret(TK_FN); }
"return"            { ret(TK_RETURN); }
"project"           { ret(TK_PROJECT); }
"rename"            { ret(TK_RENAME); }
"select"            { ret(TK_SELECT); }
"extend"            { ret(TK_EXTEND); }
"summary"           { ret(TK_SUMMARY); }

[0-9]+              { ret(TK_INT_VAL); }
[0-9]+[lL]          { ret(TK_LONG_VAL); }
[0-9]*\.[0-9]+      { ret(TK_REAL_VAL); }
\"(\\.|[^\\"])*\"   { ret(TK_STRING_VAL); }
[_a-zA-Z0-9]+       { ret(TK_NAME); }

"=="                { ret(TK_EQ); }
"!="                { ret(TK_NEQ); }
"&&"                { ret(TK_AND); }
"||"                { ret(TK_OR); }
"-"                 { ret('-'); }
"+"                 { ret('+'); }
"*"                 { ret('*'); }
"/"                 { ret('/'); }
"<"                 { ret('<'); }
"<="                { ret(TK_LTE); }
">"                 { ret('>'); }
">="                { ret(TK_GTE); }
"{"                 { ret('{'); }
"}"                 { ret('}'); }
"("                 { ret('('); }
")"                 { ret(')'); }
"]"                 { ret(']'); }
"["                 { ret('['); }
":"                 { ret(':'); }
";"                 { ret(';'); }
","                 { ret(','); }
"!"                 { ret('!'); }
"="                 { ret('='); }

[ \t\r\n]           { if (*yytext == '\n') line++; }
"#".*\n             { line++; }
.                   { conv_error("invalid character '%c'", *yytext); }

%%

extern int conv_wrap()
{
    return 1;
}

extern void conv_error(const char *msg, ...)
{
    char m[MAX_FILE_PATH + str_len(msg)];
    str_print(m, "stdin:%d: %s\n", line, msg);

    va_list ap;

    va_start(ap, msg);
    vfprintf(stderr, m, ap);
    fflush(stderr);
    va_end(ap);

    sys_exit(PROC_FAIL);
}

extern char *cat(const char *fmt, ...)
{
    va_list ap1;
    va_list ap2;
    va_list ap3;

    va_start(ap1, fmt);
    va_copy(ap2, ap1);
    va_copy(ap3, ap1);

    int len = 0;
    for (int i = 0; i < str_len(fmt); ++i)
        if (fmt[i] == '%')
            len += str_len(va_arg(ap1, char*));

    char *res = mem_alloc(len + MAX_STRING);
    vsprintf(res, fmt, ap2);

    for (int i = 0; i < str_len(fmt); ++i)
        if (fmt[i] == '%')
            mem_free(va_arg(ap3, void*));

    va_end(ap1);
    va_end(ap2);
    va_end(ap3);

    return res;
}
