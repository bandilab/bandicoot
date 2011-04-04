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

#include <stdlib.h>
#include <stdio.h>
#include "common.h"

extern void fail_test(int line)
{
    sys_die("line %d: test failed\n", line);
}

extern Tuple *gen_tuple(int i)
{
    char buf[32];
    str_print(buf, "hello_from_gen%d", i);

    Value vals[2];
    vals[0] = val_new_int(&i);
    vals[1] = val_new_str(buf);

    return tuple_new(vals, 2);
}

static void init_gen(Rel *r, Vars *rvars, TBuf *arg) {}
static void free_gen(Rel *r) {}

extern TBuf *gen_tuples(int start, int end)
{
    if (end < start)
        fail();

    TBuf *res = tbuf_new();

    int len = end - start;
    int *tmp = mem_alloc(sizeof(int) * len);
    for (int i = start, j = 0; i < end; ++i, ++j)
        tmp[j] = i;

    while (len > 0) {
        int idx = rand() % len;
        tbuf_add(res, gen_tuple(tmp[idx]));
        tmp[idx] = tmp[--len];
    }
    mem_free(tmp);

    return res;
}

extern Head *gen_head()
{
    char *names[] = {"a", "c"};
    Type types[] = {Int, String};

    return head_new(names, types, 2);
}

extern Rel *gen_rel(int start, int end)
{
    Rel *res = mem_alloc(sizeof(Rel));
    res->head = gen_head();
    res->init = init_gen;
    res->free = free_gen;
    res->ctxt = NULL;
    res->body = gen_tuples(start, end);

    return res;
}

extern void change_stderr(const char *path)
{
    if (freopen(path, "wb", stderr) == NULL)
        fail();
}
