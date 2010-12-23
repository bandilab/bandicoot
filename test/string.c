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

#include "common.h"

static void test_cpy()
{
    int i;
    char dest[16];

    for (i = 0; i < 16; ++i)
        dest[i] = '\0';

    if (str_cpy(dest, "hello") != 5)
        fail();

    if (str_cmp("hello", dest) != 0)
        fail();

    if (str_cpy(dest, "he") != 2)
        fail();

    if (str_cmp("he", dest) != 0)
        fail();
}

static void test_int()
{
    int error;
    int i = str_int("+12345", &error);
    if (error || i != 12345)
        fail();

    i = str_int("123456789", &error);
    if (error || i != 123456789)
        fail();

    i = str_int("7", &error);
    if (error || i != 7)
        fail();

    i = str_int("-7", &error);
    if (error || i != -7)
        fail();

    i = str_int("0", &error);
    if (error || i != 0)
        fail();

    i = str_int("32asdfa32", &error);
    if (!error)
        fail();

    i = str_int("", &error);
    if (!error)
        fail();

    i = str_int(".", &error);
    if (!error)
        fail();

    i = str_int(" ", &error);
    if (!error)
        fail();

    i = str_int("123123123123", &error);
    if (!error)
        fail();

    i = str_int("-123123123123", &error);
    if (!error)
        fail();
}

static void test_long()
{
    int error;
    long long res = str_long("12345", &error);
    if (error || res != 12345)
        fail();

    res = str_long("+9223372036854775807", &error);
    if (error || res != 9223372036854775807LL)
        fail();

    res = str_long("7", &error);
    if (error || res != 7LL)
        fail();

    res = str_long("-7", &error);
    if (error || res != -7LL)
        fail();

    res = str_long("0", &error);
    if (error || res != 0LL)
        fail();

    res = str_long("32asdfa32", &error);
    if (!error)
        fail();

    res = str_long("", &error);
    if (!error)
        fail();

    res = str_long(".", &error);
    if (!error)
        fail();

    res = str_long(" ", &error);
    if (!error)
        fail();

    res = str_long("9223372036854775808", &error);
    if (!error)
        fail();

    res = str_long("-9223372036854775809", &error);
    if (!error)
        fail();
}

static void test_real()
{
    int error;
    double d = str_real("+12.34", &error);
    if (error || d != 12.34)
        fail();

    d = str_real("-12.34", &error);
    if (error || d != -12.34)
        fail();

    d = str_real(".3", &error);
    if (error || d != .3)
        fail();

    d = str_real("12345", &error);
    if (error || d != 12345)
        fail();

    d = str_real("", &error);
    if (!error)
        fail();

    d = str_real(".", &error);
    if (!error)
        fail();

    d = str_real("0.a", &error);
    if (!error)
        fail();

    d = str_real("32aa.cbd", &error);
    if (!error)
        fail();
}

static void test_idx()
{
    if (-1 != str_idx("does not contain", "does not exist"))
        fail();

    if (0 != str_idx("a", "a"))
        fail();

    if (5 != str_idx("a b c d\r\n", " d\r\n"))
        fail();

    if (19 != str_idx("POST /some_service HTTP/1.1\r\n", "HTTP/1.1"))
        fail();
}

static void test_trim()
{
    char s[64];
    str_cpy(s, " \r  \n \t  abc\t\t\t\r\r");
    char *res = str_trim(s);
    if (str_len(res) != 3)
        fail();

    if (str_cmp("abc", res) != 0)
        fail();
}

static void test_split()
{
    int cnt;
    char str[64];

    str_cpy(str, "a`b`");
    char **s = str_split(str, '`', &cnt);

    if (cnt != 3)
        fail();
    if (str_cmp("a", s[0]) != 0)
        fail();
    if (str_cmp("b", s[1]) != 0)
        fail();
    if (str_cmp("", s[2]) != 0)
        fail();

    mem_free(s);
    str_cpy(str, "");
    s = str_split(str, '=', &cnt);

    if (cnt != 1)
        fail();
    if (str_cmp("", s[0]) != 0)
        fail();

    mem_free(s);
    str_cpy(str, "c:string,a:int");
    s = str_split(str, ',', &cnt);

    if (cnt != 2)
        fail();
    if (str_cmp("c:string", s[0]) != 0)
        fail();
    if (str_cmp("a:int", s[1]) != 0)
        fail();

    mem_free(s);
    str_cpy(str, "hello world,123430");
    s = str_split(str, ',', &cnt);

    if (cnt != 2)
        fail();
    if (str_cmp("hello world", s[0]) != 0)
        fail();
    if (str_cmp("123430", s[1]) != 0)
        fail();

    mem_free(s);
    str_cpy(str, "hello\\\\,world,123430");
    s = str_split(str, ',', &cnt);

    if (cnt != 3)
        fail();
    if (str_cmp("hello\\\\", s[0]) != 0)
        fail();
    if (str_cmp("world", s[1]) != 0)
        fail();
    if (str_cmp("123430", s[2]) != 0)
        fail();

    mem_free(s);
    str_cpy(str, "hello\\,world,123430");
    s = str_split(str, ',', &cnt);

    if (cnt != 2)
        fail();
    if (str_cmp("hello\\,world", s[0]) != 0)
        fail();
    if (str_cmp("123430", s[1]) != 0)
        fail();

    mem_free(s);
}

int main()
{
    test_cpy();
    test_int();
    test_long();
    test_real();
    test_idx();
    test_trim();
    test_split();

    return 0;
}
