/*
Copyright 2008-2012 Ostap Cherkashin
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

static Rel *pack_init(char *str,
                      char *names[],
                      Type types[],
                      int len,
                      char *errmsg)
{
    char *n[len];
    Type t[len];
    for (int i = 0; i < len; ++i) {
        n[i] = names[i];
        t[i] = types[i];
    }
    Head *exp = head_new(n, t, len);

    TBuf *b = NULL;
    Error *err = pack_csv2rel(str, exp, &b);
    Vars *v = vars_new(1);
    vars_add(v, "param", 0, b);

    Rel *res = NULL;
    if (err == NULL) {
        if (errmsg != NULL)
            fail();

        /* FIXME: project should be part of the function call or the pack itself */
        res = rel_project(rel_load(exp, "param"), exp->names, exp->len);

        Arg arg;
        rel_eval(res, v, &arg);
    } else {
        if (errmsg == NULL)
            fail();
        if (str_cmp(err->msg, errmsg) != 0)
            fail();

        mem_free(err);
    }

    mem_free(exp);
    return res;
}

static void test_pack()
{
    char *names[2];
    Type types[2];
    char str[1024];

    names[0] = "a"; names[1] = "b";
    types[0] = Int; types[1] = String;
    str_cpy(str, "a,b");
    Rel *rel = pack_init(str, names, types, 2, NULL);

    if (rel == NULL)
        fail();
    if (rel->head->len != 2)
        fail();
    Tuple *t = tbuf_next(rel->body);
    if (t != NULL)
        fail();

    rel_free(rel);

    names[0] = "c"; names[1] = "a";
    types[0] = String; types[1] = Long;
    str_cpy(str, "c,a\nworld hello,12343");
    rel = pack_init(str, names, types, 2, NULL);

    if (rel == NULL)
        fail();
    if (rel->head->len != 2)
        fail();
    t = tbuf_next(rel->body);
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

    names[0] = "c"; names[1] = "a";
    types[0] = String; types[1] = Int;
    str_cpy(str, "c,a\nhello world,12343\nhello world,123430");
    rel = pack_init(str, names, types, 2, NULL);

    if (rel == NULL)
        fail();
    if (rel->head->len != 2)
        fail();

    t = tbuf_next(rel->body);
    if (t == NULL)
        fail();

    char *v_str2 = "hello world";
    v2 = val_new_str(v_str2);
    if (!val_eq(tuple_attr(t, 1), v2))
        fail();

    tuple_free(t);
    t = tbuf_next(rel->body);
    if (t == NULL)
        fail();

    if (!val_eq(tuple_attr(t, 1), v2))
        fail();

    tuple_free(t);
    rel_free(rel);

    /* test duplicate input */
    str_cpy(str, "c,a\nhello world,12343\nhello world,12343");
    rel = pack_init(str, names, types, 2, NULL);

    if (rel == NULL)
        fail();
    if (rel->head->len != 2)
        fail();

    t = tbuf_next(rel->body);
    if (t == NULL)
        fail();

    v_str2 = "hello world";
    v2 = val_new_str(v_str2);
    if (!val_eq(tuple_attr(t, 1), v2))
        fail();

    tuple_free(t);
    t = tbuf_next(rel->body);
    if (t != NULL)
        fail();

    rel_free(rel);

    names[0] = "hehehe";
    types[0] = String;
    str_cpy(str, "hehehe");
    rel = pack_init(str, names, types, 1, NULL);
    if (rel == NULL)
        fail();

    /* test invalid input */
    str_cpy(str, "abc+_)# $!@#$");
    rel = pack_init(str, names, types, 1,
            "bad header: invalid attribute name: 'abc+_)# $!@#$'");
    if (rel != NULL)
        fail();

    names[0] = "a"; names[1] = "b";
    types[0] = Int; types[1] = String;
    str_cpy(str, "a,b\nabc,123");
    rel = pack_init(str, names, types, 2,
            "bad tuple on line 2: value 'abc' (attribute 'a') "
            "is not of type 'int'");
    if (rel != NULL)
        fail();
}

static char* unpack(Rel *r, int buf_sz)
{
    PBuf tmp = { .data = NULL, .len = 0, .iteration = 0 };

    int len = 1, off = 0, sz = buf_sz;
    char *buf = mem_alloc(buf_sz), *res = mem_alloc(sz);

    while (len > 0) {
        len = pack_rel2csv(r, buf, buf_sz, &tmp);
        sz += buf_sz;
        res = mem_realloc(res, sz);
        off += str_cpy(res + off, buf);
    }
    res[off++] = '\0';

    mem_free(buf);
    return res;
}

static void test_unpack()
{
    char str[11 + MAX_STRING];

    char *names[2];
    Type types[2];

    names[0] = "a"; names[1] = "c";
    types[0] = Int; types[1] = String;

    /* simple test - everything fits into the buffer */
    char *data = "a,c\n123,hehehe\n";
    str_cpy(str, data);

    Rel *rel = pack_init(str, names, types, 2, NULL);
    char *unpacked = unpack(rel, MAX_BLOCK);
    if (str_cmp(data, unpacked) != 0)
        fail();

    rel_free(rel);
    mem_free(unpacked);

    names[0] = "a";
    types[0] = String;

    /* single tuple is bigger than the buffer */
    data = "a\nHello World!\n";
    str_cpy(str, data);

    rel = pack_init(str, names, types, 1, NULL);
    unpacked = unpack(rel, 12);
    if (str_cmp(data, unpacked) != 0)
        fail();

    rel_free(rel);
    mem_free(unpacked);

    /* small tuples, but whole relation does not fit into the buffer */
    int start = 1, end = 10000;
    Head *h = gen_head();
    rel = gen_rel(start, end);

    unpacked = unpack(rel, 256);

    Rel *rel1 = gen_rel(start, end);
    Rel *rel2 = pack_init(unpacked, h->names, h->types, h->len, NULL);
    if (!rel_eq(rel1, rel2))
        fail();

    mem_free(h);
    rel_free(rel);
    rel_free(rel1);
    rel_free(rel2);
    mem_free(unpacked);

    /* check if the config is allright, the header must fit into MAX_BLOCK */
    int max_tsize = MAX_ATTRS * MAX_STRING + MAX_ATTRS;
    if (max_tsize >= MAX_BLOCK)
        fail();
}

static void test_fn2csv()
{
    char *ns[] = {"fname", "pattr", "pname", "ptype"};
    Type ts[] = {String, String, String, String};

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
    Rel *r1 = pack_init(buf, ns, ts, 4, NULL);
    Rel *r2 = pack_init(code, ns, ts, 4, NULL);
    if (!rel_eq(r1, r2))
        fail();
    len = pack_fn2csv(fns, cnt, buf, MAX_BLOCK, &iteration);
    if (len != 0)
        fail();

    mem_free(fns);
    mem_free(code);
    rel_free(r1);
    rel_free(r2);

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
    r1 = pack_init(fullbuf, ns, ts, 4, NULL);
    r2 = pack_init(code, ns, ts, 4, NULL);
    if (!rel_eq(r1, r2))
        fail();

    mem_free(fns);
    mem_free(code);
    rel_free(r1);
    rel_free(r2);
    mem_free(fullbuf);
    mem_free(buf);
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

    env_free(env);

    return 0;
}
