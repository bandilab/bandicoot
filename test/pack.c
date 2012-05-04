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

static Env *env;

static Rel *pack_init(char *str)
{
    Head *h;
    Arg arg = { .body = pack_csv2rel(str, &h) };
    Rel *res = NULL;

    if (arg.body != NULL) {
        Vars rvars = { .len = 0 };
        res = rel_param(h);

        rel_init(res, &rvars, &arg);
        mem_free(h);
    }

    return res;
}

static void test_pack()
{
    char str[1024];

    str_cpy(str, "a:int,b:string");
    Rel *rel = pack_init(str);
    if (rel == NULL)
        fail();
    if (rel->head->len != 2)
        fail();
    Tuple *t = rel_next(rel);
    if (t != NULL)
        fail();

    rel_free(rel);

    str_cpy(str, "c:string,a:long\nworld hello,12343");
    rel = pack_init(str);
    if (rel == NULL)
        fail();
    if (rel->head->len != 2)
        fail();
    t = rel_next(rel);
    if (t == NULL)
        fail();

    long long v_long = 12343;
    char *v_str = "world hello";
    Value v1 = val_new_long(&v_long);
    Value v2 = val_new_str(v_str);
    if (!val_eq(tuple_attr(t, 0), v1))
        fail();
    if (!val_eq(tuple_attr(t, 1), v2))
        fail();

    tuple_free(t);
    rel_free(rel);

    str_cpy(str, "c:string,a:int\nhello world,12343\nhello world,123430");
    rel = pack_init(str);
    if (rel == NULL)
        fail();
    if (rel->head->len != 2)
        fail();

    t = rel_next(rel);
    if (t == NULL)
        fail();

    char *v_str2 = "hello world";
    v2 = val_new_str(v_str2);
    if (!val_eq(tuple_attr(t, 1), v2))
        fail();

    tuple_free(t);
    t = rel_next(rel);
    if (t == NULL)
        fail();

    if (!val_eq(tuple_attr(t, 1), v2))
        fail();

    tuple_free(t);
    rel_free(rel);

    /* test duplicate input */
    str_cpy(str, "c:string,a:int\nhello world,12343\nhello world,12343");
    rel = pack_init(str);
    if (rel == NULL)
        fail();
    if (rel->head->len != 2)
        fail();

    t = rel_next(rel);
    if (t == NULL)
        fail();

    v_str2 = "hello world";
    v2 = val_new_str(v_str2);
    if (!val_eq(tuple_attr(t, 1), v2))
        fail();

    tuple_free(t);
    t = rel_next(rel);
    if (t != NULL)
        fail();

    rel_free(rel);

    /* test invalid input */
    str_cpy(str, "hehehe");
    rel = pack_init(str);
    if (rel != NULL)
        fail();

    str_cpy(str, "hehehe:hahaha");
    rel = pack_init(str);
    if (rel != NULL)
        fail();

    str_cpy(str, "abc+_)# $!@#$:int");
    rel = pack_init(str);
    if (rel != NULL)
        fail();

    str_cpy(str, "a:int,b:string\nabc,123");
    rel = pack_init(str);
    if (rel != NULL)
        fail();
}

static void test_unpack()
{
    int i = 0;
    char str[11 + MAX_STRING];

    char *data = "a int,c string\n123,hehehe\n";
    str_cpy(str, data);

    Rel *rel = pack_init(str);
    char *unpacked = mem_alloc(MAX_BLOCK);
    pack_rel2csv(rel, unpacked, MAX_BLOCK, i++);

    if (str_cmp(data, unpacked) != 0)
        fail();

    rel_free(rel);

    char *p = mem_alloc(MAX_STRING + 2);
    for (int i = 0; i < MAX_STRING; ++i)
        p[i] = 'A';
    p[MAX_STRING] = '\n';
    p[MAX_STRING + 1] = '\0';

    data = "a string\n";
    char data2[str_len(data) + str_len(p)];
    str_cpy(data2, data);
    str_cpy(data2 + str_len(data), p);
    str_cpy(str, data2);

    i = 0;
    rel = pack_init(str);
    pack_rel2csv(rel, unpacked, MAX_BLOCK, i++);
    if (str_cmp(data2, unpacked) != 0)
        fail();

    rel_free(rel);

    /* test of a file longer than MAX_BLOCK */
    int start = 1, end = 100000;
    rel = gen_rel(start, end);

    i = 0;
    unpacked = mem_realloc(unpacked, 1024*1024*3);
    int len = 0;
    char *ptr = unpacked;
    while ((len = pack_rel2csv(rel, ptr, MAX_BLOCK, i++)) > 0)
        ptr += len;

    Rel *rel1 = gen_rel(start, end);
    Rel *rel2 = pack_init(unpacked);
    if (!rel_eq(rel1, rel2))
        fail();

    rel_free(rel);
    rel_free(rel1);
    rel_free(rel2);

    mem_free(p);
    mem_free(unpacked);

    int max_tsize = MAX_ATTRS * MAX_STRING + MAX_ATTRS;
    if (max_tsize >= MAX_BLOCK)
        fail();
}

static void test_fn2csv()
{
    char *buf = mem_alloc(MAX_BLOCK);
    int iteration = 0, len = 0;

    len = pack_fn2csv(NULL, 0, buf, MAX_BLOCK, &iteration);
    if (iteration != 1 || len == 0)
        fail();
    len = pack_fn2csv(NULL, 0, buf, MAX_BLOCK, &iteration);
    if (len != 0)
        fail();

    int cnt = 0;
    Func **fns = env_funcs(env, "test_sign1", &cnt);
    char *code = sys_load("test/data/sign1");

    iteration = 0;
    len = pack_fn2csv(fns, cnt, buf, MAX_BLOCK, &iteration);
    if (iteration != 1 || len == 0)
        fail();
    Rel *r1 = pack_init(buf);
    Rel *r2 = pack_init(code);
    if (!rel_eq(r1, r2))
        fail();
    len = pack_fn2csv(fns, cnt, buf, MAX_BLOCK, &iteration);
    if (len != 0)
        fail();

    mem_free(fns);
    mem_free(code);

    cnt = 0;
    fns = env_funcs(env, "", &cnt);
    code = sys_load("test/data/sign");

    char *fullbuf = mem_alloc(MAX_BLOCK);
    int sz = 0;

    iteration = 0;
    int prev = iteration;
    while ((len = pack_fn2csv(fns, cnt, buf, MAX_BLOCK, &iteration))) {
        if (iteration != ++prev)
            fail();
        fullbuf = mem_realloc(fullbuf, MAX_BLOCK * iteration);
        sz += str_cpy(fullbuf + sz, buf);
    }
    if (iteration != env->fns.len + 1)
        fail();
    r1 = pack_init(fullbuf);
    r2 = pack_init(code);
    if (!rel_eq(r1, r2))
        fail();

    mem_free(code);
    mem_free(fns);
    mem_free(fullbuf);
}

static void test_env()
{
    int cnt = 0;
    Func **fns = env_funcs(env, "", &cnt);
    if (cnt != env->fns.len)
        fail();
    mem_free(fns);

    cnt = 1234; /* dummy value */
    fns = env_funcs(env, "does_not_exists", &cnt);
    if (cnt != 0)
        fail();

    fns = env_funcs(env, "load_empty_r1", &cnt);
    if (cnt != 1)
        fail();

    mem_free(fns);
}

int main()
{
    char *source = "test/test_defs.b";
    char *code = sys_load(source);
    env = env_new(source, code);
    mem_free(code);

    test_pack();
    test_unpack();
    test_env();
    test_fn2csv();

    mem_free(env);

    return 0;
}
