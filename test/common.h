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

#include "../array.h"
#include "../config.h"
#include "../error.h"
#include "../memory.h"
#include "../list.h"
#include "../string.h"
#include "../number.h"
#include "../system.h"
#include "../head.h"
#include "../value.h"
#include "../tuple.h"
#include "../expression.h"
#include "../summary.h"
#include "../volume.h"
#include "../relation.h"
#include "../transaction.h"
#include "../environment.h"
#include "../pack.h"
#include "../http.h"
#include "../index.h"

#define fail() fail_test(__LINE__);

extern void fail_test(int line);
extern Head *gen_head();
extern Tuple *gen_tuple(int i);
extern TBuf *gen_tuples(int start, int end);
extern Rel *gen_rel(int start, int end);

/* from tuple.c */
extern Tuple *tuple_dec(void *mem, int *len);
extern int tuple_enc(Tuple *t, void *buf);

/* from transaction.c */
extern long long enter(const char *eid, Vars *rvars, Vars *wvars, Mon *m);
extern void commit(long long sid);
extern void revert(long long sid);

static Expr *expr_true()
{
    return expr_int(1);
}

static Expr *expr_false()
{
    return expr_int(0);
}

static int val_eq(Value l, Value r)
{
    return val_cmp(l, r) == 0;
}

extern void change_stderr(const char *path);
