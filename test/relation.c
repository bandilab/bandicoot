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

#include "common.h"

static Args args;
static Env *env = NULL;

static Rel *pack(char *str)
{
    Head *h;
    TBuf *buf = rel_pack_sep(str, &h);
    Rel *res = NULL;

    if (buf != NULL) {
        /* rel_init (init_param) will pick up tbuf[0] (it should be
           fine to reuse the global variable name for parameter as
           they are in a different namespace) */
        args.tbufs[0] = buf;
        res = rel_param(h, args.names[0]);
        mem_free(h);
    }

    return res;
}

static Rel *load(const char *name)
{
    Head *head = env_head(env, name);
    return rel_load(head, name);
}

static int equal(Rel *left, const char *name)
{
    Rel *right = load(name);

    long sid = tx_enter(args.names, args.vers, args.len, NULL, NULL, 0);

    rel_init(left, &args);
    rel_init(right, &args);

    int res = rel_eq(left, right);

    rel_free(left);
    rel_free(right);

    tx_commit(sid);

    return res;
}

static int count(const char *name)
{
    Rel *r = load(name);
    long sid = tx_enter(args.names, args.vers, args.len, NULL, NULL, 0);
    rel_init(r, &args);

    Tuple *t;
    int i;
    for (i = 0; (t = rel_next(r)) != 0; ++i)
        tuple_free(t);

    rel_free(r);
    tx_commit(sid);

    return i;
}

static void test_load()
{
    if (count("empty_r1") != 0)
        fail();
    if (count("one_r1") != 1)
        fail();
    if (count("two_r2") != 2)
        fail();
}

static void test_param()
{
    char param_1[1024];
    str_cpy(param_1, "a=int`c=string\n1`one\n2`two\n1`one\n2`two");

    if (!equal(pack(param_1), "param_1"))
        fail();
}

static void test_eq()
{
    if (equal(load("empty_r1"), "empty_r2"))
        fail();
    if (!equal(load("empty_r1"), "empty_r1"))
        fail();
    if (!equal(load("empty_r2"), "empty_r2"))
        fail();
    if (!equal(load("equal_r1"), "equal_r1"))
        fail();
    if (equal(load("equal_r1"), "equal_r2"))
        fail();
    if (!equal(load("equal_r1"), "equal_r3"))
        fail();
    if (equal(load("equal_r2"), "equal_r3"))
        fail();
}

static void test_store()
{
    Rel *r = load("one_r1");

    char *wnames[] = {"tx_empty"};
    long wvers[1];

    long sid = tx_enter(args.names, args.vers, args.len, wnames, wvers, 1);
    rel_init(r, &args);

    rel_store("tx_empty", wvers[0], r);
    rel_free(r);
    tx_commit(sid);

    if (!equal(load("one_r1"), "tx_empty"))
        fail();
}

static void test_select()
{
    int a, b, c;
    Type ta, tb, tc;

    Rel *src = load("select_1");
    head_attr(src->head, "b", &b, &tb);
    Expr *expr = expr_eq(expr_real(1.01), expr_attr(b, tb));
    Rel *r = rel_select(src, expr);

    if (!equal(r, "select_1_res"))
        fail();

    src = load("select_2");
    head_attr(src->head, "a", &a, &ta);
    head_attr(src->head, "b", &b, &tb);
    head_attr(src->head, "c", &c, &tc);
    expr = expr_or(expr_gt(expr_attr(b, tb),
                           expr_real(4.00)),
                   expr_lt(expr_attr(a, ta),
                           expr_int(2)));
    expr = expr_or(expr,
                   expr_lt(expr_attr(c, tc),
                           expr_str("aab")));
    r = rel_select(src, expr);
    
    if (!equal(r, "select_2_res"))
        fail();

    src = load("select_3");
    head_attr(src->head, "a", &a, &ta);
    head_attr(src->head, "b", &b, &tb);
    expr = expr_and(expr_not(expr_attr(a, ta)),
                    expr_not(expr_eq(expr_attr(b, tb),
                                     expr_real(1.01))));
    r = rel_select(src, expr);
    
    if (!equal(r, "select_3_res"))
        fail();
}

static void test_rename()
{
    char *from[] = {"c", "a", "b"};
    char *to[] = {"new_name", "new_id", "new_some"};

    Rel *r = rel_rename(load("rename_1"), from, to, 3);

    if (!equal(r, "rename_1_res"))
        fail();
}

static void test_extend()
{
    char *names[] = {"d"};
    Expr *exprs[] = {expr_false()};

    Rel *r = rel_extend(load("extend_1"), names, exprs, 1);

    if (!equal(r, "extend_res_1"))
        fail();
}

static void test_join()
{
    Rel *join = rel_join(load("join_1_r1"), load("join_1_r2"));
    if (!equal(join, "join_1_res"))
        fail();

    join = rel_join(load("join_2_r1"), load("join_2_r2"));
    if (!equal(join, "join_2_res"))
        fail();
}

static void test_project()
{
    char *names[] = {"b", "c"};

    Rel *prj = rel_project(load("project_1"), names, 2);
    if (!equal(prj, "project_1_res"))
        fail();

    prj = rel_project(load("project_2"), names, 2);
    if (!equal(prj, "project_2_res"))
        fail();
}

static void test_semidiff()
{
    Rel *sem = rel_diff(load("semidiff_1_l"), load("semidiff_1_r"));
    if (!equal(sem, "semidiff_1_res"))
        fail();

    sem = rel_diff(load("semidiff_2_l"), load("semidiff_2_r"));
    if (!equal(sem, "semidiff_2_res"))
        fail();
}

static void test_summary()
{
    int int_zero = 0, int_minus_one = -1;
    double real_plus_one = 1.0, real_zero = 0.0, real_minus_decimal = -0.99;
    long long long_minus_one = -1, long_zero = 0;

    char *names[] = {"employees", 
                     "min_age", "max_age", "avg_age", "add_age",
                     "min_salary", "max_salary", "avg_salary", "add_salary",
                     "min_birth", "max_birth", "avg_birth", "add_birth"};
    Type types[] =  {Int,
                     Int, Int, Real, Int,
                     Real, Real, Real, Real,
                     Long, Long, Real, Long};

    Rel *r = load("summary_emp");
    int pos1, pos2, pos3;
    Type tp1, tp2, tp3;
    head_attr(r->head, "age", &pos1, &tp1);
    head_attr(r->head, "salary", &pos2, &tp2);
    head_attr(r->head, "birth", &pos3, &tp3);
    Sum *sums[] = {sum_count(),
                   /* age calcs */
                   sum_min(pos1, tp1, val_new_int(&int_minus_one)),
                   sum_max(pos1, tp1, val_new_int(&int_minus_one)),
                   sum_avg(pos1, tp1, val_new_real(&real_plus_one)),
                   sum_add(pos1, tp1, val_new_int(&int_minus_one)),
                   /* salary calcs */
                   sum_min(pos2, tp2, val_new_real(&real_zero)),
                   sum_max(pos2, tp2, val_new_real(&real_zero)),
                   sum_avg(pos2, tp2, val_new_real(&real_zero)),
                   sum_add(pos2, tp2, val_new_real(&real_zero)),
                   /* birth calcs */
                   sum_min(pos3, tp3, val_new_long(&long_minus_one)),
                   sum_max(pos3, tp3, val_new_long(&long_zero)),
                   sum_avg(pos3, tp3, val_new_real(&real_minus_decimal)),
                   sum_add(pos3, tp3, val_new_long(&long_zero))
                   };

    Rel *sum = rel_sum(r, load("summary_dep"), names, types, sums, 13);
    if (!equal(sum, "summary_res_1"))
        fail();

    char *unary_names[] = {"min_age", "max_age", "min_salary",
                           "max_salary", "count", "add_salary"};
    Type unary_types[] = {Int, Int, Real, Real, Int, Real};

    r = load("summary_emp");
    Sum *unary_sums[] = {sum_min(pos1, tp1, val_new_int(&int_zero)),
                         sum_max(pos1, tp1, val_new_int(&int_zero)),
                         sum_min(pos2, tp2, val_new_real(&real_zero)),
                         sum_max(pos2, tp2, val_new_real(&real_zero)),
                         sum_count(),
                         sum_add(pos2, tp2, val_new_real(&real_zero))};

    sum = rel_sum_unary(r, unary_names, unary_types, unary_sums, 6);
    if (!equal(sum, "summary_res_2"))
        fail();
}

static void test_union()
{
    Rel *un = rel_union(load("union_1_l"), load("union_1_r"));
    if (!equal(un, "union_1_res"))
        fail();

    un = rel_union(load("union_1_r"), load("union_1_l"));
    if (!equal(un, "union_1_res"))
        fail();

    un = rel_union(load("union_2_l"), load("union_2_r"));
    if (!equal(un, "union_2_res"))
        fail();

    un = rel_union(load("union_2_r"), load("union_2_l"));
    if (!equal(un, "union_2_res"))
        fail();
}

static void test_compound()
{
    char *rn_from[] = {"a2"};
    char *rn_to[] = {"a"};
    char *pn[] = {"a2", "c"};

    Rel *tmp = load("compound_1");

    int a;
    Type ta;
    head_attr(tmp->head, rn_to[0], &a, &ta);

    Expr *e[] = { expr_mul(expr_int(10), expr_attr(a, Int)) };
    tmp = rel_extend(tmp, rn_from, e, 1);
    tmp = rel_project(tmp, pn, 2);
    tmp = rel_rename(tmp, rn_from, rn_to, 1);
    tmp = rel_union(tmp, load("compound_1"));

    if (!equal(tmp, "compound_1_res"))
        fail();

    /* compound with a parameter */
    char union_2_l[1024];
    str_cpy(union_2_l, "car=string`date=string`email=string`"
                  "name=string`phone=string`pos=int`time=int\n"
                  "myCar`28-Jun-2010`myEmail`myName`myPhone`2`14");

    if (!equal(rel_union(pack(union_2_l),
                         load("union_2_r")),
               "union_2_res"))
        fail();

    str_cpy(union_2_l, "car=string`date=string`email=string`"
                  "name=string`phone=string`pos=int`time=int\n"
                  "myCar`28-Jun-2010`myEmail`myName`myPhone`2`14");
    if (!equal(rel_union(load("union_2_r"),
                         pack(union_2_l)),
               "union_2_res"))
        fail();
}

int main()
{
    env = env_new(vol_init("bin/volume"));
    tx_init(env->vars.names, env->vars.len);

    char **files = sys_list("test/data", &args.len);
    for (int i = 0; i < args.len; ++i)
        args.names[i] = files[i];

    test_load();
    test_param();
    test_eq();
    test_store();
    test_select();
    test_rename();
    test_extend();
    test_join();
    test_project();
    test_semidiff();
    test_summary();
    test_union();
    test_compound();

    tx_free();
    env_free(env);
    mem_free(files);

    return 0;
}
