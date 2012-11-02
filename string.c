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

#include "memory.h"
#include "number.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>

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

extern long long str_len(const char *s)
{
    return strlen(s);
}

extern int str_cmp(const char *l, const char *r)
{
    return strcmp(l, r);
}

extern long long str_cpy(char *dest, const char *src)
{
    long long res = 0;
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

    va_start(ap, msg);
    int chars = vsprintf(res, msg, ap);
    va_end(ap);

    return chars;
}

extern long long str_idx(const char *s, const char *seq)
{
    char *pos = strstr(s, seq);
    return (pos == NULL) ? -1 : pos - s;
}

extern int str_match(const char *s1, const char *s2, char until)
{
    long long i = 0;
    for (; s1[i] != '\0' && s1[i] != until; ++i)
        ;

    long long j = 0;
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
    long long len = str_len(s), left = 0, right = len - 1;

    for (; left < len && is_space(s[left]); ++left);
    for (; right > -1 && is_space(s[right]); --right);
    s[++right] = '\0';

    return s + left;
}

static int is_delim(char c, const char *delims)
{
    while (*delims != '\0')
        if (c == *delims)
            return 1;
        else
            delims++;

    return 0;
}

static long _str_split(char *s, const char *delims, char ***out, int max)
{
    /* TODO: get rid of str_len */
    long long len = str_len(s) + 1;
    char esc = '\\', prev = '\0';
    int parts = 0;
    int size = max;

    char **res = *out;
    for (long long i = 0, start = 0; i < len; ++i) {
        if ((is_delim(s[i], delims) && prev != esc) || (i + 1) == len) {
            parts++;
            if (max == 0 && parts > size) {
                size = size == 0 ? 2 : size * 2;
                res = mem_realloc(res, size * sizeof(char*));
            } else if (max != 0 && parts > max) {
                parts = -1;
                break;
            }

            res[parts - 1] = s + start;
            s[i] = '\0';
            start = i + 1;
        }

        prev = (prev == esc) ? '\0' : s[i];
    }
    *out = res;

    return parts;
}

extern char **str_split_big(char *s, const char *delims, int *parts)
{
    char **res = NULL;
    *parts = _str_split(s, delims, &res, 0);
    return res;
}

extern int str_split(char *s, const char *delims, char *out[], int max)
{
    return _str_split(s, delims, &out, max);
}

extern long long str_to_sid(char *str)
{
    int i = 0, shift = 60;
    long long sid = 0;
    while (shift >= 0) {
        sid = sid | (STR_TO_SID[(int) str[i++]] & 0x0F) << shift;
        shift -= 4;
    }

    return sid;
}

extern int str_from_sid(char *dest, long long sid)
{
    int i = 0, shift = 60;
    while (shift >= 0) {
        dest[i++] = SID_TO_STR[(sid >> shift) & 0x0F];
        shift -= 4;
    }

    dest[i] = '\0';

    return i;
}

extern char *str_urldecode(char *src)
{
    char hex[5];
    str_cpy(hex, "0x00");
    int off = 0, read = 0, idx = -1, len = str_len(src);
    char *res = mem_alloc(len + 1);

    for (int i = 0; i < len; ++i) {
        char c = src[i];
        if (c == '%') {
            if (i + 2 >= len)
                goto failure;
            hex[2] = src[++i];
            hex[3] = src[++i];
            c = strtol(hex, NULL, 16);
            if (c == '\0')
                goto failure;
        }
        res[off++] = c;
    }

    res[off] = '\0';
    goto success;

failure:
    mem_free(res);
    res = NULL;
success:
    return res;
}

extern char *str_urlencode(char *src)
{
    char *res = NULL;

    int off = 0, len = strlen(src);
    char *tmp = mem_alloc(3 * len + 1);
    char hex[3];

    for (int i = 0; i < len; ++i) {
        hex[1] = 0;
        int v = (int) src[i];
        if (v < 0)
            goto exit;

        str_print(hex, "%X", v);
        if (hex[1] == 0) {
            hex[1] = hex[0];
            hex[0] = '0';
        }
        off += str_print(tmp + off, "%%%s", hex);
    }

    tmp[off] = '\0';
    res = tmp;
    tmp = NULL;

exit:
    if (tmp != NULL)
        mem_free(tmp);

    return res;
}

extern int str_hexdecode(char *str, int *error)
{
    char *end = NULL;
    int res = strtol(str, &end, 16);
    *error = (*str == '\0' || *end != '\0');

    return res;
}
