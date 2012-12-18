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

typedef struct {
    union {
        int v_int;
        double v_real;
        long long v_long;
        char v_str[MAX_STRING];
    } vals[MAX_ATTRS];
} Arg;

struct Expr {
    Type type;
    void *ctxt;
    union {
        int v_int;
        double v_real;
        long long v_long;
        char v_str[MAX_STRING];
    } val;

    void (*eval)(struct Expr *self, Tuple *t, Arg *arg);
    void (*free)(struct Expr *self);
};

typedef struct Expr Expr;

extern Expr *expr_int(int val);
extern Expr *expr_long(long long val);
extern Expr *expr_real(double val);
extern Expr *expr_str(const char *val);
extern Expr *expr_attr(int pos, Type type);
extern Expr *expr_param(int pos, Type type);
extern Expr *expr_not(Expr *e);
extern Expr *expr_or(Expr *l, Expr *r);
extern Expr *expr_and(Expr *l, Expr *r);
extern Expr *expr_eq(Expr *l, Expr *r);
extern Expr *expr_lt(Expr *l, Expr *r);
extern Expr *expr_lte(Expr *l, Expr *r);
extern Expr *expr_gt(Expr *l, Expr *r);
extern Expr *expr_gte(Expr *l, Expr *r);
extern Expr *expr_sum(Expr *l, Expr *r);
extern Expr *expr_sub(Expr *l, Expr *r);
extern Expr *expr_div(Expr *l, Expr *r);
extern Expr *expr_mul(Expr *l, Expr *r);
extern Expr *expr_conv(Expr *e, Type t);
extern Expr *expr_time();
extern Expr *expr_str_index(Expr *l, Expr *r);

extern int expr_bool_val(Expr *e, Tuple *t, Arg *arg);
extern Value expr_new_val(Expr *e, Tuple *t, Arg *arg);
extern void expr_free(Expr *e);
