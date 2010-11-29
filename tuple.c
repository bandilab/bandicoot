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

#include "config.h"
#include "system.h"
#include "memory.h"
#include "string.h"
#include "number.h"
#include "head.h"
#include "value.h"
#include "tuple.h"

static void init(Tuple *t)
{
    void *mem = t + 1;
    t->v.size = mem;
    t->v.off = mem + t->v.len * sizeof(int);
}

extern Tuple *tuple_new(Value vals[], int len)
{
    int size = 0;
    for (int i = 0; i < len; ++i)
        size += vals[i].size;

    int total = size + sizeof(Tuple) + 2 * len * sizeof(int);
    Tuple *res = mem_alloc(total);
    res->size = total;
    res->v.len = len;
    init(res);

    int off = total - size;
    void *mem = (void*) res + off;
    for (int i = 0; i < len; ++i) {
        res->v.off[i] = off;
        res->v.size[i] = val_bin_enc(mem, vals[i]);
        off += res->v.size[i];
        mem += res->v.size[i];
    }

    return res;
}

extern void tuple_free(Tuple *t)
{
    mem_free(t);
}

extern Tuple *tuple_cpy(Tuple *t)
{
    Tuple *res = mem_alloc(t->size);
    mem_cpy(res, t, t->size);
    init(res);

    return res;
}

extern Tuple *tuple_dec(int fd)
{
    Tuple *res = NULL;

    char sz_buf[sizeof(int)];
    int size = sys_readn(fd, sz_buf, sizeof(sz_buf));
    if (size > 0) {
        size = int_dec(sz_buf);
        res = mem_alloc(size);
        if (sys_readn(fd, res, size) != size)
            sys_die("sys: cannot read tuple from file\n");

        init(res);
    } else
        sys_close(fd);

    return res;
}

extern void tuple_enc(Tuple *t, int fd)
{
    char sz_buf[sizeof(int)];
    int_enc(sz_buf, t->size);
    sys_write(fd, sz_buf, sizeof(sz_buf));
    void *mem = t;
    sys_write(fd, mem, t->size);
}

extern Value tuple_attr(Tuple *t, int pos)
{
    void *mem = t;
    Value res = {.size = t->v.size[pos], .data = mem + t->v.off[pos]};
    return res;
}

extern Tuple *tuple_reord(Tuple *t, int pos[], int len)
{
    Value res[len];
    for (int i = 0; i < len; ++i)
        res[i] = tuple_attr(t, pos[i]);

    return tuple_new(res, len);
}

extern Tuple *tuple_join(Tuple *l, Tuple *r, int lpos[], int rpos[], int len)
{
    Value res[len];
    for (int i = 0; i < len; ++i)
        res[i] = rpos[i] == -1
                 ? tuple_attr(l, lpos[i])
                 : tuple_attr(r, rpos[i]);

    return tuple_new(res, len);
}

extern int tuple_eq(Tuple *l, Tuple *r, int lpos[], int rpos[], int len)
{
    int res = 1;
    for (int i = 0; i < len; ++i)
        if (!val_eq(tuple_attr(l, lpos[i]), tuple_attr(r, rpos[i]))) {
            res = 0;
            break;
        }

    return res;
}

extern TBuf *tbuf_new()
{
    TBuf *res = mem_alloc(sizeof(TBuf));
    res->pos = res->len = res->size = 0;
    res->buf = NULL;

    return res;
}

extern void tbuf_add(TBuf *b, Tuple *t)
{
    if (b->len >= b->size) {
        b->size += 128;
        b->buf = mem_realloc(b->buf, sizeof(Tuple*) * b->size);
    }

    b->buf[b->len] = t;
    b->len++;
}

extern void tbuf_reset(TBuf *b)
{
    b->pos = 0;
}

extern Tuple *tbuf_next(TBuf *b)
{
    return (b->pos < b->len) ? b->buf[b->pos++] : NULL;
}

extern void tbuf_clean(TBuf *b)
{
    tbuf_reset(b);

    Tuple *t;
    while ((t = tbuf_next(b)) != NULL)
        tuple_free(t);
}

extern void tbuf_free(TBuf *b)
{
    if (b->buf != NULL)
        mem_free(b->buf);
    mem_free(b);
}
