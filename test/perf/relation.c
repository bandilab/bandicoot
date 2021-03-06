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

#include "../common.h"

static void perf_load_store(Env *env, int count)
{
    Vars *wvars = vars_new(1), *rvars = vars_new(1), *evars = vars_new(0);
    vars_add(rvars, "perf_rel", 0, NULL);
    vars_add(wvars, "perf_rel", 0, NULL);

    Rel *r = gen_rel(0, count);

    long long sid = tx_enter("", evars, wvars);
    rel_eval(r, NULL, NULL);

    long long time = sys_millis();
    vol_write(wvars->vols[0], r->body, "perf_rel", sid);
    sys_print("%8s %d tuples in %lldms\n", "write", count, sys_millis() - time);

    rel_free(r);
    tx_commit(sid);

    sid = tx_enter("", rvars, evars);

    time = sys_millis();
    TBuf *body = vol_read(rvars->vols[0], "perf_rel", rvars->vers[0]);
    Tuple *t;
    int i = 0;
    while ((t = tbuf_next(body)) != NULL) {
        tuple_free(t);
        i++;
    }
    sys_print("%8s %d tuples in %lldms\n", "read", i, sys_millis() - time);

    tx_commit(sid);

    vars_free(wvars);
    vars_free(rvars);
    vars_free(evars);
}

static void perf_join(int count)
{
    Rel *join = rel_join(gen_rel(0, count), gen_rel(-count / 2, count / 2));

    long long time = sys_millis();
    rel_eval(join, NULL, NULL);

    Tuple *t;
    int res = 0;
    while ((t = tbuf_next(join->body)) != NULL) {
        tuple_free(t);
        res++;
    }

    sys_print("%8s %dx%d tuples in %lldms\n",
              "join",
              count,
              count,
              sys_millis() - time);

    if (res != count / 2)
        fail();

    rel_free(join);
}

static void perf_project(int count)
{
    char *names[] = {"a"};
    Rel *rel = rel_project(gen_rel(0, count), names, 1);

    long long time = sys_millis();
    rel_eval(rel, NULL, NULL);
    Tuple *t;
    int i = 0;
    while ((t = tbuf_next(rel->body)) != NULL) {
        tuple_free(t);
        i++;
    }

    sys_print("%8s %i tuples from %d in %lldms\n",
              "project",
              i,
              count,
              sys_millis() - time);

    rel_free(rel);
}

static void perf_select(int count)
{
    Rel *rel = gen_rel(0, count);

    int a, c;
    Type at, ct;
    head_attr(rel->head, "a", &a, &at);
    head_attr(rel->head, "c", &c, &ct);

    Expr *e = expr_and(expr_gt(expr_attr(a, at),
                                expr_int(764)),
                       expr_eq(expr_str("hello_from_gen765"),
                               expr_attr(c, ct)));

    rel = rel_select(rel, e);

    long long time = sys_millis();
    rel_eval(rel, NULL, NULL);
    Tuple *t;
    int i = 0;
    while ((t = tbuf_next(rel->body)) != NULL) {
        tuple_free(t);
        i++;
    }

    sys_print("%8s %d tuples from %d in %lldms\n",
              "select",
              i,
              count,
              sys_millis() - time);

    rel_free(rel);
}

static void perf_diff(int count)
{
    Rel *rel = rel_diff(gen_rel(0, count),
                        gen_rel(-count / 2, count / 2));

    long long time = sys_millis();
    rel_eval(rel, NULL, NULL);
    Tuple *t;
    int i = 0;
    while ((t = tbuf_next(rel->body)) != NULL) {
        tuple_free(t);
        i++;
    }

    sys_print("%8s %dx%d tuples (res=%d tuples) in %lldms\n",
              "semidiff",
              count,
              count,
              i,
              sys_millis() - time);

    rel_free(rel);
}

static void perf_union(int count)
{
    Rel *rel = rel_union(gen_rel(0, count),
                         gen_rel(-count / 2, count));

    long long time = sys_millis();
    rel_eval(rel, NULL, NULL);
    Tuple *t;
    int i = 0;
    while ((t = tbuf_next(rel->body)) != NULL) {
        tuple_free(t);
        i++;
    }

    sys_print("%8s %dx%d tuples (res=%d tuples) in %lldms\n",
              "union",
              count,
              count,
              i,
              sys_millis() - time);

    rel_free(rel);
}

static void perf_extend(int count)
{
    Rel *rel = gen_rel(0, count);

    int a;
    Type at;
    head_attr(rel->head, "a", &a, &at);

    char *names[] = {"d", "e"};
    Expr *e[] = { expr_sum(expr_div(expr_attr(a, at),
                                    expr_int(3)),
                           expr_int(count)),
                  expr_mul(expr_attr(a, Int),
                           expr_int(57)) };

    rel = rel_extend(rel, names, e, 2);

    long long time = sys_millis();
    rel_eval(rel, NULL, NULL);
    Tuple *t;
    while ((t = tbuf_next(rel->body)) != NULL)
        tuple_free(t);

    sys_print("%8s %d tuples in %lldms\n",
              "extend",
              count,
              sys_millis() - time);

    rel_free(rel);
}

static void perf_sum(int count)
{
    int v = -1;
    char *names[] = {"d", "e", "f"};
    Type types[] =  {Int, Int, Int};

    Rel *rel = gen_rel(0, count);
    int pos;
    Type at;
    head_attr(rel->head, "a", &pos, &at);
    Sum *sums[] = {sum_cnt(),
                   sum_min(pos, Int, val_new_int(&v)),
                   sum_max(pos, Int, val_new_int(&v))};

    rel = rel_sum(rel, gen_rel(0, count), names, types, sums, 3);

    long long time = sys_millis();
    rel_eval(rel, NULL, NULL);
    Tuple *t;
    while ((t = tbuf_next(rel->body)) != NULL)
        tuple_free(t);

    sys_print("%8s %dx%d tuples in %lldms\n",
              "sum",
              count,
              count,
              sys_millis() - time);

    rel_free(rel);
}

static void perf_eq(int count)
{
    Rel *l = gen_rel(0, count);
    Rel *r = gen_rel(0, count);

    rel_eval(l, NULL, NULL);
    rel_eval(r, NULL, NULL);

    long long time = sys_millis();

    rel_eq(l, r);

    sys_print("%8s %dx%d tuples in %lldms\n",
              "eq",
              count,
              count,
              sys_millis() - time);

    rel_free(l);
    rel_free(r);
}

int main()
{
    sys_init(0);

    int tx_port = 0;
    char *source = "test/test_defs.b";
    tx_server(source, "bin/state", &tx_port);
    vol_init(0, "bin/volume");

    char *code = tx_program();
    Env *env = env_new(source, code);
    mem_free(code);

    perf_load_store(env, 1 * 1000);
    perf_load_store(env, 10 * 1000);
    perf_load_store(env, 100 * 1000);
    perf_load_store(env, 1000 * 1000);
    perf_join(1 * 1000);
    perf_join(10 * 1000);
    perf_join(100 * 1000);
    perf_join(1000 * 1000);
    perf_diff(1 * 1000);
    perf_diff(10 * 1000);
    perf_diff(100 * 1000);
    perf_diff(1000 * 1000);
    perf_project(1 * 1000);
    perf_project(10 * 1000);
    perf_project(100 * 1000);
    perf_project(1000 * 1000);
    perf_select(1 * 1000);
    perf_select(10 * 1000);
    perf_select(100 * 1000);
    perf_select(1000 * 1000);
    perf_union(1 * 1000);
    perf_union(10 * 1000);
    perf_union(100 * 1000);
    perf_union(1000 * 1000);
    perf_extend(1 * 1000);
    perf_extend(10 * 1000);
    perf_extend(100 * 1000);
    perf_extend(1000 * 1000);
    perf_sum(1 * 1000);
    perf_sum(10 * 1000);
    perf_sum(100 * 1000);
    perf_sum(1000 * 1000);
    perf_eq(1 * 1000);
    perf_eq(10 * 1000);
    perf_eq(100 * 1000);
    perf_eq(1000 * 1000);

    env_free(env);
    tx_free();

    return 0;
}
