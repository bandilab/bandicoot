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

static char *exe;

static char run(char *file)
{
    char *argv[] = {exe, file, NULL};
    int pid = sys_exec(argv);
    char ret = sys_wait(pid);

    return ret;
}

#define OK(f) do { if (run("test/progs/" f)) fail(); } while (0)
#define FAIL(f) do { if (!run("test/progs/" f)) fail(); } while (0)

static void test_basics()
{
    OK("basic_empty.b");
    OK("basic_prog.b");
}

static void test_rel_types()
{
    FAIL("rel_type_same_attr_err.b");
    FAIL("rel_type_same_type_err.b");
    FAIL("rel_type_max_attrs_err.b");
    FAIL("rel_type_max_decl_err.b");
}

static void test_rel_vars()
{
    OK("rel_var_basic.b");
    FAIL("rel_var_unknown_type_err.b");
    FAIL("rel_var_reassign_err.b");
    FAIL("rel_var_redecl_err.b");
    FAIL("rel_var_redecl2_err.b");
    FAIL("rel_var_name_err.b");
    FAIL("rel_var_read_after_assign_err.b");
    FAIL("rel_var_max_vars_err.b");
}

static void test_func()
{
    OK("func_basic.b");
    FAIL("func_redecl_err.b");
    FAIL("func_unknown_res_type_err.b");
    FAIL("func_bad_res_type_err.b");
    FAIL("func_unexp_ret_stmt_err.b");
    FAIL("func_missing_ret_stmt_err.b");
}

static void test_load()
{
    OK("load_basic.b");
    FAIL("load_unknown_var_err.b");
}

static void test_join()
{
    OK("join_basic.b");
    OK("join_cart.b");
    FAIL("join_types_err.b");
}

static void test_union()
{
    OK("union_basic.b");
    FAIL("union_types_err.b");
}

static void test_semidiff()
{
    OK("semidiff_basic.b");
    FAIL("semidiff_types_err.b");
}

static void test_project()
{
    OK("project_basic.b");
    FAIL("project_same_attr_err.b");
    FAIL("project_max_attrs_err.b");
    FAIL("project_bad_attr_err.b");
}

static void test_rename()
{
    OK("rename_basic.b");
    FAIL("rename_same_from_err.b");
    FAIL("rename_same_to_err.b");
    FAIL("rename_unknown_attr_err.b");
    FAIL("rename_attr_exists_err.b");
}

static void test_primitive_exprs()
{
    OK("primitive_expr_basic.b");
    OK("primitive_expr_ops.b");
    FAIL("primitive_expr_not_string_err.b");
    FAIL("primitive_expr_not_real_err.b");
    FAIL("primitive_expr_eq_err.b");
    FAIL("primitive_expr_neq_err.b");
    FAIL("primitive_expr_and_err.b");
    FAIL("primitive_expr_or_err.b");
    FAIL("primitive_expr_gt_err.b");
    FAIL("primitive_expr_lt_err.b");
    FAIL("primitive_expr_sum_err.b");
    FAIL("primitive_expr_sub_err.b");
    FAIL("primitive_expr_mul_err.b");
    FAIL("primitive_expr_div_err.b");
}

static void test_assign()
{
    OK("assign_basic.b");
    FAIL("assign_bad_wvar_err.b");
    FAIL("assign_bad_types_err.b");
}

static void test_params()
{
    OK("params_basic.b");
    FAIL("params_rel_unknown_type_err.b");
    FAIL("params_rel_unknown_param_err.b");
}

static void test_compat()
{
    Env *e1 = env_new("test/progs/env_compat_1.b");
    Env *e2 = env_new("test/progs/env_compat_2.b");

    if (!env_compat(e1, e1))
        fail();
    if (!env_compat(e2, e2))
        fail();
    if (env_compat(e1, e2))
        fail();

    env_free(e1);
    env_free(e2);
}

static void test_tmp_var()
{
    OK("tmp_var_basic.b");
    FAIL("tmp_var_unknown_err.b");
    FAIL("tmp_var_redecl_err.b");
    FAIL("tmp_var_max_read_1_err.b");
    FAIL("tmp_var_max_read_2_err.b");
}

static void test_summary()
{
    OK("summary_basic.b");
    FAIL("summary_def_err.b");
    FAIL("summary_attr_err.b");
    FAIL("summary_str_err.b");
    FAIL("summary_expr_err.b");
    FAIL("summary_common_err.b");
    FAIL("summary_attr_exists_err.b");
}

static void test_literal()
{
    OK("literal_basic.b");
    FAIL("literal_int_overflow_pos_err.b");
    FAIL("literal_int_overflow_neg_err.b");
    FAIL("literal_long_overflow_pos_err.b");
    FAIL("literal_long_overflow_neg_err.b");
}

int main(int argc, char *argv[])
{
    exe = argv[0];

    int ret = 0;
    if (argc == 1) {
        test_basics();
        test_rel_types();
        test_rel_vars();
        test_func();
        test_load();
        test_join();
        test_union();
        test_semidiff();
        test_project();
        test_rename();
        test_primitive_exprs();
        test_assign();
        test_params();
        test_compat();
        test_tmp_var();
        test_summary();
        test_literal();
    } else {
        char log[MAX_FILE_PATH];
        str_print(log, "%s.log", argv[1]);

        sys_close(2);
        sys_open(log, CREATE | WRITE);

        env_new(argv[1]);
        ret = PROC_OK;
    }

    return ret;
}
