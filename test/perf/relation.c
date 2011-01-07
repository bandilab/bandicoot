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

#include "../common.h"

static void perf_load_store(Env *env, int count)
{
    Args args = {.len = 1, .names[0] = "perf_rel"};

    char *names[] = { "perf_rel" };
    long vers[1];

    Rel *r = gen_rel(0, count);

    long sid = tx_enter(NULL, NULL, 0, names, vers, 1);
    rel_init(r, NULL);

    long time = sys_millis();
    rel_store("perf_rel", vers[0], r);
    sys_print("%8s %d tuples in %dms\n", "write", count, sys_millis() - time);

    rel_free(r);
    tx_commit(sid);

    Head *h = env_head(env, "perf_rel");
    r = rel_load(h, "perf_rel");
    sid = tx_enter(args.names, args.vers, args.len, NULL, NULL, 0);

    time = sys_millis();
    rel_init(r, &args);
    int i = 0;
    Tuple *t;
    while ((t = rel_next(r)) != NULL) {
        i++;
        tuple_free(t);
    }
    sys_print("%8s %d tuples in %dms\n", "read", i, sys_millis() - time);

    rel_free(r);
    tx_commit(sid);
}

static void perf_join(int count)
{
    Rel *join = rel_join(gen_rel(0, count), gen_rel(-count / 2, count / 2));

    long time = sys_millis();
    rel_init(join, NULL);

    Tuple *t;
    int res = 0;
    while ((t = rel_next(join)) != 0) {
        tuple_free(t);
        res++;
    }

    sys_print("%8s %dx%d tuples in %dms\n",
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

    long time = sys_millis();
    rel_init(rel, NULL);
    Tuple *t;
    int i = 0;
    while ((t = rel_next(rel)) != 0) {
        tuple_free(t);
        i++;
    }

    sys_print("%8s %i tuples from %d in %dms\n",
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

    long time = sys_millis();
    rel_init(rel, NULL);
    Tuple *t;
    int i = 0;
    while ((t = rel_next(rel)) != 0) {
        tuple_free(t);
        i++;
    }

    sys_print("%8s %d tuples from %d in %dms\n",
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

    long time = sys_millis();
    rel_init(rel, NULL);
    Tuple *t;
    int i = 0;
    while ((t = rel_next(rel)) != 0) {
        tuple_free(t);
        i++;
    }

    sys_print("%8s %dx%d tuples (res=%d tuples) in %dms\n",
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

    long time = sys_millis();
    rel_init(rel, NULL);
    Tuple *t;
    int i = 0;
    while ((t = rel_next(rel)) != 0) {
        tuple_free(t);
        i++;
    }

    sys_print("%8s %dx%d tuples (res=%d tuples) in %dms\n",
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

    long time = sys_millis();
    rel_init(rel, NULL);
    Tuple *t;
    int i = 0;
    while ((t = rel_next(rel)) != 0) {
        tuple_free(t);
        i++;
    }

    sys_print("%8s %d tuples in %dms\n",
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

    long time = sys_millis();
    rel_init(rel, NULL);
    Tuple *t;
    int i = 0;
    while ((t = rel_next(rel)) != 0) {
        tuple_free(t);
        i++;
    }

    sys_print("%8s %dx%d tuples in %dms\n",
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

    rel_init(l, NULL);
    rel_init(r, NULL);

    long time = sys_millis();

    rel_eq(l, r);

    sys_print("%8s %dx%d tuples in %dms\n",
              "eq",
              count,
              count,
              sys_millis() - time);

    rel_free(l);
    rel_free(r);
}

int main()
{
    Env *env = env_new(vol_init("bin/volume"));
    tx_init(env->vars.names, env->vars.len);

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

    return 0;
}
