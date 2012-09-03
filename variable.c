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
#include "memory.h"
#include "system.h"
#include "string.h"
#include "head.h"
#include "value.h"
#include "tuple.h"
#include "variable.h"

static void *vars_p(Vars *v)
{
    return ((void*) v->names) + v->size * sizeof(void*);
}

static void *vols_p(Vars *v)
{
    return ((void*) v->vols) + v->size * sizeof(void*);
}

static void vars_init(Vars *v)
{
    for (int i = 0; i < v->size; ++i) {
        v->names[i] = vars_p(v) + i * MAX_NAME;
        mem_set(v->names[i], 0, MAX_NAME);
        v->vols[i] = vols_p(v) + i * MAX_ADDR;
        mem_set(v->vols[i], 0, MAX_ADDR);
        v->vers[i] = 0;
        v->vals[i] = NULL;
    }
}

extern Vars *vars_new(int len)
{
    Vars *res = mem_alloc(sizeof(Vars));
    res->size = len;
    res->len = 0;
    res->names = mem_alloc(len * (sizeof(void*) + MAX_NAME));
    res->vols = mem_alloc(len * (sizeof(void*) + MAX_ADDR));
    res->vers = mem_alloc(len * sizeof(long long));
    res->vals = mem_alloc(len * sizeof(void*));

    vars_init(res);

    return res;
}

extern void vars_cpy(Vars *dest, Vars *src)
{
    dest->len = 0;
    for (int i = 0; i < src->len; ++i) {
        vars_add(dest, src->names[i], src->vers[i], src->vals[i]);
        str_cpy(dest->vols[i], src->vols[i]);
    }
}

extern void vars_free(Vars *v)
{
    mem_free(v->names);
    mem_free(v->vers);
    mem_free(v->vols);
    mem_free(v->vals);
    mem_free(v);
}

extern Vars *vars_read(IO *io)
{
    Vars *v = NULL;

    int len;
    if (sys_readn(io, &len, sizeof(len)) != sizeof(len))
       goto failure;

    v = vars_new(len);
    v->len = len;
    int size = len * MAX_NAME;
    if (sys_readn(io, vars_p(v), size) != size)
        goto failure;

    size = len * sizeof(long long);
    if (sys_readn(io, v->vers, size) != size)
        goto failure;

    size = len * MAX_ADDR;
    if (sys_readn(io, vols_p(v), size) != size)
        goto failure;

    return v;

failure:
    if (v != NULL)
        vars_free(v);

    return NULL;
}

extern int vars_write(Vars *v, IO *io)
{
    if (sys_write(io, &v->len, sizeof(v->len)) < 0 ||
        sys_write(io, vars_p(v), v->len * MAX_NAME) < 0 ||
        sys_write(io, v->vers, v->len * sizeof(long long)) < 0 ||
        sys_write(io, vols_p(v), v->len * MAX_ADDR) < 0)
        return -1;

    return sizeof(v->len) + v->len * (MAX_NAME + sizeof(long long) + MAX_ADDR);
}

extern void vars_add(Vars *v, const char *var, long long ver, TBuf *val)
{
    if (v->len == v->size) {
        char **names = v->names;
        char **vols = v->vols;
        long long *vers = v->vers;
        TBuf **vals = v->vals;

        v->size += MAX_VARS;
        v->names = mem_alloc(v->size * (sizeof(void*) + MAX_NAME));
        v->vols = mem_alloc(v->size * (sizeof(void*) + MAX_ADDR));
        v->vers = mem_alloc(v->size * sizeof(long long));
        v->vals = mem_alloc(v->size * sizeof(void*));

        vars_init(v);

        for (int i = 0; i < v->len; ++i) {
            str_cpy(v->names[i], names[i]);
            str_cpy(v->vols[i], vols[i]);
            v->vers[i] = vers[i];
            v->vals[i] = vals[i];
        }
        mem_free(names);
        mem_free(vers);
        mem_free(vols);
        mem_free(vals);
    }

    str_cpy(v->names[v->len], var);
    v->vers[v->len] = ver;
    v->vals[v->len] = val;
    v->len++;
}

extern int vars_scan(Vars *v, const char *var, long long ver)
{
    int idx = -1;
    for (int i = 0; i < v->len && idx < 0; ++i)
        if (str_cmp(var, v->names[i]) == 0 && ver == v->vers[i])
            idx = i;

    return idx;
}
