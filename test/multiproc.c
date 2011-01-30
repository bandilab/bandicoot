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

#define TESTS 1000

static void test_monitor()
{
    Mon *ms[TESTS];
    for (int i = 0; i < TESTS; ++i)
        ms[i] = mon_new();

    for (int i = 0; i < TESTS; ++i)
        mon_lock(ms[i]);

    for (int i = 0; i < TESTS; ++i)
        mon_unlock(ms[i]);

    for (int i = 0; i < TESTS; ++i)
        mon_free(ms[i]);
}

static void proc_a(void *arg)
{
    sys_exit(37);
}

static void test_proc()
{
    if (sys_proc(proc_a, 0) != 37)
        fail();
}

static void *thread_a(void *arg)
{
    if ((long) arg != 0x75L)
        fail();

    return 0;
}

static void test_thread()
{
    sys_thread(thread_a, (void*) 0x75L);
}

static void proc_exit_normal(void *arg)
{
}

static void proc_exit_ok(void *arg)
{
    sys_exit(PROC_OK);
}

static void proc_exit_fail(void *arg)
{
    sys_exit(PROC_FAIL);
}

static void proc_exit_400(void *arg)
{
    sys_exit(PROC_400);
}

static void proc_exit_404(void *arg)
{
    sys_exit(PROC_404);
}

static void proc_exit_405(void *arg)
{
    sys_exit(PROC_405);
}

static void test_exit()
{
    if (PROC_OK != sys_proc(proc_exit_normal, NULL))
        fail();
    if (PROC_OK != sys_proc(proc_exit_ok, NULL))
        fail();
    if (PROC_FAIL != sys_proc(proc_exit_fail, NULL))
        fail();
    if (PROC_400 != sys_proc(proc_exit_400, NULL))
        fail();
    if (PROC_404 != sys_proc(proc_exit_404, NULL))
        fail();
    if (PROC_405 != sys_proc(proc_exit_405, NULL))
        fail();
}

int main(void)
{
    test_thread();
    test_monitor();
    test_proc();
    test_exit();

    return 0;
}
