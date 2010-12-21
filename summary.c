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
#include "memory.h"
#include "string.h"
#include "head.h"
#include "value.h"
#include "tuple.h"
#include "summary.h"

static Sum *alloc(int size, int pos, Type t, Value def) {
    Sum *res = mem_alloc(size);
    res->type = t;
    res->pos = pos;
    res->ctxt = res + 1;

    if (def.size != 0) {
        if (t == Int)
            res->def.i = val_int(def);
        else if (t == Real)
            res->def.d = val_real(def);
        else
            res->def.l = val_long(def);
    }

    return res;
}

static void cnt_reset(Sum *s)
{
    s->def.i = s->res.i = s->cnt = 0;
}

static void cnt_update(Sum *s, Tuple *t)
{
    (s->cnt)++;
    s->def.i = s->res.i = s->cnt;
}

extern Sum *sum_cnt()
{
    Value v = {.size = 0, .data = NULL};
    Sum *res = alloc(sizeof(Sum), 0, Int, v);
    res->reset = cnt_reset;
    res->update = cnt_update;

    return res;
}

static void mm_reset(Sum *s)
{
    s->cnt = 0;
}

static void min_update(Sum *s, Tuple *t)
{
    Value v = tuple_attr(t, s->pos);

    if (s->type == Int) {
        int m = val_int(v);
        if (s->cnt == 0 || s->res.i > m) 
            s->res.i = m;
    } else if (s->type == Real) {
        double m = val_real(v);
        if (s->cnt == 0 || s->res.d > m) 
            s->res.d = m;
    } else {
        long long m = val_long(v);
        if (s->cnt == 0 || s->res.l > m) 
            s->res.l = m;
    }

    s->cnt++;
}

static void max_update(Sum *s, Tuple *t)
{
    Value v = tuple_attr(t, s->pos);

    if (s->type == Int) {
        int m = val_int(v);
        if (s->cnt == 0 || s->res.i < m) 
            s->res.i = m;
    } else if (s->type == Real) {
        double m = val_real(v);
        if (s->cnt == 0 || s->res.d < m) 
            s->res.d = m;
    } else {
        long long m = val_long(v);
        if (s->cnt == 0 || s->res.l < m) 
            s->res.l = m;
    }

    s->cnt++;
}

static Sum *sum_mm_reset(int pos, Type t, Value def)
{
    Sum *res = alloc(sizeof(Sum), pos, t, def);
    res->reset = mm_reset;

    return res;
}

extern Sum *sum_min(int pos, Type t, Value def)
{
    Sum *res = sum_mm_reset(pos, t, def);
    res->update = min_update;

    return res;
}

extern Sum *sum_max(int pos, Type t, Value def)
{
    Sum *res = sum_mm_reset(pos, t, def);
    res->update = max_update;

    return res;
}

typedef struct {
    Type type;
    union {
        int i;
        double d;
        long long l;
    } sum;
} C_Avg;

static void avg_reset(Sum *s)
{
    C_Avg *c = s->ctxt;
    c->sum.i = 0;
    c->sum.d = 0.0;
    c->sum.l = 0;

    s->cnt = 0;
    s->res.d = s->def.d;
}

static void avg_update(Sum *s, Tuple *t)
{
    C_Avg *c = s->ctxt;
    Value v = tuple_attr(t, s->pos);

    s->cnt++;
    
    if (c->type == Int) {
        c->sum.i += val_int(v);
        s->res.d = (double) c->sum.i / s->cnt;
    } else if (c->type == Real) {
        c->sum.d += val_real(v);
        s->res.d = c->sum.d / s->cnt;
    } else {
        c->sum.l += val_long(v);
        s->res.d = (double) c->sum.l / s->cnt;
    }
}

extern Sum *sum_avg(int pos, Type t, Value def)
{
    Sum *res = alloc(sizeof(Sum) + sizeof(C_Avg), pos, Real, def);
    res->reset = avg_reset;
    res->update = avg_update;

    C_Avg *c = res->ctxt;
    c->type = t;

    return res;
}

static void add_reset(Sum *s)
{
    s->res.i = 0;
    s->res.d = 0.0;
    s->res.l = 0;

    s->cnt = 0;
}

static void add_update(Sum *s, Tuple *t)
{
    Value v = tuple_attr(t, s->pos);
    
    if (s->type == Int)
        s->res.i += val_int(v);
    else if (s->type == Real)
        s->res.d += val_real(v);
    else
        s->res.l += val_long(v);

    s->cnt++;
}

extern Sum *sum_add(int pos, Type t, Value def)
{
    Sum *res = alloc(sizeof(Sum), pos, t, def);
    res->reset = add_reset;
    res->update = add_update;

    return res;
}

extern Value sum_value(Sum *s) {
    Type t = s->type;
    if (t == Int)
        return s->cnt == 0 ? val_new_int(&(s->def.i)) : val_new_int(&(s->res.i));
    else if (t == Real)
        return s->cnt == 0 ? val_new_real(&(s->def.d)) : val_new_real(&(s->res.d));
    else
        return s->cnt == 0 ? val_new_long(&(s->def.l)) : val_new_long(&(s->res.l));
}

