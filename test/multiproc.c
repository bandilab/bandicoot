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

static void proc_check(char *env[], char exit_code)
{
    int pid = sys_exec(env);
    char ret = sys_wait(pid);
    if (ret != exit_code)
        fail();
}

static void test_proc(char *exe)
{
    char *test_ok[] = {exe, "PROC_OK", NULL};
    char *test_fail[] = {exe, "PROC_FAIL", NULL};
    char *test_cust[] = {exe, "PROC_CUST", NULL};

    proc_check(test_ok, PROC_OK);
    proc_check(test_fail, PROC_FAIL);
    proc_check(test_cust, 'T');
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

int main(int argc, char *argv[])
{
    int exit = 0;

    if (argc == 1) {
        test_thread();
        test_monitor();
        test_proc(argv[0]);
    } else if (argc == 2) {
        /* child process exit codes */
        if (str_cmp(argv[1], "PROC_OK") == 0)
            exit = PROC_OK;
        else if (str_cmp(argv[1], "PROC_FAIL") == 0)
            exit = PROC_FAIL;
        else if (str_cmp(argv[1], "PROC_CUST") == 0)
            exit = 'T';
    }

    return exit;
}
