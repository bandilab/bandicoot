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

static Vars *rvars;
static TBuf *arg;
static Env *env = NULL;

static Rel *pack(char *str)
{
    Head *h;
    TBuf *buf = rel_pack_sep(str, &h);
    Rel *res = NULL;

    if (buf != NULL) {
        arg = buf;
        res = rel_param(h);
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
    Vars *wvars = vars_new(0);

    long sid = tx_enter(rvars, wvars);

    rel_init(left, rvars, arg);
    rel_init(right, rvars, arg);

    int res = rel_eq(left, right);

    rel_free(left);
    rel_free(right);

    tx_commit(sid);

    vars_free(wvars);

    return res;
}

static int count(const char *name)
{
    Rel *r = load(name);
    Vars *wvars = vars_new(0);

    long sid = tx_enter(rvars, wvars);

    rel_init(r, rvars, arg);

    Tuple *t;
    int i;
    for (i = 0; (t = rel_next(r)) != 0; ++i)
        tuple_free(t);

    rel_free(r);
    tx_commit(sid);

    vars_free(wvars);
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
    str_cpy(param_1, "a:int,c:string\n1,one\n2,two\n1,one\n2,two");

    if (!equal(pack(param_1), "param_1"))
        fail();
}

static void test_eq()
{
    Rel *left = rel_empty();
    Rel *right = rel_empty();

    int res = rel_eq(left, right);

    rel_free(left);
    rel_free(right);

    if (!res)
        fail();

    if (equal(rel_empty(), "empty_r1"))
        fail();
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

    Vars *wvars = vars_new(1);
    vars_put(wvars, "one_r1_cpy", 0L);

    long sid = tx_enter(rvars, wvars);
    rel_init(r, rvars, arg);

    rel_store("one_r1_cpy", wvars->vers[0], r);
    rel_free(r);
    tx_commit(sid);

    vars_free(wvars);
    if (!equal(load("one_r1"), "one_r1_cpy"))
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
    Sum *sums[] = {sum_cnt(),
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
                         sum_cnt(),
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
    str_cpy(union_2_l, "car:string,date:string,email:string,"
                  "name:string,phone:string,pos:int,time:int\n"
                  "myCar,28-Jun-2010,myEmail,myName,myPhone,2,14");

    if (!equal(rel_union(pack(union_2_l),
                         load("union_2_r")),
               "union_2_res"))
        fail();

    str_cpy(union_2_l, "car:string,date:string,email:string,"
                  "name:string,phone:string,pos:int,time:int\n"
                  "myCar,28-Jun-2010,myEmail,myName,myPhone,2,14");
    if (!equal(rel_union(load("union_2_r"),
                         pack(union_2_l)),
               "union_2_res"))
        fail();
}

static void check_vars(Vars *v)
{
    if (v == NULL || v->len != 3)
        fail();

    if (str_cmp(v->vars[0], "a1") != 0 || v->vers[0] != 1)
        fail();
    if (str_cmp(v->vars[1], "a2") != 0 || v->vers[1] != 2)
        fail();
    if (str_cmp(v->vars[2], "a3") != 0 || v->vers[2] != 3)
        fail();

    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < MAX_VOLS; ++j)
            if (v->vols[i][j] != i * j + 3)
                fail();
}

static void test_vars()
{
    Vars *v[] = {vars_new(0), vars_new(3), vars_new(7)};

    for (unsigned i = 0; i < sizeof(v) / sizeof(Vars*); ++i) {
        vars_put(v[i], "a1", 1);
        vars_put(v[i], "a2", 2);
        vars_put(v[i], "a3", 3);
        for (int j = 0; j < 3; ++j)
            for (int k = 0; k < MAX_VOLS; ++k)
                v[i]->vols[j][k] = j * k + 3;

        check_vars(v[i]);

        Vars *c = vars_new(0);
        vars_cpy(c, v[i]);
        check_vars(c);

        const char *file = "bin/relation_vars_test";
        IO *io = sys_open(file, CREATE | WRITE);
        if (vars_write(v[i], io) < 0)
            fail();
        sys_close(io);
        io = sys_open(file, READ);
        Vars *out = vars_read(io);
        if (out == NULL)
            fail();

        check_vars(out);

        vars_free(v[i]);
        vars_free(out);
        vars_free(c);

        sys_close(io);
    }
}

int main()
{
    int tx_port = 0;

    fs_init("bin/volume");
    tx_server(&tx_port);
    tx_attach(tx_port);
    vol_init();

    env = env_new(fs_source);

    int len = 0;
    char **files = sys_list("test/data", &len);

    rvars = vars_new(len);
    for (int i = 0; i < len; ++i)
        vars_put(rvars, files[i], 0L);

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
    test_vars();

    tx_free();
    env_free(env);
    mem_free(files);
    vars_free(rvars);

    return 0;
}
