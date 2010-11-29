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

static void test_codec()
{
    int idx;

    unsigned char i[sizeof(int)];
    unsigned char d[sizeof(double)];

    int_enc(i, 0x0AFF0C0D);
    if (0x0AFF0C0D != int_dec(i))
        fail();

    for (idx = -500; idx < 500; ++idx) {
        int_enc(i, idx);
        if (idx != int_dec(i))
            fail();
    }

    real_enc(d, 1);
    if (1 != real_dec(d))
        fail();

    for (idx = -500; idx < 500; ++idx) {
        real_enc(d, idx);
        if (idx != (int) real_dec(d))
            fail();
    }
}

static void test_mul()
{
    int error = 0;
    unsigned long long lres = ulong_mul(3LL, 7LL, &error);
    if (error || lres != 21LL)
        fail();

    error = 0;
    lres = ulong_mul(MAX_ULONG / 2, 10, &error);
    if (!error)
        fail();
}

static void test_add()
{
    int error = 0;
    unsigned long long lres = ulong_add(123LL, 9LL, &error);
    if (error || lres != 132LL)
        fail();

    error = 0;
    lres = ulong_add(MAX_ULONG, 1LL, &error);
    if (!error)
        fail();
}

int main(void)
{
    test_codec();
    test_mul();
    test_add();

    return 0;
}
