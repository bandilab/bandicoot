/*
Copyright 2008-2012 Ostap Cherkashin
Copyright 2008-2012 Julius Chrobak

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

static int pp_pos(Func *fn, const char *name)
{
    int i = array_scan(fn->pp.names, fn->pp.len, name);
    return (i > -1) ? fn->pp.positions[i] : 0;
}

static int rp_pos(Func *fn, const char *name)
{
    if (str_cmp(fn->rp.name, name) == 0)
        return fn->rp.position;

    return 0;
}

#define OK(f) do { if (run("test/progs/" f)) fail(); } while (0)
#define FAIL(f) do { if (!run("test/progs/" f)) fail(); } while (0)

static void test_basics()
{
    OK("basic_empty.b");
    OK("basic_prog.b");
    FAIL("basic_max_funcs_err.b");
}

static void test_rel_types()
{
    FAIL("rel_type_same_attr_err.b");
    FAIL("rel_type_same_type_err.b");
    FAIL("rel_type_max_attrs_1_err.b");
    FAIL("rel_type_max_attrs_2_err.b");
    FAIL("rel_type_max_attrs_3_err.b");
    FAIL("rel_type_max_decl_err.b");
}

static void test_rel_vars()
{
    OK("rel_var_basic.b");
    OK("rel_var_multiple_decls.b");
    FAIL("rel_var_unknown_type_err.b");
    FAIL("rel_var_redecl_err.b");
    FAIL("rel_var_redecl_2_err.b");
    FAIL("rel_var_redecl_3_err.b");
    FAIL("rel_var_redecl_4_err.b");
    FAIL("rel_var_redecl_5_err.b");
    FAIL("rel_var_name_err.b");
    FAIL("rel_var_max_vars_err.b");
}

static void test_func()
{
    OK("func_basic.b");
    OK("func_calls_basic.b");
    FAIL("func_redecl_err.b");
    FAIL("func_unknown_res_type_err.b");
    FAIL("func_bad_res_type_err.b");
    FAIL("func_unexp_ret_stmt_err.b");
    FAIL("func_missing_res_type_err.b");
    FAIL("func_missing_ret_stmt_err.b");
    FAIL("func_missing_ret_stmt_2_err.b");
    FAIL("func_stmt_exceeded_err.b");
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
    OK("join_max_attrs.b");
    FAIL("join_types_err.b");
    FAIL("join_max_attrs_err.b");
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

static void test_select()
{
    OK("select_basic.b");
    FAIL("select_unknown_attr_err.b");
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
    OK("primitive_expr_conv.b");
    OK("primitive_expr_time.b");
    OK("primitive_expr_str_index.b");
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
    FAIL("primitive_expr_unknown_func_err.b");
    FAIL("primitive_expr_conv_str_to_int_err.b");
    FAIL("primitive_expr_conv_str_to_real_err.b");
    FAIL("primitive_expr_conv_str_to_long_err.b");
    FAIL("primitive_expr_conv_param_err.b");
    FAIL("primitive_expr_time_int_err.b");
    FAIL("primitive_expr_str_index_param_missing_err.b");
    FAIL("primitive_expr_str_index_param_one_type_err.b");
    FAIL("primitive_expr_str_index_param_two_type_err.b");
}

static void test_assign()
{
    OK("assign_basic.b");
    OK("assign_union.b");
    OK("assign_diff.b");
    OK("assign_semidiff.b");
    OK("assign_join.b");
    OK("assign_semijoin.b");
    FAIL("assign_bad_wvar_err.b");
    FAIL("assign_bad_types_err.b");
    FAIL("assign_union_bad_types_err.b");
    FAIL("assign_diff_bad_types_err.b");
    FAIL("assign_join_bad_types_err.b");
}

static void test_params()
{
    OK("params_basic.b");
    FAIL("params_rel_unknown_type_err.b");
    FAIL("params_rel_unknown_param_err.b");
    FAIL("params_rel_redecl_err.b");
    FAIL("params_rel_redecl_2_err.b");
    FAIL("params_rel_more_than_one_err.b");
    FAIL("params_prim_redecl_err.b");
    FAIL("params_max_attrs_err.b");

    const char *s = "test/progs/params_order.b";
    char *c = sys_load(s);
    Env *e = env_new(s, c);

    Func *f = env_func(e, "rel_param1");
    if (rp_pos(f, "p") != 1 ||
        pp_pos(f, "i") != 2 ||
        pp_pos(f, "l") != 3 ||
        pp_pos(f, "r") != 4 ||
        pp_pos(f, "s") != 5)
        fail();

    f = env_func(e, "rel_param2");
    if (pp_pos(f, "l") != 1 ||
        pp_pos(f, "i") != 2 ||
        rp_pos(f, "p") != 3 ||
        pp_pos(f, "r") != 4 ||
        pp_pos(f, "s") != 5)
        fail();

    f = env_func(e, "rel_param3");
    if (pp_pos(f, "s") != 1 ||
        pp_pos(f, "r") != 2 ||
        pp_pos(f, "l") != 3 ||
        pp_pos(f, "i") != 4 ||
        rp_pos(f, "p") != 5)
        fail();

    mem_free(c);
    env_free(e);
}

static void test_compat()
{
    const char *s1 = "test/progs/env_compat_1.b";
    const char *s2 = "test/progs/env_compat_2.b";

    char *c1 = sys_load(s1);
    char *c2 = sys_load(s2);

    Env *e1 = env_new(s1, c1);
    Env *e2 = env_new(s2, c2);

    if (!env_compat(e1, e1))
        fail();
    if (!env_compat(e2, e2))
        fail();
    if (env_compat(e1, e2))
        fail();

    env_free(e1);
    env_free(e2);
    mem_free(c1);
    mem_free(c2);
}

static void test_tmp_var()
{
    OK("tmp_var_basic.b");
    FAIL("tmp_var_unknown_err.b");
    FAIL("tmp_var_redecl_err.b");
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
    FAIL("summary_max_attrs_unary_err.b");
    FAIL("summary_max_attrs_binary_err.b");
    FAIL("summary_brackets_err.b");
    FAIL("summary_unknown_err.b");
}

static void test_literal()
{
    OK("literal_basic.b");
    FAIL("literal_int_overflow_pos_err.b");
    FAIL("literal_int_overflow_neg_err.b");
    FAIL("literal_long_overflow_pos_err.b");
    FAIL("literal_long_overflow_neg_err.b");
}

static void test_extend()
{
    OK("extend_basic.b");
    FAIL("extend_attr_exists_err.b");
    FAIL("extend_max_attrs_err.b");
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
        test_select();
        test_project();
        test_rename();
        test_primitive_exprs();
        test_assign();
        test_params();
        test_compat();
        test_tmp_var();
        test_summary();
        test_literal();
        test_extend();
    } else {
        char log[MAX_FILE_PATH];
        str_print(log, "%s.log", argv[1]);

        change_stderr(log);

        char *code = sys_load(argv[1]);
        Env *env = env_new(argv[1], code);
        mem_free(code);

        env_free(env);

        ret = PROC_OK;
    }

    return ret;
}
