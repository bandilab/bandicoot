/*
Copyright 2008-2011 Ostap Cherkashin
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
#include "system.h"
#include "memory.h"
#include "string.h"
#include "head.h"
#include "value.h"
#include "tuple.h"
#include "volume.h"
#include "expression.h"
#include "summary.h"
#include "relation.h"
#include "index.h"

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
        v->vers[i] = 0L;
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

    vars_init(res);

    return res;
}

extern void vars_cpy(Vars *dest, Vars *src)
{
    dest->len = 0;
    for (int i = 0; i < src->len; ++i) {
        vars_put(dest, src->names[i], src->vers[i]);
        str_cpy(dest->vols[i], src->vols[i]);
    }
}

extern void vars_free(Vars *v)
{
    mem_free(v->names);
    mem_free(v->vers);
    mem_free(v->vols);
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

extern void vars_put(Vars *v, const char *var, long long ver)
{
    if (v->len == v->size) {
        char **names = v->names;
        char **vols = v->vols;
        long long *vers = v->vers;

        v->size += MAX_VARS;
        v->names = mem_alloc(v->size * (sizeof(void*) + MAX_NAME));
        v->vols = mem_alloc(v->size * (sizeof(void*) + MAX_ADDR));
        v->vers = mem_alloc(v->size * sizeof(long long));

        vars_init(v);

        for (int i = 0; i < v->len; ++i) {
            str_cpy(v->names[i], names[i]);
            str_cpy(v->vols[i], vols[i]);
            v->vers[i] = vers[i];
        }
        mem_free(names);
        mem_free(vers);
        mem_free(vols);
    }

    str_cpy(v->names[v->len], var);
    v->vers[v->len] = ver;
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

typedef struct {
    Rel *left;
    Rel *right;

    /* load */
    char name[MAX_NAME];

    /* join, union, diff, project, binary sum */
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

    /* unary & binary sum */
    int scnt;
    Sum *sums[MAX_ATTRS];

    /* temp */
    int ccnt;
    Rel *clones[MAX_VARS];

    /* error response */
    int code;
    char *msg;
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

static Rel *alloc(void (*init)(Rel *r, Vars *s, Arg *a))
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

static void init_noop(Rel *r, Vars *rvars, Arg *arg)
{
}

static void init_load(Rel *r, Vars *rvars, Arg *arg)
{
    Ctxt *c = r->ctxt;

    int pos = array_scan(rvars->names, rvars->len, c->name);
    r->body = vol_read(rvars->vols[pos], c->name, rvars->vers[pos]);
}

extern Rel *rel_load(Head *head, const char *name)
{
    Rel *res = alloc(init_load);
    res->head = head_cpy(head);

    Ctxt *c = res->ctxt;
    str_cpy(c->name, name);

    return res;
}

extern Rel *rel_err(int code, char *msg)
{
    Rel *res = alloc(init_noop);

    char *names[] = {"code", "msg"};
    Type types[] = {Int, String};
    res->head = head_new(names, types, 2);

    Ctxt *c = res->ctxt;
    c->code = code;
    c->msg = msg;

    Value vals[2] = { val_new_int(&c->code), val_new_str(c->msg) };
    Tuple *t = tuple_new(vals, 2);
    res->body = tbuf_new();
    tbuf_add(res->body, t);

    return res;
}

static void init_param(Rel *r, Vars *rvars, Arg *arg)
{
    r->body = arg->body;
}

extern Rel *rel_param(Head *head)
{
    Rel *res = alloc(init_param);
    res->head = head_cpy(head);

    return rel_project(res, head->names, head->len);
}

static void init_join(Rel *r, Vars *rvars, Arg *arg)
{
    Ctxt *c = r->ctxt;
    r->body = tbuf_new();

    rel_init(c->left, rvars, arg);
    rel_init(c->right, rvars, arg);

    TBuf *lb = c->left->body;
    index_sort(lb, c->e.lpos, c->e.len);

    Tuple *lt, *rt;
    while ((rt = rel_next(c->right)) != NULL) {
        TBuf *m = index_match(lb, rt, c->e.lpos, c->e.rpos, c->e.len);

        if (m != NULL) {
            while ((lt = tbuf_next(m)) != NULL) {
                Tuple *t = tuple_join(lt, rt, c->j.lpos, c->j.rpos, c->j.len);
                tbuf_add(r->body, t);
            }

            tbuf_free(m);
        }

        tuple_free(rt);
    }

    tbuf_clean(lb);
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

static void init_union(Rel *r, Vars *rvars, Arg *arg)
{
    Ctxt *c = r->ctxt;
    r->body = tbuf_new();

    rel_init(c->left, rvars, arg);
    rel_init(c->right, rvars, arg);

    TBuf *rb = c->right->body;
    index_sort(rb, c->e.rpos, c->e.len);

    Tuple *lt, *rt;
    while ((lt = rel_next(c->left)) != NULL)
        if (index_has(rb, lt, c->e.rpos, c->e.lpos, c->e.len))
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

static void init_diff(Rel *r, Vars *rvars, Arg *arg)
{
    Ctxt *c = r->ctxt;
    r->body = tbuf_new();

    rel_init(c->left, rvars, arg);
    rel_init(c->right, rvars, arg);

    TBuf *rb = c->right->body;
    index_sort(rb, c->e.rpos, c->e.len);

    Tuple *lt;
    while ((lt = rel_next(c->left)) != NULL)
        if (index_has(rb, lt, c->e.rpos, c->e.lpos, c->e.len))
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

static void init_project(Rel *r, Vars *rvars, Arg *arg)
{
    Ctxt *c = r->ctxt;
    r->body = tbuf_new();

    rel_init(c->left, rvars, arg);
    index_sort(c->left->body, c->e.lpos, c->e.len);

    Tuple *t;
    while ((t = rel_next(c->left)) != NULL) {
        if (!index_has(r->body, t, c->e.rpos, c->e.lpos, c->e.len))
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

static void init_rename(Rel *r, Vars *rvars, Arg *arg)
{
    Ctxt *c = r->ctxt;
    r->body = tbuf_new();

    rel_init(c->left, rvars, arg);

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

static void init_select(Rel *r, Vars *rvars, Arg *arg)
{
    Ctxt *c = r->ctxt;
    r->body = tbuf_new();

    rel_init(c->left, rvars, arg);

    Tuple *t;
    while ((t = rel_next(c->left)) != NULL)
        if (expr_bool_val(c->exprs[0], t, arg))
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

static void init_extend(Rel *r, Vars *rvars, Arg *arg)
{
    Ctxt *c = r->ctxt;
    r->body = tbuf_new();

    rel_init(c->left, rvars, arg);

    Tuple *t;
    while ((t = rel_next(c->left)) != NULL) {
        Value vals[c->ecnt];
        for (int i = 0; i < c->ecnt; ++i)
            vals[i] = expr_new_val(c->exprs[i], t, arg);

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

static void init_sum(Rel *r, Vars *rvars, Arg *arg)
{
    Ctxt *c = r->ctxt;
    r->body = tbuf_new();

    rel_init(c->left, rvars, arg);
    rel_init(c->right, rvars, arg);

    int scnt = c->scnt;

    TBuf *lb = c->left->body;
    index_sort(lb, c->e.lpos, c->e.len);

    Tuple *lt, *rt;
    while ((rt = rel_next(c->right)) != NULL) {
        for (int i = 0; i < scnt; ++i)
            sum_reset(c->sums[i]);

        TBuf *m = index_match(lb, rt, c->e.lpos, c->e.rpos, c->e.len);
        if (m != NULL) {
            while ((lt = tbuf_next(m)) != NULL)
                for (int i = 0; i < scnt; ++i)
                    sum_update(c->sums[i], lt);

            tbuf_free(m);
        }

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

static void init_sum_unary(Rel *r, Vars *rvars, Arg *arg)
{
    Ctxt *c = r->ctxt;
    r->body = tbuf_new();

    rel_init(c->left, rvars, arg);

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

extern void rel_store(const char *vid,
                      const char *name,
                      long long vers,
                      Rel *r)
{
    vol_write(vid, r->body, name, vers);
}

extern int rel_eq(Rel *l, Rel *r)
{
    int rcnt = 0, all_ok = 1;

    if (!head_eq(l->head, r->head))
        return 0;

    int lpos[MAX_ATTRS], rpos[MAX_ATTRS];
    int len = head_common(l->head, r->head, lpos, rpos);

    index_sort(l->body, lpos, len);

    Tuple *rt;
    while (all_ok && (rt = rel_next(r)) != NULL) {
        all_ok = all_ok && index_has(l->body, rt, lpos, rpos, len);
        tuple_free(rt);
        rcnt++;
    }

    all_ok = all_ok && (l->body->len == rcnt);

    while ((rt = rel_next(r)) != NULL)
        tuple_free(rt);

    tbuf_clean(l->body);

    return all_ok;
}

static void free_clone(Rel *r) { }

static void init_clone(Rel *r, Vars *rvars, Arg *arg) {
    Ctxt *c = r->ctxt;
    r->body = tbuf_new();

    Tuple *t;
    while ((t = rel_next(c->left)) != NULL)
        tbuf_add(r->body, tuple_cpy(t));

    tbuf_reset(c->left->body);
}

extern Rel *rel_clone(Rel *r) {
    Rel *res = alloc(init_clone);
    res->head = head_cpy(r->head);
    res->free = free_clone;

    Ctxt *c = res->ctxt;
    c->left = r;

    return res;
}
