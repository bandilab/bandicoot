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

typedef enum { ATTR, VALUE, NOT, EQ, NEQ, AND,
               OR, GT, LT, GTE, LTE, SUM, SUB,
               MUL, DIV, NEG, POS, FUNC } L_Expr_Type;

typedef enum { CNT, MIN, MAX, AVG, ADD } L_Sum_Type;

union L_Value {
    int v_int;
    long long v_long;
    double v_real;
    char v_str[MAX_STRING];
};

struct L_Expr {
    L_Expr_Type node_type;
    char pkg[MAX_NAME];
    char name[MAX_NAME];
    Type type;
    int is_const;
    union L_Value val;
    struct L_Expr *left;
    struct L_Expr *right;
};

struct L_Sum {
    L_Sum_Type sum_type;
    char attr[MAX_NAME];
    struct L_Expr *def;
};

struct L_Attrs {
    int len;
    char *names[MAX_ATTRS];
    char *renames[MAX_ATTRS];
    Type types[MAX_ATTRS];
    Rel *rexprs[MAX_ATTRS];
    struct L_Expr *pexprs[MAX_ATTRS];
    struct L_Sum sums[MAX_ATTRS];
};

typedef union L_Value L_Value;
typedef struct L_Attrs L_Attrs;
typedef struct L_Expr L_Expr;
typedef struct L_Sum L_Sum;
