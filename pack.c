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
#include "array.h"
#include "string.h"
#include "memory.h"
#include "head.h"
#include "value.h"
#include "tuple.h"
#include "volume.h"
#include "system.h"
#include "expression.h"
#include "summary.h"
#include "relation.h"

static int valid_id(const char *id)
{
    int len = 1;
    while (*id != '\0') {
        if (!((*id >= 'a' && *id <= 'z') || (*id >= 'A'&& *id <= 'Z')) &&
            !(*id >= '0' && *id <= '9') &&
            !(*id == '_'))
            return 0;

        if (++len == MAX_NAME)
            return 0;

        id++;
    }

    return 1;
}

static Head *head_pack(char *buf, char *names[])
{
    Head *h = NULL;
    char **attrs = NULL, **name_type = NULL;

    int attrs_len;
    attrs = str_split(buf, '`', &attrs_len);
    if (attrs_len > MAX_ATTRS)
        goto error;

    Type types[MAX_ATTRS];
    char *copy[MAX_ATTRS];
    for (int i = 0; i < attrs_len; ++i) {
        int cnt, bad_type;
        name_type = str_split(attrs[i], '=', &cnt);
        if (cnt != 2 || !valid_id(name_type[0]))
            goto error;

        types[i] = type_from_str(str_trim(name_type[1]), &bad_type);
        if (bad_type)
            goto error;

        names[i] = copy[i] = str_trim(name_type[0]);

        mem_free(name_type);
        name_type = NULL;
    }

    h = head_new(copy, types, attrs_len);

error:
    if (attrs != NULL)
        mem_free(attrs);
    if (name_type != NULL)
        mem_free(name_type);

    return h;
}

extern TBuf *rel_pack_sep(char *buf, Head **res_h)
{
    Head *h = NULL;
    TBuf *tbuf = NULL, *res_tbuf = NULL;
    char **lines = NULL, **str_vals = NULL, *names[MAX_ATTRS];

    int len;
    lines = str_split(buf, '\n', &len);
    if (len < 1)
        goto failure;

    if ((h = head_pack(lines[0], names)) == NULL)
        goto failure;

    if (str_len(lines[len - 1]) == 0)
        len--;

    tbuf = tbuf_new();

    int i = 1;
    for (; i < len; ++i) {
        int attrs;
        str_vals = str_split(lines[i], '`', &attrs);

        if (attrs != h->len)
            goto failure;

        int v_ints[attrs];
        double v_reals[attrs];
        long long v_longs[attrs];
        int v_int_cnt = 0, v_real_cnt = 0, v_long_cnt = 0;
        Value v[attrs];

        for (int j = 0; j < attrs; ++j) {
            int pos = 0; Type t;
            int res = head_attr(h, names[j], &pos, &t);
            if (!res)
                goto failure;

            int error = 0;
            if (t == Int) {
                v_ints[v_int_cnt] = str_int(str_vals[j], &error);
                v[pos] = val_new_int(&v_ints[v_int_cnt++]);
            } else if (t == Real) {
                v_reals[v_real_cnt] = str_real(str_vals[j], &error);
                v[pos] = val_new_real(&v_reals[v_real_cnt++]);
            } else if (t == Long) {
                v_longs[v_long_cnt] = str_long(str_vals[j], &error);
                v[pos] = val_new_long(&v_longs[v_long_cnt++]);
            } else if (t == String) {
                error = str_len(str_vals[j]) > MAX_STRING;
                v[pos] = val_new_str(str_vals[j]);
            }

            if (error)
                goto failure;
        }

        tbuf_add(tbuf, tuple_new(v, attrs));

        mem_free(str_vals);
        str_vals = NULL;
    }

    *res_h = h;
    h = NULL;

    res_tbuf = tbuf;
    tbuf = NULL;

failure:
    if (lines != NULL)
        mem_free(lines);
    if (str_vals != NULL)
        mem_free(str_vals);
    if (h != NULL)
        mem_free(h);
    if (tbuf != NULL) {
        Tuple *t;
        while ((t = tbuf_next(tbuf)) != NULL)
            tuple_free(t);

        mem_free(tbuf);
    }

    return res_tbuf;
}

static int head_unpack(char *dest, Head *h)
{
    int off = 0;
    for (int i = 0; i < h->len; ++i) {
        off += str_print(dest + off,
                         "%s=%s",
                         h->names[i],
                         type_to_str(h->types[i]));

        if ((i + 1) != h->len)
            dest[off++] = '`';
    }
    dest[off++] = '\n';
    dest[off] = '\0';

    return off;
}

static int tuple_unpack(char *dest, Tuple *t, int len, Type type[])
{
    int off = 0;
    for (int i = 0; i < len; i++) {
        if (i != 0)
            dest[off++] = '`';

        Value v = tuple_attr(t, i);
        off += val_to_str(dest + off, v, type[i]);
    }

    dest[off] = '\0';

    return off;
}

static const int UNPACK_STEP = MAX_TUPLE_SIZE * 16;

/* TODO: returning a single buf is too much, consider returning chunks,
         or may be directly writing to the output stream in chunks */
extern char *rel_unpack(Rel *rel, int *size)
{
    int len = rel->head->len;
    Type *types = rel->head->types;

    int buf_size = UNPACK_STEP;
    char *buf = mem_alloc(buf_size);
    int off = head_unpack(buf, rel->head);

    Tuple *t;
    while ((t = rel_next(rel)) != NULL) {
        off += tuple_unpack(buf + off, t, len, types);
        buf[off++] = '\n';

        if ((buf_size - off) < MAX_TUPLE_SIZE)
            buf = mem_realloc(buf, buf_size += UNPACK_STEP);

        tuple_free(t);
    }

    buf[off] = '\0';
    *size = off;

    return buf;
}
