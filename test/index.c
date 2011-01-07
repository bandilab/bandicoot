#include "common.h"

static void test_sort(int pos[], int len, int start, int end)
{
    TBuf *b = gen_tuples(start, end);
    index_sort(b, pos, len);

    int elems = end - start;
    if (b->len != elems)
        fail();

    if (elems > 1) {
        tbuf_reset(b);
        Tuple *prev = tbuf_next(b), *next = NULL;
        while ((next = tbuf_next(b)) != NULL) {
            if (tuple_cmp(prev, next, pos, pos, len) > 0)
                fail();

            prev = next;
        }
    }

    tbuf_clean(b);
    tbuf_free(b);
}

static void test_find_match(int pos[], int len)
{
    TBuf *b = gen_tuples(-300, 300);
    index_sort(b, pos, len);

    Tuple *t = gen_tuple(505);
    if (index_has(b, t, pos, pos, len))
        fail();
    TBuf *m = index_match(b, t, pos, pos, len);
    if (m != NULL)
        fail();
    tuple_free(t);

    t = gen_tuple(-300);
    if (!index_has(b, t, pos, pos, len))
        fail();
    m = index_match(b, t, pos, pos, len);
    if (m == NULL)
        fail();
    if (m->len != 1)
        fail();
    if (tuple_cmp(t, m->buf[0], pos, pos, len) != 0)
        fail();
    tuple_free(t);
    tbuf_free(m);

    t = gen_tuple(299);
    if (!index_has(b, t, pos, pos, len))
        fail();
    m = index_match(b, t, pos, pos, len);
    if (m == NULL)
        fail();
    if (m->len != 1)
        fail();
    if (tuple_cmp(t, m->buf[0], pos, pos, len) != 0)
        fail();
    tuple_free(t);
    tbuf_free(m);

    tbuf_clean(b);
    tbuf_free(b);
}

int main()
{
    Head *h = gen_head();
    int lpos[MAX_ATTRS], rpos[MAX_ATTRS];
    int len = head_common(h, h, lpos, rpos);

    test_sort(lpos, len, 0, 0);
    test_sort(lpos, len, 0, 1);
    test_sort(lpos, len, -1, 1);
    test_sort(lpos, len, -337, 12);

    test_find_match(lpos, len);

    mem_free(h);

    return 0;
}
