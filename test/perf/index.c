#include "../common.h"

static const int FIND_CALLS = 100 * 1000;

static void perf_sort(int count, int pos[], int len)
{
    TBuf *b = gen_tuples(-count / 2, count / 2);

    long long time = sys_millis();
    index_sort(b, pos, len);
    sys_print("sorted %d tuples in %lldms\n", count, sys_millis() - time);

    tbuf_clean(b);
    tbuf_free(b);
}

static void perf_find(int count, int pos[], int len)
{
    int start = -count / 2;
    Tuple *t = gen_tuple(start);
    TBuf *b = gen_tuples(start, count / 2);

    index_sort(b, pos, len);

    long long time = sys_millis();
    for (int i = 0; i < FIND_CALLS; ++i)
        if (!index_has(b, t, pos, pos, len))
            fail();
    sys_print("found %d tuples within %d-tuple index in %lldms\n",
              FIND_CALLS, count, sys_millis() - time);

    tuple_free(t);
    tbuf_clean(b);
    tbuf_free(b);
}

int main()
{
    Head *h = gen_head();
    int lpos[MAX_ATTRS], rpos[MAX_ATTRS];
    int len = head_common(h, h, lpos, rpos);

    perf_sort(10 * 1000, lpos, len);
    perf_sort(100 * 1000, lpos, len);
    perf_sort(1000 * 1000, lpos, len);

    perf_find(10 * 1000, lpos, len);
    perf_find(100 * 1000, lpos, len);
    perf_find(1000 * 1000, lpos, len);

    return 0;
}
