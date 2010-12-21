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

static const int MAX = 10;

struct {
    int defi;
    double defd;
    long long defl;

    int min_int, max_int, add_int;
    double min_real, max_real, add_real;
    long long min_long, max_long, add_long;
    double avg_int, avg_real, avg_long;
} res;

static void test_cnt(Tuple *tuples[])
{
    Sum *s = sum_cnt();

    sum_reset(s);
    for (int i = 0; i < MAX; ++i)
        sum_update(s, tuples[i]);

    Value v = sum_value(s);
    if (MAX != val_int(v))
        fail();

    sum_reset(s);
    v = sum_value(s);
    if (0 != val_int(v))
        fail();

    mem_free(s);
}

static void calc(Tuple *tuples[], Sum *s_int, Sum *s_real, Sum *s_long)
{
    sum_reset(s_int);
    sum_reset(s_real);
    sum_reset(s_long);
    for (int i = 0; i < MAX; ++i) {
        sum_update(s_int, tuples[i]);
        sum_update(s_real, tuples[i]);
        sum_update(s_long, tuples[i]);
    }
}

static void test_defs(Sum *s_int, Sum *s_real, Sum *s_long)
{
    sum_reset(s_int);
    sum_reset(s_real);
    sum_reset(s_long);
    if (res.defi != val_int(sum_value(s_int)))
        fail();
    if (res.defd != val_real(sum_value(s_real)))
        fail();
    if (res.defl != val_long(sum_value(s_long)))
        fail();
}

static void test_min(Tuple *tuples[])
{
    Sum *s_int = sum_min(0, Int, val_new_int(&res.defi));
    Sum *s_real = sum_min(1, Real, val_new_real(&res.defd));
    Sum *s_long = sum_min(2, Long, val_new_long(&res.defl));

    calc(tuples, s_int, s_real, s_long);

    if (res.min_int != val_int(sum_value(s_int)))
        fail();
    if (res.min_real != val_real(sum_value(s_real)))
        fail();
    if (res.min_long != val_long(sum_value(s_long)))
        fail();

    test_defs(s_int, s_real, s_long);

    mem_free(s_int);
    mem_free(s_real);
    mem_free(s_long);
}

static void test_max(Tuple *tuples[])
{
    Sum *s_int = sum_max(0, Int, val_new_int(&res.defi));
    Sum *s_real = sum_max(1, Real, val_new_real(&res.defd));
    Sum *s_long = sum_max(2, Long, val_new_long(&res.defl));

    calc(tuples, s_int, s_real, s_long);

    if (res.max_int != val_int(sum_value(s_int)))
        fail();
    if (res.max_real != val_real(sum_value(s_real)))
        fail();
    if (res.max_long != val_long(sum_value(s_long)))
        fail();

    test_defs(s_int, s_real, s_long);

    mem_free(s_int);
    mem_free(s_real);
    mem_free(s_long);
}

static void test_avg(Tuple *tuples[])
{
    Sum *s_int = sum_avg(0, Int, val_new_real(&res.defd));
    Sum *s_real = sum_avg(1, Real, val_new_real(&res.defd));
    Sum *s_long = sum_avg(2, Long, val_new_real(&res.defd));

    calc(tuples, s_int, s_real, s_long);

    if (res.avg_int != val_real(sum_value(s_int)))
        fail();
    if (res.avg_real != val_real(sum_value(s_real)))
        fail();
    if (res.avg_long != val_real(sum_value(s_long)))
        fail();

    sum_reset(s_int);
    sum_reset(s_real);
    sum_reset(s_long);
    if (res.defd != val_real(sum_value(s_int)))
        fail();
    if (res.defd != val_real(sum_value(s_real)))
        fail();
    if (res.defd != val_real(sum_value(s_long)))
        fail();

    mem_free(s_int);
    mem_free(s_real);
    mem_free(s_long);
}

static void test_add(Tuple *tuples[])
{
    Sum *s_int = sum_add(0, Int, val_new_int(&res.defi));
    Sum *s_real = sum_add(1, Real, val_new_real(&res.defd));
    Sum *s_long = sum_add(2, Long, val_new_long(&res.defl));

    calc(tuples, s_int, s_real, s_long);

    if (res.add_int != val_int(sum_value(s_int)))
        fail();
    if (res.add_real != val_real(sum_value(s_real)))
        fail();
    if (res.add_long != val_long(sum_value(s_long)))
        fail();

    test_defs(s_int, s_real, s_long);

    mem_free(s_int);
    mem_free(s_real);
    mem_free(s_long);
}

int main()
{
    res.defi = -1;
    res.defd = -0.999;
    res.defl = -123;
    res.add_int = 0;
    res.add_real = 0.0;
    res.add_long = 0;

    int ints[MAX];
    double reals[MAX];
    long long longs[MAX];
    Value val_ints[MAX];
    Value val_reals[MAX];
    Value val_longs[MAX];

    Tuple *tuples[MAX];

    for (int i = 0; i < MAX; ++i) {
        ints[i] = i + 1;
        reals[i] = i + 0.5;
        longs[i] = 0x7000ffffffffffff;

        val_ints[i] = val_new_int(&ints[i]);
        val_reals[i] = val_new_real(&reals[i]);
        val_longs[i] = val_new_long(&longs[i]);

        if (i == 0 || ints[i] > res.max_int)
            res.max_int = ints[i];
        if (i == 0 || reals[i] > res.max_real)
            res.max_real = reals[i];
        if (i == 0 || longs[i] > res.max_long)
            res.max_long = longs[i];

        if (i == 0 || ints[i] < res.min_int)
            res.min_int = ints[i];
        if (i == 0 || reals[1] < res.min_real)
            res.min_real = reals[i];
        if (i == 0 || longs[i] < res.min_long)
            res.min_long = longs[i];

        res.add_int += ints[i];
        res.add_real += reals[i];
        res.add_long += longs[i];
    }

    res.avg_int = (double) res.add_int / MAX;
    res.avg_real = res.add_real / MAX;
    res.avg_long = (double) res.add_long / MAX;

    Value vals[3];
    for (int i = 0; i < MAX; ++i) {
        vals[0] = val_ints[i];
        vals[1] = val_reals[i];
        vals[2] = val_longs[i];

        tuples[i] = tuple_new(vals, 3);
    }

    test_cnt(tuples);
    test_min(tuples);
    test_max(tuples);
    test_avg(tuples);
    test_add(tuples);

    for (int i = 0; i < MAX; ++i)
        tuple_free(tuples[i]);

    return 0;
}
