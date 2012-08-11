/*
Copyright 2008-2012 Ostap Cherkashin
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

static void test_bool_vals()
{
    Expr *e_true = expr_true();
    Expr *e_false = expr_false();

    if (expr_bool_val(e_true, NULL, NULL) == 0)
        fail();
    if (expr_bool_val(e_false, NULL, NULL) != 0)
        fail();

    expr_free(e_true);
    expr_free(e_false);
}

static void test_bool_ops()
{
    Expr *e_not_t = expr_not(expr_true());
    Expr *e_not_f = expr_not(expr_false());
    Expr *e_and_tt = expr_and(expr_true(), expr_true());
    Expr *e_and_tf = expr_and(expr_true(), expr_false());
    Expr *e_or_tf = expr_or(expr_true(), expr_false());
    Expr *e_or_ff = expr_or(expr_false(), expr_false());

    if (expr_bool_val(e_not_t, NULL, NULL) != 0)
        fail();
    if (expr_bool_val(e_not_f, NULL, NULL) == 0)
        fail();
    if (expr_bool_val(e_and_tt, NULL, NULL) == 0)
        fail();
    if (expr_bool_val(e_and_tf, NULL, NULL) != 0)
        fail();
    if (expr_bool_val(e_or_tf, NULL, NULL) == 0)
        fail();
    if (expr_bool_val(e_or_ff, NULL, NULL) != 0)
        fail();

    expr_free(e_not_t);
    expr_free(e_not_f);
    expr_free(e_and_tt);
    expr_free(e_and_tf);
    expr_free(e_or_tf);
    expr_free(e_or_ff);
}

static void test_cmp_ops()
{
    Expr *e_eq = expr_eq(expr_str("hello world"),
                         expr_str("hello world"));
    Expr *e_neq = expr_eq(expr_str("hello world"),
                          expr_str("hello world2"));
    Expr *e_lt = expr_lt(expr_str("BacvfebG"),
                         expr_str("Bacvfebg"));
    Expr *e_gt = expr_gt(expr_str("BacvfebG"),
                         expr_str("BacvfeaG"));
    Expr *e_ngt = expr_gt(expr_str("hello world!"),
                          expr_str("hello world!"));
    Expr *e_gte = expr_gte(expr_str("hello world!"),
                           expr_str("hello world!"));
    Expr *e_lte = expr_lte(expr_str("hello world!"),
                           expr_str("hello world!"));
    Expr *e_ngte = expr_gte(expr_str("hello a"),
                            expr_str("hello world!"));
    Expr *e_nlte = expr_lte(expr_str("hello world!"),
                            expr_str("hello a"));

    if (!expr_bool_val(e_eq, NULL, NULL))
        fail();
    if (expr_bool_val(e_neq, NULL, NULL))
        fail();
    if (!expr_bool_val(e_gt, NULL, NULL))
        fail();
    if (!expr_bool_val(e_lt, NULL, NULL))
        fail();
    if (expr_bool_val(e_ngt, NULL, NULL))
        fail();
    if (!expr_bool_val(e_gte, NULL, NULL))
        fail();
    if (!expr_bool_val(e_lte, NULL, NULL))
        fail();
    if (expr_bool_val(e_ngte, NULL, NULL))
        fail();
    if (expr_bool_val(e_nlte, NULL, NULL))
        fail();

    expr_free(e_eq);
    expr_free(e_neq);
    expr_free(e_lt);
    expr_free(e_gt);
    expr_free(e_ngt);
    expr_free(e_gte);
    expr_free(e_lte);
    expr_free(e_ngte);
    expr_free(e_nlte);

    e_gte = expr_gte(expr_real(0.9),
                     expr_real(0.5));
    e_lte = expr_lte(expr_int(-1),
                     expr_int(1));

    if (!expr_bool_val(e_gte, NULL, NULL))
        fail();
    if (!expr_bool_val(e_lte, NULL, NULL))
        fail();

    expr_free(e_gte);
    expr_free(e_lte);
}

static void test_arithmetic()
{
    double d0 = 0.0;
    double d1 = 0.0;

    Expr *e_isum = expr_sum(expr_int(2), expr_int(2));
    Expr *e_lsum = expr_sum(expr_long(2), expr_long(2));
    Expr *e_rsum = expr_sum(expr_real(2.2), expr_real(2.2));

    Expr *e_isub = expr_sub(expr_int(3), expr_int(7));
    Expr *e_lsub = expr_sub(expr_long(3), expr_long(7));
    Expr *e_rsub = expr_sub(expr_real(1.3), expr_real(0.3));

    Expr *e_idiv = expr_div(expr_int(8), expr_int(4));
    Expr *e_ldiv = expr_div(expr_long(8), expr_long(4));
    Expr *e_rdiv = expr_div(expr_real(3.0), expr_real(0.1));

    Expr *e_imul = expr_mul(expr_int(5), expr_int(5));
    Expr *e_lmul = expr_mul(expr_long(5), expr_long(5));
    Expr *e_rmul = expr_mul(expr_real(5.0), expr_real(0.5));

    if (4 != val_int(expr_new_val(e_isum, NULL, NULL)))
        fail();
    if (4LL != val_long(expr_new_val(e_lsum, NULL, NULL)))
        fail();

    d0 = val_real(expr_new_val(e_rsum, NULL, NULL));
    d1 = 4.4;
    if (d1 != d0)
        fail();

    if (-4 != val_int(expr_new_val(e_isub, NULL, NULL)))
        fail();
    if (-4LL != val_long(expr_new_val(e_lsub, NULL, NULL)))
        fail();

    d0 = val_real(expr_new_val(e_rsub, NULL, NULL));
    d1 = 1.0;
    if (d1 != d0)
        fail();

    if (2 != val_int(expr_new_val(e_idiv, NULL, NULL)))
        fail();
    if (2LL != val_long(expr_new_val(e_ldiv, NULL, NULL)))
        fail();

    d0 = val_real(expr_new_val(e_rdiv, NULL, NULL));
    d1 = 30;
    if (d1 != d0)
        fail();

    if (25 != val_int(expr_new_val(e_imul, NULL, NULL)))
        fail();
    if (25 != val_long(expr_new_val(e_lmul, NULL, NULL)))
        fail();

    d0 = val_real(expr_new_val(e_rmul, NULL, NULL));
    d1 = 2.5;
    if (d1 != d0)
        fail();

    expr_free(e_isum);
    expr_free(e_lsum);
    expr_free(e_rsum);

    expr_free(e_isub);
    expr_free(e_lsub);
    expr_free(e_rsub);

    expr_free(e_idiv);
    expr_free(e_ldiv);
    expr_free(e_rdiv);

    expr_free(e_imul);
    expr_free(e_lmul);
    expr_free(e_rmul);
}

static void test_attr()
{
    Head *h = gen_head();

    int i = 1; char *s = "string_1";
    Value vals[2];
    vals[0] = val_new_int(&i);
    vals[1] = val_new_str(s);
    Tuple *t = tuple_new(vals, 2);

    int pos;
    Type type;
    head_attr(h, "c", &pos, &type);
    Expr *e = expr_eq(expr_str("string_1"), expr_attr(pos, type));

    if (expr_bool_val(e, t, NULL) == 0)
        fail();

    expr_free(e);
    tuple_free(t);
    mem_free(h);
}

static void test_param()
{
    Expr *e = NULL;

    int i = 12345; double d = 12.345; long long l = 9223372036854775807LL;
    char *s = "string_1";
    Arg *arg = arg_new();
    arg->vals[0].v_int = i;
    arg->vals[1].v_real = d;
    arg->vals[2].v_long = l;
    arg->vals[3].v_str = str_dup(s);

    e = expr_eq(expr_int(i), expr_param(0, Int));
    if (expr_bool_val(e, NULL, arg) == 0)
        fail();
    expr_free(e);

    e = expr_eq(expr_real(d), expr_param(1, Real));
    if (expr_bool_val(e, NULL, arg) == 0)
        fail();
    expr_free(e);

    e = expr_eq(expr_long(l), expr_param(2, Long));
    if (expr_bool_val(e, NULL, arg) == 0)
        fail();
    expr_free(e);

    e = expr_eq(expr_str(s), expr_param(3, String));
    if (expr_bool_val(e, NULL, arg) == 0)
        fail();
    expr_free(e);
    arg_free(arg);
}

static void test_compound()
{
    char *names[] = {"a", "b", "c"};
    Type types[] = {Int, Real, String};

    int a, b, c;
    Type ta, tb, tc;
    Head *h = head_new(names, types, 3);
    head_attr(h, "a", &a, &ta);
    head_attr(h, "b", &b, &tb);
    head_attr(h, "c", &c, &tc);

    int v_int = 3;
    double v_real = 1.01;
    char *v_str = "bbb";

    Value vals[3];
    vals[0] = val_new_int(&v_int);
    vals[1] = val_new_real(&v_real);
    vals[2] = val_new_str(v_str);
    Tuple *t = tuple_new(vals, 3);

    Expr *expr = expr_or(expr_gt(expr_attr(b, Real),
                                 expr_real(4.00)),
                         expr_lt(expr_attr(a, Int),
                                 expr_int(2)));
    expr = expr_or(expr,
                   expr_lt(expr_attr(c, String),
                           expr_str("aab")));

    if (expr_bool_val(expr, t, NULL))
        fail();

    expr_free(expr);
    tuple_free(t);
    mem_free(h);
}

static void test_conv_eval(Expr *l, Expr *r)
{
    Expr *e = expr_eq(l, r);
    if (!expr_bool_val(e, NULL, NULL))
        fail();

    expr_free(e);
}

static void test_conv()
{
    test_conv_eval(
            expr_conv(expr_int(123), Int),
            expr_int(123));
    test_conv_eval(
            expr_conv(expr_int(123), Real),
            expr_real(123.0));
    test_conv_eval(
            expr_conv(expr_int(123), Long),
            expr_long(123LL));

    test_conv_eval(
            expr_conv(expr_real(123.12), Int),
            expr_int(123));
    test_conv_eval(
            expr_conv(expr_real(123.12), Real),
            expr_real(123.12));
    test_conv_eval(
            expr_conv(expr_real(123.12), Long),
            expr_long(123LL));

    test_conv_eval(
            expr_conv(expr_long(123LL), Int),
            expr_int(123));
    test_conv_eval(
            expr_conv(expr_long(123LL), Real),
            expr_real(123.0));
    test_conv_eval(
            expr_conv(expr_long(123LL), Long),
            expr_long(123LL));
}

int main()
{
    test_bool_vals();
    test_bool_ops();
    test_cmp_ops();
    test_arithmetic();
    test_attr();
    test_param();
    test_compound();
    test_conv();

    return 0;
}
