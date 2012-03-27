/*
Copyright 2008-2011 Ostap Cherkashin

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

static int vol_size(const char *dir)
{
    int len = 0;
    char **files = sys_list(dir, &len);

    int size = 0;
    for (int i = 0; i < len; ++i) {
        char path[MAX_FILE_PATH];
        str_print(path, "%s/%s", dir, files[i]);

        size += sys_fsize(path);
    }

    mem_free(files);
    return size;
}

static void *monitor_thread(void *a)
{
    Mon *m = a;
    int stop = 0, max = 0;

    while (!stop) {
        int size = vol_size("bin/volume");
        if (max < size)
            max = size;

        mon_lock(m);

        if (m->value < 0)
            stop = 1;
        else
            m->value = max;

        mon_signal(m);
        mon_unlock(m);

        sys_sleep(1);
    }

    return NULL;
}

static char *test_append(int num_chunks, int chunk_size)
{
    char *chunks[num_chunks];
    char *e[] = { "bin/bandicoot", "start",
                  "-p", "12345",
                  "-d", "bin/volume",
                  "-s", "bin/state",
                  "-c", "test/test_defs.b", NULL };

    int sizes[num_chunks];
    int vol_min = 0;
    int vol_max = 0;
    long long total = 0;
    long long time_min = MAX_LONG;
    long long time_max = 0;

    gen_rel_str(num_chunks, chunk_size, chunks, sizes);

    Mon *m = mon_new();
    int pid = sys_exec(e);
    sys_sleep(1);
    /* FIXME: wrong place for cleanup. should either wait 30 secs or do it
     * differently */
    if (!http_req("GET", "localhost:12345", "reset_append", "", 0))
        sys_die("reset_append failed\n");
    sys_sleep(1);

    sys_thread(monitor_thread, m);
    for (int i = 0; i < num_chunks; ++i) {
        long time = sys_millis();
        if (!http_req("POST", "localhost:12345", "append", chunks[i], sizes[i]))
            sys_die("append failed\n");
        time = sys_millis() - time;

        total += time;
        if (time_min > time)
            time_min = time;
        if (time_max < time)
            time_max = time;
    }
    sys_kill(pid);

    mon_lock(m);
    mon_wait(m);
    vol_max = m->value;
    m->value = -1;
    mon_unlock(m);

    pid = sys_exec(e);
    sys_sleep(3);
    vol_min = vol_size("bin/volume");
    sys_kill(pid);

    char *res = mem_alloc(128);
    str_print(res, "%d\t%d\t%lld\t%lld\t%lld\t%lld\t%d\t%d\n",
              num_chunks, chunk_size, total, time_min, time_max,
              total / num_chunks, vol_min / (1024 * 1024),
              vol_max / (1024 * 1024));

    mon_free(m);
    for (int i = 0; i < num_chunks; ++i)
        mem_free(chunks[i]);

    return res;
}

int main(void)
{
    int len = 8;
    char *res[len];
    int num_chunks[] = {1,    1,     1,      1,
                        10,   10,    10,
                        1000};
    int chunk_size[] = {1000, 10000, 100000, 1000000,
                        1000, 10000, 100000,
                        1};

    for (int i = 0; i < len; ++i)
        res[i] = test_append(num_chunks[i], chunk_size[i]);
    sys_print("---\n\n");

    sys_print("num chunks\n");
    sys_print("|\tchunk size (tuples)\n");
    sys_print("|\t|\ttotal time (ms)\n");
    sys_print("|\t|\t|\tmin time (ms per chunk)\n");
    sys_print("|\t|\t|\t|\tmax time (ms per chunk)\n");
    sys_print("|\t|\t|\t|\t|\tavg time (ms per chunk)\n");
    sys_print("|\t|\t|\t|\t|\t|\tactual volume size (MiB)\n");
    sys_print("|\t|\t|\t|\t|\t|\t|\tmax volume size (MiB)\n");
    for (int i = 0; i < len; ++i) {
        sys_print(res[i]);
        mem_free(res[i]);
    }

    return 0;
}
