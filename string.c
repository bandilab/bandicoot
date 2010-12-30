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

#include "memory.h"
#include "number.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

extern int str_len(const char *s)
{
    return strlen(s);
}

extern int str_cmp(const char *l, const char *r)
{
    return strcmp(l, r);
}

extern int str_cpy(char *dest, const char *src)
{
    int res = 0;
    while ((*dest++ = *src++))
        ++res;

    return res;
}

extern char* str_dup(const char *src) {
    char *res = mem_alloc(str_len(src) + 1);
    str_cpy(res, src);

    return res;
}

extern int str_print(char *res, const char *msg, ...)
{
    va_list ap;
    int chars;

    va_start(ap, msg);
    chars = vsprintf(res, msg, ap);
    va_end(ap);

    return chars;
}

extern int str_idx(const char *s, const char *seq)
{
    char *pos = strstr(s, seq);
    return (pos == 0) ? -1 : pos - s;
}

static int is_space(char ch)
{
    return ch == '\r' || ch == '\n' || ch == ' ' || ch == '\t';
}

static int is_digit(char ch)
{
    return ch >= '0' && ch <= '9';
}

static const char *get_sign(const char *s, int *sign, int *error)
{
    *error = 0;
    *sign = 1;

    if (*s == '-') {
        *sign = -1;
        ++s;
    } else if (*s == '+') {
        *sign = +1;
        ++s;
    } else if (!is_digit(*s) && *s != '.')
        *error = 1;

    return s;
}

extern unsigned long long str_ulong(const char *s, int *error)
{
    *error = 0;

    unsigned long long val;

    for (val = 0ULL; *s && !*error; ++s)
        if (is_digit(*s))
            val = ulong_add(ulong_mul(10ULL, val, error), *s - '0', error);
        else
            *error = 1;

    return val;
}

extern unsigned int str_uint(const char *s, int *error)
{
    unsigned long long val = str_ulong(s, error);
    if (val > MAX_UINT)
        *error = 1;

    return val;
}

extern long long str_long(const char *s, int *error)
{
    int sign, uerror = 0;
    s = get_sign(s, &sign, error);

    unsigned long long val = str_ulong(s, &uerror);
    *error = *error || uerror || long_overflow(sign, val);

    return sign * val;
}

extern int str_int(const char *s, int *error)
{
    int sign, uerror = 0;
    s = get_sign(s, &sign, error);

    unsigned long long val = str_ulong(s, &uerror);
    *error = *error || uerror || (val > MAX_UINT) || int_overflow(sign, val);
    
    return sign * val;
}

/* FIXME: we need to check the logic, test with DBL_MAX fails */
extern double str_real(const char *s, int *error)
{
    *error = 0;

    int sign;
    s = get_sign(s, &sign, error);

    double val;
    for (val = 0; *s && *s != '.' && !*error; ++s)
        if (is_digit(*s))
            val = 10 * val + (*s - '0');
        else
            *error = 1;

    if (*s == '.' && !*error) {
        if (*(++s) != '\0') {
            double fract, div;
            for (fract = 0, div = 10; *s && !*error; ++s, div *= 10)
                if (is_digit(*s))
                    fract = fract + (*s - '0') / div;
                else
                    *error = 1;
            val += fract;
        } else
            *error = 1;
    }

    return sign * val;
}

extern char *str_trim(char *s)
{
    /* TODO: get rid of str_len */
    int len = str_len(s), left = 0, right = len - 1;

    for (; left < len && is_space(s[left]); ++left);
    for (; right > -1 && is_space(s[right]); --right);
    s[++right] = '\0';

    return s + left;
}

extern char **str_split(char *buf, char delim, int *parts)
{
    /* TODO: get rid of str_len */
    int len = str_len(buf) + 1;
    char **res = 0;
    char esc = '\\', prev = ' ';
    *parts = 0;

    for (int i = 0, start = 0; i < len; ++i) {
        if ((buf[i] == delim && (prev != esc)) || (i + 1) == len) {
            *parts += 1;
            res = mem_realloc(res, *parts * sizeof(char*));
            res[*parts - 1] = buf + start;
            buf[i] = '\0';
            start = i + 1;
        }

        prev = (prev == esc) ? ' ' : buf[i];
    }

    return res;
}
