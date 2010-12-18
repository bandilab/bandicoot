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

#include "../common.h"

static void perf_cmp(int M)
{
    Head *h = gen_head();
    int lpos[MAX_ATTRS], rpos[MAX_ATTRS];
    int len = head_common(h, h, lpos, rpos);

    TBuf *buf1 = gen_tuples(0, 1000);
    TBuf *buf2 = gen_tuples(0, 1000);

    long time = sys_millis();

    Tuple *t1, *t2;

    for (int i = 0; i < M; ++i) {
        tbuf_reset(buf1);
        while ((t1 = tbuf_next(buf1)) != NULL) {
            tbuf_reset(buf2);
            while ((t2 = tbuf_next(buf2)) != NULL)
                tuple_eq(t1, t2, lpos, rpos, len);
        }
    }

    sys_print("tuple_eq(%dM) in %dms\n", M, sys_millis() - time);

    while ((t1 = tbuf_next(buf1)) != NULL)
        tuple_free(t1);
    while ((t2 = tbuf_next(buf2)) != NULL)
        tuple_free(t2);

    mem_free(buf1);
    mem_free(buf2);
}

static void perf_encdec(int M)
{
    int count = M * 1000 * 1000;

    Head *h = gen_head();
    int lpos[MAX_ATTRS], rpos[MAX_ATTRS];
    int len = head_common(h, h, lpos, rpos);

    Tuple *t = gen_tuple(1);

    char buf[h->len * MAX_STRING + h->len];
    long i;

    long time = sys_millis();
    for (i = 0; i < count; ++i)
        tuple_enc(t, buf);
    sys_print("tuple_enc(%dM) in %dms\n", M, sys_millis() - time);

    tuple_free(t);

    Tuple **tbuf = mem_alloc(count * sizeof(Tuple*));

    time = sys_millis();
    for (i = 0; i < count; ++i) {
        tbuf[i] = tuple_dec(buf, &len);
    }
    sys_print("tuple_dec(%dM) in %dms\n", M, sys_millis() - time);

    for (i = 0; i < count; ++i)
        tuple_free(tbuf[i]);
    mem_free(tbuf);
}

int main()
{
    perf_cmp(1);
    perf_cmp(10);
    perf_encdec(1);
    perf_encdec(10);

    return 0;
}
