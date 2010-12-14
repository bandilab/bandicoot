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

static void test_join(Value t1_vals[], int len1, Value t2_vals[], int len2)
{
    Tuple *t1 = tuple_new(t1_vals, len1);
    Tuple *t2 = tuple_new(t2_vals, len2);

    int lpos[] = {0, 1, -1, -1};
    int rpos[] = {-1, -1, 0, 1};
    Tuple *t3 = tuple_join(t1, t2, lpos, rpos, 4);

    if (!val_eq(tuple_attr(t3, 0), t1_vals[0]))
        fail();

    if (!val_eq(tuple_attr(t3, 1), t1_vals[1]))
        fail();

    if (!val_eq(tuple_attr(t3, 2), t2_vals[0]))
        fail();

    if (!val_eq(tuple_attr(t3, 3), t2_vals[1]))
        fail();

    tuple_free(t1);
    tuple_free(t2);
    tuple_free(t3);
}

static void test_reord(Value vals[], int len)
{
    Tuple *t1 = tuple_new(vals, len);

    int pos1[] = {1, 0};
    Tuple *t2 = tuple_reord(t1, pos1, 2);

    if (!val_eq(tuple_attr(t2, 0), vals[1]))
        fail();
    if (!val_eq(tuple_attr(t2, 1), vals[0]))
        fail();

    int pos2[] = {1};
    Tuple *t3 = tuple_reord(t1, pos2, 1);

    if (!val_eq(tuple_attr(t3, 0), vals[1]))
        fail();

    tuple_free(t1);
    tuple_free(t2);
    tuple_free(t3);
}

static void test_encdec()
{
    Head *h = gen_head();
    int lpos[MAX_ATTRS];
    int rpos[MAX_ATTRS];
    int len = head_common(h, h, lpos, rpos);

    Tuple *t1 = gen_tuple(1);

    char buf1[MAX_STRING];
    char buf2[MAX_STRING];

    int enc_len1 = tuple_enc(t1, buf1);
    int dec_len1;
    Tuple *t2 = tuple_dec(buf1, &dec_len1);
    if (dec_len1 != enc_len1)
        fail();
    if (!tuple_eq(t1, t2, lpos, rpos, len))
        fail();

    int enc_len2 = tuple_enc(t2, buf2);
    if (dec_len1 != enc_len2)
        fail();
    if (mem_cmp(buf1 + sizeof(Tuple),
                buf2 + sizeof(Tuple),
                enc_len2 - sizeof(Tuple)) != 0)
        fail();

    tuple_free(t1);
    tuple_free(t2);
    mem_free(h);
}

static void test_tbuf()
{
    Tuple *t = gen_tuple(1);

    TBuf *b = tbuf_new();
    if (b->pos != b->len || b->pos != 0)
        fail();
    if (tbuf_next(b) != NULL)
        fail();

    tbuf_add(b, t);
    if (b->len != 1)
        fail();
    if (tbuf_next(b) != t)
        fail();
    if (tbuf_next(b) != NULL)
        fail();

    tbuf_add(b, t);
    tbuf_reset(b);
    if (b->len != 2)
        fail();
    if (tbuf_next(b) != t)
        fail();
    if (tbuf_next(b) != t)
        fail();
    if (tbuf_next(b) != NULL)
        fail();

    tuple_free(t);
    tbuf_free(b);
}

int main(void)
{
    int v_int1 = 1, v_int2 = 2;
    char *v_str1 = "string_1", *v_str2 = "string_2";
    Value v1[2];
    v1[0] = val_new_int(&v_int1);
    v1[1] = val_new_str(v_str1);

    Value v2[2];
    v2[0] = val_new_int(&v_int2);
    v2[1] = val_new_str(v_str2);

    test_join(v1, 2, v2, 2);
    test_reord(v1, 2);
    test_encdec();
    test_tbuf();

    return 0;
}
