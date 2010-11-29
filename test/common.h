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

#include "../array.h"
#include "../config.h"
#include "../memory.h"
#include "../string.h"
#include "../number.h"
#include "../system.h"
#include "../head.h"
#include "../value.h"
#include "../tuple.h"
#include "../expression.h"
#include "../summary.h"
#include "../mutex.h"
#include "../volume.h"
#include "../transaction.h"
#include "../relation.h"
#include "../pack.h"
#include "../environment.h"
#include "../http.h"

#define fail() fail_test(__LINE__);

extern void fail_test(int line);
extern Head *gen_head();
extern TBuf *gen_tuples(int start, int end);
extern Rel *gen_rel(int start, int end);

static Expr *expr_true()
{
    return expr_int(1);
}

static Expr *expr_false()
{
    return expr_int(0);
}
