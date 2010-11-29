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

static const unsigned int MAX_INT = 0x7FFFFFFFU;
static const unsigned int MAX_UINT = 0xFFFFFFFFU;
static const unsigned long long MAX_LONG = 0x7FFFFFFFFFFFFFFFULL;
static const unsigned long long MAX_ULONG = 0xFFFFFFFFFFFFFFFFULL;

static void int_enc(void *buf, int val)
{
    *((int*) buf) = val;
}

static int int_dec(const void *buf)
{
    return *((int*) buf);
}

static int int_overflow(int sign, unsigned int val)
{
    if (sign > 0 && val > MAX_INT)
        return 1;
    else if (sign < 0 && val > MAX_INT + 1)
        return 1;

    return 0;
}

static void long_enc(void *buf, long long val)
{
    *((long long*) buf) = val;
}

static long long long_dec(const void *buf)
{
    return *((long long*) buf);
}

static unsigned long long ulong_mul(unsigned long long si1,
                                    unsigned long long si2,
                                    int *error)
{
    if (si2 > 0ULL && si1 > MAX_ULONG / si2)
        *error = 1;

    return si1 * si2;
}

static unsigned long long ulong_add(unsigned long long si1,
                                    unsigned long long si2,
                                    int *error)
{
    if (si1 > MAX_ULONG - si2)
        *error = 1;

    return si1 + si2;
}

static int long_overflow(int sign, unsigned long long val)
{
    if (sign > 0 && val > MAX_LONG)
        return 1;
    else if (sign < 0 && val > MAX_LONG + 1)
        return 1;

    return 0;
}

static void real_enc(void *buf, double val)
{
    *((double*) buf) = val;
}

static double real_dec(const void *buf)
{
    return *((double*) buf);
}
