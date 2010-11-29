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

#define NUM_MUTEX_TESTS 10000

static void test_mutex()
{
    int i;

    long m = mutex_new();
    for (i = 0; i < NUM_MUTEX_TESTS; ++i) {
        mutex_lock(m);
        mutex_unlock(m);
    }
    mutex_close(m);

    long ms[NUM_MUTEX_TESTS];
    for (i = 0; i < NUM_MUTEX_TESTS; ++i) {
        ms[i] = mutex_new();
        mutex_lock(ms[i]);
    }

    for (i = 0; i < NUM_MUTEX_TESTS; ++i)
        mutex_unlock(ms[i]);

    for (i = 0; i < NUM_MUTEX_TESTS; ++i)
        mutex_close(ms[i]);
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

int main(void)
{
    test_thread();
    test_mutex();
    test_proc();

    return 0;
}
