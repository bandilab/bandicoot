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
#include "system.h"
#include "memory.h"
#include "string.h"
#include "head.h"
#include "volume.h"
#include "value.h"
#include "tuple.h"
#include "expression.h"
#include "summary.h"
#include "relation.h"

typedef struct {
    Rel *left;
    Rel *right;

    /* load, param */
    char name[MAX_NAME];

    /* join, union, diff, project, binary summarize */
    struct {
        int len;
        int lpos[MAX_ATTRS];
        int rpos[MAX_ATTRS];
    } e, j;

    /* rename */
    int acnt;
    int apos[MAX_ATTRS];

    /* select, extend */
    int ecnt;
    Expr *exprs[MAX_ATTRS];

    /* unary & binary summarize */
    int scnt;
    Sum *sums[MAX_ATTRS];

    /* temp */
    int ccnt;
    Rel *clones[MAX_RVARS];
} Ctxt;

static void free(Rel *r)
{
    Ctxt *c = r->ctxt;
    if (c->left != NULL)
        rel_free(c->left);
    if (c->right != NULL)
        rel_free(c->right);

    for (int i = 0; i < c->ecnt; ++i)
        expr_free(c->exprs[i]);
    for (int i = 0; i < c->scnt; ++i)
        mem_free(c->sums[i]);
}

static Rel *alloc(void (*init)(Rel *r, Args *a))
{
    Rel *r = mem_alloc(sizeof(Rel) + sizeof(Ctxt));
    r->head = NULL;
    r->body = NULL;
    r->ctxt = r + 1;
    r->init = init;
    r->free = free;

    Ctxt *c = r->ctxt;
    c->left = NULL;
    c->right = NULL;
    c->name[0] = '\0';
    c->e.len = 0;
    c->j.len = 0;
    c->acnt = 0;
    c->ecnt = 0;
    c->scnt = 0;
    c->ccnt = 0;

    return r;
}

static void init_load(Rel *r, Args *args)
{
    Ctxt *c = r->ctxt;
    int pos = array_scan(args->names, args->len, c->name);
    int fd = vol_open(c->name, args->vers[pos], READ);
    r->body = tbuf_read(fd);
    sys_close(fd);
}

extern Rel *rel_load(Head *head, const char *name)
{
    Rel *res = alloc(init_load);
    res->head = head;

    Ctxt *c = res->ctxt;
    str_cpy(c->name, name);

    return res;
}

static void init_param(Rel *r, Args *args)
{
    Ctxt *c = r->ctxt;
    int pos = array_scan(args->names, args->len, c->name);
    r->body = args->tbufs[pos];
}

extern Rel *rel_param(Head *head, const char *name)
{
    Rel *res = alloc(init_param);
    res->head = head; /* FIXME: we do head_cpy everywhere but in
                                rel_param & rel_load <- inconsisten */

    Ctxt *c = res->ctxt;
    str_cpy(c->name, name);

    return rel_project(res, head->names, head->len);
}

static void init_join(Rel *r, Args *args)
{
    Ctxt *c = r->ctxt;
    r->body = tbuf_new();

    rel_init(c->left, args);
    rel_init(c->right, args);
    TBuf *rb = c->right->body;

    Tuple *lt, *rt;
    while ((lt = rel_next(c->left)) != NULL) {
        tbuf_reset(rb);
        while ((rt = tbuf_next(rb)) != NULL)
            if (tuple_eq(lt, rt, c->e.lpos, c->e.rpos, c->e.len)) {
                Tuple *t = tuple_join(lt, rt, c->j.lpos, c->j.rpos, c->j.len);
                tbuf_add(r->body, t);
            }

        tuple_free(lt);
    }

    tbuf_clean(rb);
}

extern Rel *rel_join(Rel *l, Rel *r)
{
    Rel *res = alloc(init_join);

    Ctxt *c = res->ctxt;
    res->head = head_join(l->head, r->head, c->j.lpos, c->j.rpos, &c->j.len);
    c->e.len = head_common(l->head, r->head, c->e.lpos, c->e.rpos);
    c->left = l;
    c->right = r;

    return res;
}

static int contains(TBuf *b, Tuple *t, int bpos[], int tpos[], int len)
{
    int match = 0;
    Tuple *bt;

    tbuf_reset(b);
    while ((bt = tbuf_next(b)) != NULL && !match)
        match = tuple_eq(bt, t, bpos, tpos, len);

    return match;
}

static void init_union(Rel *r, Args *args)
{
    Ctxt *c = r->ctxt;
    r->body = tbuf_new();

    rel_init(c->left, args);
    rel_init(c->right, args);
    TBuf *rb = c->right->body;

    Tuple *lt, *rt;
    while ((lt = rel_next(c->left)) != NULL)
        if (contains(rb, lt, c->e.rpos, c->e.lpos, c->e.len))
            tuple_free(lt);
        else
            tbuf_add(r->body, lt);

    tbuf_reset(rb);
    while ((rt = tbuf_next(rb)) != NULL)
        tbuf_add(r->body, rt);
}

extern Rel *rel_union(Rel *l, Rel *r)
{
    Rel *res = alloc(init_union);
    res->head = head_cpy(l->head);

    Ctxt *c = res->ctxt;
    c->e.len = head_common(l->head, r->head, c->e.lpos, c->e.rpos);
    c->left = l;
    c->right = r;

    return res;
}

static void init_diff(Rel *r, Args *args)
{
    Ctxt *c = r->ctxt;
    r->body = tbuf_new();

    rel_init(c->left, args);
    rel_init(c->right, args);
    TBuf *rb = c->right->body;

    Tuple *lt;
    while ((lt = rel_next(c->left)) != NULL)
        if (contains(rb, lt, c->e.rpos, c->e.lpos, c->e.len))
            tuple_free(lt);
        else
            tbuf_add(r->body, lt);

    tbuf_clean(rb);
}

extern Rel *rel_diff(Rel *l, Rel *r)
{
    Rel *res = alloc(init_diff);
    res->head = head_cpy(l->head);

    Ctxt *c = res->ctxt;
    c->e.len = head_common(l->head, r->head, c->e.lpos, c->e.rpos);
    c->left = l;
    c->right = r;

    return res;
}

static void init_project(Rel *r, Args *args)
{
    Ctxt *c = r->ctxt;
    r->body = tbuf_new();

    rel_init(c->left, args);

    Tuple *t;
    while ((t = rel_next(c->left)) != NULL) {
        if (!contains(r->body, t, c->e.rpos, c->e.lpos, c->e.len))
            tbuf_add(r->body, tuple_reord(t, c->e.lpos, c->e.len));

        tuple_free(t);
    }
}

extern Rel *rel_project(Rel *r, char *names[], int len)
{
    Rel *res = alloc(init_project);
    res->head = head_project(r->head, names, len);

    Ctxt *c = res->ctxt;
    c->left = r;
    c->e.len = head_common(r->head, res->head, c->e.lpos, c->e.rpos);

    return res;
}

static void init_rename(Rel *r, Args *args)
{
    Ctxt *c = r->ctxt;
    r->body = tbuf_new();

    rel_init(c->left, args);

    Tuple *t;
    while ((t = rel_next(c->left)) != NULL) {
        tbuf_add(r->body, tuple_reord(t, c->apos, c->acnt));
        tuple_free(t);
    }
}

extern Rel *rel_rename(Rel *r, char *from[], char *to[], int len)
{
    Rel *res = alloc(init_rename);

    Ctxt *c = res->ctxt;
    res->head = head_rename(r->head, from, to, len, c->apos, &c->acnt);
    c->left = r;

    return res;
}

static void init_select(Rel *r, Args *args)
{
    Ctxt *c = r->ctxt;
    r->body = tbuf_new();

    rel_init(c->left, args);

    Tuple *t;
    while ((t = rel_next(c->left)) != NULL)
        if (expr_bool_val(c->exprs[0], t))
            tbuf_add(r->body, t);
        else
            tuple_free(t);
}

extern Rel *rel_select(Rel *r, Expr *bool_expr)
{
    Rel *res = alloc(init_select);
    res->head = head_cpy(r->head);

    Ctxt *c = res->ctxt;
    c->left = r;
    c->ecnt = 1;
    c->exprs[0] = bool_expr;

    return res;
}

static void init_extend(Rel *r, Args *args)
{
    Ctxt *c = r->ctxt;
    r->body = tbuf_new();

    rel_init(c->left, args);

    Tuple *t;
    while ((t = rel_next(c->left)) != NULL) {
        Value vals[c->ecnt];
        for (int i = 0; i < c->ecnt; ++i)
            vals[i] = expr_new_val(c->exprs[i], t);

        Tuple *e = tuple_new(vals, c->ecnt);
        Tuple *res = tuple_join(t, e, c->j.lpos, c->j.rpos, c->j.len);
        tbuf_add(r->body, res);

        tuple_free(e);
        tuple_free(t);
    }
}

extern Rel *rel_extend(Rel *r, char *names[], Expr *e[], int len)
{
    Rel *res = alloc(init_extend);
    Ctxt *c = res->ctxt;
    c->left = r;
    c->ecnt = len;
    
    Type types[len];
    int map[len];
    array_sort(names, len, map);
    for (int i = 0; i < len; ++i) {
        c->exprs[i] = e[map[i]];
        types[i] = e[map[i]]->type;
    }
    Head *h = head_new(names, types, len);
    res->head = head_join(r->head, h, c->j.lpos, c->j.rpos, &c->j.len);
    mem_free(h);

    return res;
}

static void init_sum(Rel *r, Args *args)
{
    Ctxt *c = r->ctxt;
    r->body = tbuf_new();

    rel_init(c->left, args);
    rel_init(c->right, args);

    int scnt = c->scnt;

    TBuf *lb = c->left->body;
    Tuple *lt, *rt;
    while ((rt = rel_next(c->right)) != NULL) {
        for (int i = 0; i < scnt; ++i)
            sum_reset(c->sums[i]);

        tbuf_reset(lb);
        while ((lt = tbuf_next(lb)) != NULL)
            if (tuple_eq(lt, rt, c->e.lpos, c->e.rpos, c->e.len))
                for (int i = 0; i < scnt; ++i)
                    sum_update(c->sums[i], lt);

        Value vals[scnt];
        for (int i = 0; i < scnt; ++i)
            vals[i] = sum_value(c->sums[i]);

        Tuple *st = tuple_new(vals, scnt);
        Tuple *res = tuple_join(rt, st, c->j.lpos, c->j.rpos, c->j.len);

        tbuf_add(r->body, res);

        tuple_free(st);
        tuple_free(rt);
    }

    tbuf_clean(lb);
}

extern Rel *rel_sum(Rel *r,
                    Rel *per,
                    char *names[],
                    Type types[],
                    Sum *sums[],
                    int len)
{
    Rel *res = alloc(init_sum);

    Ctxt *c = res->ctxt;
    c->e.len = head_common(r->head, per->head, c->e.lpos, c->e.rpos);
    c->scnt = len;
    c->left = r;
    c->right = per;

    Type stypes[len];
    int map[len];
    array_sort(names, len, map);
    for (int i = 0; i < len; ++i) {
        c->sums[i] = sums[map[i]];
        stypes[i] = types[map[i]];
    }
    Head *h = head_new(names, stypes, len);

    res->head = head_join(per->head, h, c->j.lpos, c->j.rpos, &c->j.len);
    mem_free(h);

    return res;
}

static void init_sum_unary(Rel *r, Args *args)
{
    Ctxt *c = r->ctxt;
    r->body = tbuf_new();

    rel_init(c->left, args);

    for (int i = 0; i < c->scnt; ++i)
        sum_reset(c->sums[i]);

    Tuple *t;
    while ((t = rel_next(c->left)) != NULL) {
        for (int i = 0; i < c->scnt; ++i)
            sum_update(c->sums[i], t);

        tuple_free(t);
    }

    Value vals[c->scnt];
    for (int i = 0; i < c->scnt; ++i)
        vals[i] = sum_value(c->sums[i]);

    tbuf_add(r->body, tuple_new(vals, c->scnt));
}

extern Rel *rel_sum_unary(Rel *r,
                          char *names[],
                          Type types[],
                          Sum *sums[],
                          int len)
{
    Rel *res = alloc(init_sum_unary);

    Ctxt *c = res->ctxt;
    c->scnt = len;
    c->left = r;

    Type stypes[len];
    int map[len];
    array_sort(names, len, map);
    for (int i = 0; i < len; ++i) {
        c->sums[i] = sums[map[i]];
        stypes[i] = types[map[i]];
    }
    res->head = head_new(names, stypes, len);

    return res;
}

extern int rel_store(const char *name, long vers, Rel *r)
{
    int res = 0, fd = vol_open(name, vers, CREATE | WRITE);
    tbuf_write(r->body, fd);
    sys_close(fd);

    return res;
}

extern int rel_eq(Rel *l, Rel *r)
{
    int rcnt = 0, all_ok = 1;

    if (!head_eq(l->head, r->head))
        return 0;

    int lpos[MAX_ATTRS], rpos[MAX_ATTRS];
    int len = head_common(l->head, r->head, lpos, rpos);

    Tuple *rt;
    while (all_ok && (rt = rel_next(r)) != NULL) {
        all_ok = all_ok && contains(l->body, rt, lpos, rpos, len);
        tuple_free(rt);
        rcnt++;
    }

    all_ok = all_ok && (l->body->len == rcnt);

    while ((rt = rel_next(r)) != NULL)
        tuple_free(rt);

    tbuf_clean(l->body);

    return all_ok;
}

static void init_tmp(Rel *r, Args *args)
{
    Ctxt *c = r->ctxt;
    rel_init(c->left, args);

    for (int i = 0; i < c->ccnt; ++i)
        c->clones[i]->body = tbuf_new();

    Tuple *t;
    while ((t = rel_next(c->left)) != NULL) {
        for (int i = 0; i < c->ccnt; ++i)
            tbuf_add(c->clones[i]->body, tuple_cpy(t));

        tuple_free(t);
    }
}

static void init_clone(Rel *r, Args *args) {}

extern Rel *rel_tmp(Rel *r, Rel *clones[], int cnt)
{
    Rel *res = alloc(init_tmp);
    res->head = head_cpy(r->head);

    Ctxt *c = res->ctxt;
    c->left = r;
    c->ccnt = cnt;

    for (int i = 0; i < cnt; ++i) {
        Rel *tmp = alloc(init_clone);
        tmp->head = head_cpy(r->head);
        c->clones[i] = clones[i] = tmp;
    }

    return res;
}
