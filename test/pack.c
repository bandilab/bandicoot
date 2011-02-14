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

static Rel *pack_init(char *str)
{
    Head *h;
    TBuf *buf = rel_pack_sep(str, &h);
    Rel *res = NULL;

    if (buf != NULL) {
        Args args = { .len = 1, .names[0] = "input", .tbufs[0] = buf };
        res = rel_param(h, "input");

        rel_init(res, &args);
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
    int size;
    char str[11 + MAX_STRING];

    char *data = "a:int,c:string\n123,hehehe\n";
    str_cpy(str, data);

    Rel *rel = pack_init(str);
    char *unpacked = rel_unpack(rel->head, rel->body, &size);

    if (str_cmp(data, unpacked) != 0)
        fail();

    rel_free(rel);
    mem_free(unpacked);

    char *p = mem_alloc(MAX_STRING + 2);
    for (int i = 0; i < MAX_STRING; ++i)
        p[i] = 'A';
    p[MAX_STRING] = '\n';
    p[MAX_STRING + 1] = '\0';

    data = "a:string\n";
    char data2[str_len(data) + str_len(p)];
    str_cpy(data2, data);
    str_cpy(data2 + str_len(data), p);
    str_cpy(str, data2);

    rel = pack_init(str);
    unpacked = rel_unpack(rel->head, rel->body, &size);
    if (str_cmp(data2, unpacked) != 0)
        fail();

    rel_free(rel);
    mem_free(p);
    mem_free(unpacked);
}

int main()
{
    test_pack();
    test_unpack();

    return 0;
}
