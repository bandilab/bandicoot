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

#include "../common.h"

static char *exe;
static void *thread(void *arg) { return 0; }

static void perf_thread(int count)
{
    long long time = sys_millis();
    for (int i = 0; i < count; ++i)
        sys_thread(thread, 0);

    sys_print("created %d threads in %lldms\n", count, sys_millis() - time);
}

static void perf_fork(int count)
{
    char *argv[] = {exe, "test_arg", NULL};

    long long time = sys_millis();
    for (int i = 0; i < count; ++i) {
        int pid = sys_exec(argv);
        if (0 != sys_wait(pid))
            fail();
    }

    sys_print("fork/wait of %d processes - %lldms\n",
              count, sys_millis() - time);
}

int main(int argc, char *argv[])
{
    exe = argv[0];

    if (argc == 1) {
        void *mem = mem_alloc(1024 * 1024);

        perf_fork(1000);
        perf_fork(10000);
        perf_thread(1000);
        perf_thread(10000);

        mem_free(mem);
    }

    return 0;
}
