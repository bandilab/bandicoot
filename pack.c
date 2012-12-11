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

#include "config.h"
#include "system.h"
#include "array.h"
#include "string.h"
#include "memory.h"
#include "head.h"
#include "value.h"
#include "tuple.h"
#include "volume.h"
#include "expression.h"
#include "summary.h"
#include "variable.h"
#include "relation.h"
#include "environment.h"
#include "error.h"

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

static Error *pack_csv2head(char *buf, Head *exp, char *names[])
{
    char *attrs[MAX_ATTRS];

    int len = str_split(buf, ",", attrs, MAX_ATTRS);
    if (len < 1)
        return error_new("bad header: '%s'", buf);

    Type types[len];
    char *copy[len];
    for (int i = 0; i < len; ++i) {
        char *name = str_trim(attrs[i]);
        if (!valid_id(name))
            return error_new("bad header: invalid attribute name: '%s'", name);

        names[i] = copy[i] = name;

        int idx = 0;
        head_attr(exp, name, &idx, &types[i]);
    }

    Head *res = head_new(copy, types, len);
    if (!head_eq(exp, res)) {
        char head_exp[MAX_HEAD_STR];
        char head_got[MAX_HEAD_STR];

        head_to_str(head_exp, exp);
        head_to_str(head_got, res);

        mem_free(res);
        return error_new("bad header: expected %s got %s", head_exp, head_got);
    }

    return NULL;
}

static Error *pack_csv2tuple(char *buf,
                             int line,
                             char *names[],
                             Head *head,
                             Tuple **out)
{
    char *str_vals[MAX_ATTRS];

    int attrs = str_split(buf, ",", str_vals, MAX_ATTRS);
    if (attrs != head->len)
        return error_new("bad tuple on line %d: expected %d attributes, got %d",
                         line, head->len, attrs);

    int v_ints[attrs];
    double v_reals[attrs];
    long long v_longs[attrs];
    int v_int_cnt = 0, v_real_cnt = 0, v_long_cnt = 0;
    Value v[attrs];

    for (int i = 0; i < attrs; ++i) {
        int pos = 0; Type t;
        int res = head_attr(head, names[i], &pos, &t);
        if (!res)
            return error_new("bad header: unexpected attribute '%s'",
                             names[i]);

        int error = 0;
        if (t == Int) {
            v_ints[v_int_cnt] = str_int(str_vals[i], &error);
            v[pos] = val_new_int(&v_ints[v_int_cnt++]);
        } else if (t == Real) {
            v_reals[v_real_cnt] = str_real(str_vals[i], &error);
            v[pos] = val_new_real(&v_reals[v_real_cnt++]);
        } else if (t == Long) {
            v_longs[v_long_cnt] = str_long(str_vals[i], &error);
            v[pos] = val_new_long(&v_longs[v_long_cnt++]);
        } else if (t == String) {
            error = str_len(str_vals[i]) > MAX_STRING;
            v[pos] = val_new_str(str_vals[i]);
        }

        if (error)
            return error_new("bad tuple on line %d: value '%s' "
                             "(attribute '%s') is not of type '%s'",
                             line, str_vals[i], names[i], type_to_str(t));
    }

    *out = tuple_new(v, attrs);
    return NULL;
}

extern Error *pack_csv2rel(char *buf, Head *head, TBuf **body)
{
    Error *err = NULL;
    char **lines = NULL;
    char *names[MAX_ATTRS];

    *body = NULL;

    int len;
    lines = str_split_big(buf, "\n", &len);
    if (len < 1) {
        err = error_new("bad csv: missing header");
        goto exit;
    }

    if ((err = pack_csv2head(lines[0], head, names)) != NULL)
        goto exit;

    if (str_len(lines[len - 1]) == 0)
        len--;

    *body = tbuf_new();
    for (int i = 1; i < len; ++i) {
        Tuple *t = NULL;
        if ((err = pack_csv2tuple(lines[i], i + 1, names, head, &t)) != NULL)
            goto exit;

        tbuf_add(*body, t);
    }

exit:
    if (lines != NULL)
        mem_free(lines);

    if (err != NULL && *body != NULL) {
        Tuple *t = NULL;
        while ((t = tbuf_next(*body)) != NULL)
            tuple_free(t);

        tbuf_free(*body);
        *body = NULL;
    }

    return err;
}

static int head_unpack(char *dest, Head *h)
{
    int off = 0;
    for (int i = 0; i < h->len; ++i) {
        off += str_print(dest + off, "%s", h->names[i]);

        if ((i + 1) != h->len)
            dest[off++] = ',';
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
            dest[off++] = ',';

        Value v = tuple_attr(t, i);
        off += val_to_str(dest + off, v, type[i]);
    }

    dest[off] = '\0';

    return off;
}

extern int pack_rel2csv(Rel *r, char *buf, int size, int iteration)
{
    int off = 0;

    if (r == NULL || r->body == NULL)
        return off;

    if (iteration == 0)
        off = head_unpack(buf, r->head);

    int len = r->head->len;
    Type *types = r->head->types;

    int tsize = len * MAX_STRING + len;
    Tuple *t = NULL;
    while ((size - off) > tsize && (t = tbuf_next(r->body)) != NULL) {
        off += tuple_unpack(buf + off, t, len, types);
        tuple_free(t);

        buf[off++] = '\n';
    }

    buf[off] = '\0';

    if (t == NULL && r->body != NULL) {
        tbuf_free(r->body);
        r->body = NULL;
    }

    return off;
}

extern int pack_fn2csv(Func **fns, int cnt, char *buf, int size, int *iteration)
{
    int idx = (*iteration)++, off = 0;
    if (idx == 0) {
        char *names[] = {"fname", "pname", "pattr", "ptype"};
        Type types[] = {String, String, String, String};

        Head *h = head_new(names, types, 4);
        off = head_unpack(buf, h);
        mem_free(h);
    }

    if (cnt <= idx)
        return off;
    Func *fn = fn = fns[idx];

    char *pattern = "%s,%s,%s,%s\n";
    char *name = fn->name;
    Head *ret = fn->ret;

    off += str_print(buf + off, pattern, name, "", "", "");

    if (ret != NULL)
        for (int j = 0; j < ret->len; ++j)
            off += str_print(buf + off, pattern,
                             name,
                             ret->names[j],
                             "return",
                             type_to_str(ret->types[j]));

    for (int j = 0; j < fn->pp.len; ++j)
        off += str_print(buf + off, pattern,
                         name,
                         "",
                         fn->pp.names[j],
                         type_to_str(fn->pp.types[j]));

    Head *rp = fn->rp.head;
    if (rp != NULL)
        for (int i = 0; i < rp->len; ++i)
            off += str_print(buf + off, pattern,
                             name,
                             rp->names[i],
                             fn->rp.name,
                             type_to_str(rp->types[i]));

    return off;
}
