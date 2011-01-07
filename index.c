#include "config.h"
#include "memory.h"
#include "head.h"
#include "value.h"
#include "tuple.h"
#include "index.h"

static void merge(Tuple *keys[],
                  Tuple *tmp[],
                  int pos[],
                  int len,
                  int left,
                  int mid,
                  int right)
{
    int llen = mid - left;
    int rlen = right - mid;

    Tuple **l = tmp;
    Tuple **r = tmp + llen + 1;
    for (int i = 0; i < llen; ++i)
        l[i] = keys[left + i];
    for (int i = 0; i < rlen; ++i)
        r[i] = keys[mid + i];
    l[llen] = NULL;
    r[rlen] = NULL;

    int i = 0, j = 0, k = left;
    while (l[i] != NULL && r[j] != NULL)
        if (tuple_cmp(l[i], r[j], pos, pos, len) <= 0)
            keys[k++] = l[i++];
        else
            keys[k++] = r[j++];

    while (l[i] != NULL)
        keys[k++] = l[i++];
    while (r[j] != NULL)
        keys[k++] = r[j++];
}

static void sort(Tuple *keys[],
                 Tuple *tmp[],
                 int pos[],
                 int len,
                 int left,
                 int right)
{
    int elems = right - left;
    if (elems < 2)
        return;

    int mid = left + elems / 2;
    sort(keys, tmp, pos, len, left, mid);
    sort(keys, tmp, pos, len, mid, right);

    merge(keys, tmp, pos, len, left, mid, right);
}

static int find(TBuf *idx, Tuple *t, int ipos[], int tpos[], int len)
{
    int low = 0, high = idx->len - 1;

    while (low <= high) {
        int mid = (low + high) / 2;

        if (tuple_cmp(t, idx->buf[mid], tpos, ipos, len) < 0)
            high = mid - 1;
        else if (tuple_cmp(t, idx->buf[mid], tpos, ipos, len) > 0)
            low = mid + 1;
        else
            return mid;
    }

    return -1;
}

extern void index_sort(TBuf *buf, int pos[], int len)
{
    Tuple **tmp = mem_alloc((buf->len + 2) * sizeof(Tuple*));
    sort(buf->buf, tmp, pos, len, 0, buf->len);
    mem_free(tmp);
}

extern int index_has(TBuf *idx, Tuple *t, int ipos[], int tpos[], int len)
{
    return find(idx, t, ipos, tpos, len) >= 0;
}

extern TBuf *index_match(TBuf *idx, Tuple *t, int ipos[], int tpos[], int len)
{
    int match = find(idx, t, ipos, tpos, len);
    if (match < 0)
        return NULL;

    TBuf *res = tbuf_new();

    int i = match - 1;
    while (i >= 0 && tuple_cmp(idx->buf[i], t, ipos, tpos, len) == 0)
        tbuf_add(res, idx->buf[i--]);

    i = match;
    while (i < idx->len && tuple_cmp(idx->buf[i], t, ipos, tpos, len) == 0)
        tbuf_add(res, idx->buf[i++]);

    return res;
}
