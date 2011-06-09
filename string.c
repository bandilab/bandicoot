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

static char SID_TO_STR[] = {
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
static char STR_TO_SID[127];

extern void str_init()
{
    for (int i = '0'; i <= '9'; ++i)
        STR_TO_SID[i] = i - '0';
    for (int i = 'A'; i <= 'F'; ++i)
        STR_TO_SID[i] = i - 'A' + 10;
    for (int i = 'a'; i <= 'f'; ++i)
        STR_TO_SID[i] = i - 'a' + 10;
}

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
    return (pos == NULL) ? -1 : pos - s;
}

extern int str_match(const char *s1, const char *s2, char until)
{
    int i = 0;
    for (; s1[i] != '\0' && s1[i] != until; ++i)
        ;

    int j = 0;
    for (; j <= i && s2[j] != '\0' && s2[j] != until && s1[j] == s2[j]; ++j)
        ;

    return i == j && s1[i] == s2[j];
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

extern double str_real(const char *s, int *error)
{
    *error = 0;

    int sign;
    s = get_sign(s, &sign, error);

    register long double val;
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

    return (double) sign * val;
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

extern long str_to_sid(char *str)
{
#ifdef LP64
    int i = 0, shift = 60;
#else
    int i = 8, shift = 28;
#endif

    long sid = 0;
    while (shift >= 0) {
        sid = sid | (STR_TO_SID[(int) str[i++]] & 0x0F) << shift;
        shift -= 4;
    }

    return sid;
}

extern int str_from_sid(char *dest, long sid)
{
#ifdef LP64
    int i = 0, shift = 60;
#else
    int i = 0, shift = 28;

    while (i < 8)
        dest[i++] = SID_TO_STR[0];
#endif

    while (shift >= 0) {
        dest[i++] = SID_TO_STR[(sid >> shift) & 0x0F];
        shift -= 4;
    }

    dest[i] = '\0';

    return i;
}

