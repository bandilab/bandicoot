/*
Copyright 2008-2012 Ostap Cherkashin
Copyright 2008-2011 Julius Chrobak

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
#include "memory.h"
#include "string.h"
#include "head.h"

extern char *type_to_str(Type t)
{
    if (t == Int)
        return "int";
    else if (t == Long)
        return "long";
    else if (t == Real)
        return "real";
    else if (t == String)
        return "string";

    return NULL;
}

extern Type type_from_str(const char *t, int *error)
{
    *error = 0;
    if (str_cmp(t, "int") == 0)
        return Int;
    else if (str_cmp(t, "long") == 0)
        return Long;
    else if (str_cmp(t, "real") == 0)
        return Real;
    else if (str_cmp(t, "string") == 0)
        return String;

    *error = 1;
    return -1;
}

static void sort(Head *h)
{
    int map[h->len];
    Type types[h->len];
    for (int i = 0; i < h->len; ++i)
        types[i] = h->types[i];

    array_sort(h->names, h->len, map);
    for (int j = 0; j < h->len; ++j)
        h->types[j] = types[map[j]];
}

extern Head *head_new(char *names[], Type types[], int len)
{
    Head *h = mem_alloc(sizeof(Head) + MAX_ATTRS * MAX_NAME);
    h->len = len;

    void *mem = h + 1;
    for (int i = 0; i < MAX_ATTRS; ++i)
        h->names[i] = mem + i * MAX_NAME;

    for (int i = 0; i < len; ++i) {
        str_cpy(h->names[i], names[i]);
        h->types[i] = types[i];
    }

    sort(h);
    return h;
}

extern Head *head_cpy(Head *h)
{
    return head_new(h->names, h->types, h->len);
}

extern int head_common(Head *l, Head *r, int lpos[], int rpos[])
{
    int cnt = 0;
    for (int i = 0; i < l->len; ++i) {
        int p = array_find(r->names, r->len, l->names[i]);
        if (p > -1 && l->types[i] == r->types[p]) {
            lpos[cnt] = i;
            rpos[cnt] = p;
            cnt++;
        }
    }

    return cnt;
}

extern int head_attr(Head *h, char *name, int *pos, Type *t)
{
    int res = 0;
    *pos = array_find(h->names, h->len, name);

    if (*pos > -1) {
        res = 1;
        *t = h->types[*pos];
    }

    return res;
}

extern int head_find(Head *h, char *name)
{
    int pos = array_find(h->names, h->len, name);
    return pos > -1;
}

extern Head *head_join(Head *l, Head *r, int lpos[], int rpos[], int *len)
{
    Head *res = head_cpy(l);
    for (int i = 0; i < r->len; ++i)
        if (array_find(l->names, l->len,  r->names[i]) < 0) {
            str_cpy(res->names[res->len],  r->names[i]);
            res->types[res->len] = r->types[i];
            res->len++;
        }

    sort(res);
    *len = res->len;
    for (int i = 0; i < res->len; ++i) {
        lpos[i] = array_find(l->names, l->len, res->names[i]);
        if (lpos[i] < 0)
            rpos[i] = array_find(r->names, r->len, res->names[i]);
        else
            rpos[i] = -1;
    }

    return res;
}

extern Head *head_project(Head *h, char *names[], int len)
{
    Type types[len];
    char *copy[len];
    for (int i = 0; i < len; ++i)
        copy[i] = names[i];
    array_sort(copy, len, NULL);

    for (int i = 0; i < len; ++i) {
        int p = array_find(h->names, h->len, copy[i]);
        if (p > -1)
            types[i] = h->types[p];
    }

    return head_new(copy, types, len);
}

extern int head_eq(Head *l, Head *r)
{
    int res = l->len == r->len;

    for (int i = 0; i < l->len && res; ++i)
        res = (str_cmp(l->names[i], r->names[i]) == 0) &&
              l->types[i] == r->types[i];

    return res;

}

extern Head *head_rename(Head *h,
                         char *from[],
                         char *to[],
                         int len,
                         int pos[],
                         int *plen)
{
    char *names[h->len];
    Type types[h->len];
    for (int i = 0; i < h->len; ++i) {
        types[i] = h->types[i];
        int p = array_scan(from, len, h->names[i]);
        if (p > -1)
            names[i] = to[p];
        else
            names[i] = h->names[i];
    }

    Head *res = head_new(names, types, h->len);
    *plen = res->len;
    for (int i = 0; i < res->len; ++i)
        pos[i] = array_scan(names, h->len, res->names[i]);

    return res;
}

extern void head_to_str(char *dest, Head *h)
{
    if (h == NULL) {
        dest = '\0';
        return;
    }

    int off = str_print(dest, "%s", "{");
    for (int i = 0; i < h->len; ++i)
        off += str_print(dest + off,
                         (i == h->len - 1) ? "%s %s" : "%s %s, ",
                         h->names[i],
                         type_to_str(h->types[i]));
    dest[off++] = '}';
    dest[off] = '\0';
}
