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

static void test_int()
{
    int v_int1 = 345;
    Value v1 = val_new_int(&v_int1);
    Value v2 = val_new_int(&v_int1);

    if (!val_eq(v1, v2))
        fail();

    int v_int2 = -345;
    v2 = val_new_int(&v_int2);
    if (val_eq(v1, v2))
        fail();

    v1 = val_new_int(&v_int2);
    if (!val_eq(v1, v2))
        fail();

    int v_int3 = 0x7FFFFFFF;
    v2 = val_new_int(&v_int3); /* = 2147483647 */
    if (val_eq(v1, v2))
        fail();
}

static void test_real()
{
    double v_real1 = 123.123456, v_real2 = 123.123455;
    Value v1 = val_new_real(&v_real1);
    Value v2 = val_new_real(&v_real1);

    if (!val_eq(v1, v2))
        fail();

    v2 = val_new_real(&v_real2);
    if (val_eq(v1, v2))
        fail();
}

static void test_string()
{
    char *v_str1 = "hello", *v_str2 = "world";
    Value v1 = val_new_str(v_str1);
    Value v2 = val_new_str(v_str1);

    if (!val_eq(v1, v2))
        fail();

    v2 = val_new_str(v_str2);
    if (val_eq(v1, v2))
        fail();

    v1 = val_new_str(v_str2);
    if (!val_eq(v1, v2))
        fail();
}

static void test_encdec()
{
    int v_int = 123;
    double v_real = 123.232;
    char *v_str = "hello world";
    long long v_long = 0x7FFFFFFFFFFFFFFFLL;
    Value ival = val_new_int(&v_int);
    Value rval = val_new_real(&v_real);
    Value sval = val_new_str(v_str);
    Value lval = val_new_long(&v_long);

    if (v_int != val_int(ival))
        fail();
    if (v_real != val_real(rval))
        fail();
    if (str_cmp(v_str, val_str(sval)) != 0)
        fail();
    if (v_long != val_long(lval))
        fail();
}

static void test_to_str()
{
    char istr[MAX_STRING];
    char rstr[MAX_STRING];
    char sstr[MAX_STRING];
    char lstr[MAX_STRING];

    int v_int = 123;
    double v_real = 123.232;
    char *v_str = "hello world";
    long long v_long = 0x7FFFFFFFFFFFFFFFLL;
    int ilen = val_to_str(istr, val_new_int(&v_int), Int);
    int rlen = val_to_str(rstr, val_new_real(&v_real), Real);
    int slen = val_to_str(sstr, val_new_str(v_str), String);
    int llen = val_to_str(lstr, val_new_long(&v_long), Long);

    if (str_cmp(istr, "123") != 0 || ilen != 3)
        fail();
    if (str_cmp(rstr, "123.232") != 0 || rlen != 7)
        fail();
    if (str_cmp(sstr, "hello world") != 0 || slen != 11)
        fail();
    if (str_cmp(lstr, "9223372036854775807") != 0 || llen != 19)
        fail();
}

int main(void)
{
    test_int();
    test_real();
    test_string();
    test_encdec();
    test_to_str();

    return 0;
}
