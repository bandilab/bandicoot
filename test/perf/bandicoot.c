/*
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

#include <stdlib.h>
#include <stdio.h>
#include "../common.h"

#define CONNS 16
#define ITERATIONS 1024
#define IDS_CNT 100000

int THREADS;

typedef struct {
    char *addrs[CONNS];
    int len;
} Exec;

static int get(IO *io, char *fn)
{
    Http_Args args = {.len = 0};
    if (http_get(io, fn, &args) != 200)
        fail();

    Http_Resp *resp = http_parse_resp(io);
    int res = 0;

    if (resp != NULL) {
        res = resp->status;
        http_free_resp(resp);
    }

    return res;
}

static IO *conn(IO *ios[CONNS], int i, char *addr)
{
    ios[i] = sys_connect(addr, IO_STREAM);
    if (ios[i] == NULL)
        fail();

    return ios[i];
}

static void close(IO *io) {
    if (io != NULL)
        sys_close(io);
}

static int *get_ids(const char *addr, int *len) {
    IO *io = sys_connect(addr, IO_STREAM);

    Http_Args args = {.len = 0};
    if (http_get(io, "/ids", &args) != 200)
        fail();

    Http_Resp *resp = http_parse_resp(io);
    if (resp == NULL || resp->status != 200)
        fail();
    
    int cnt = 0;
    char **ids = str_split_big(resp->body, "\n", &cnt);

    int *res = mem_alloc(cnt * sizeof(int));

    for (int i = 0; i < cnt; ++i) {
        int err = 0;
        int val = str_int(ids[i], &err);
        if (!err)
            res[(*len)++] = val;
    }

    mem_free(ids);
    http_free_resp(resp);
    sys_close(io);

    return res;
}

static void *exec_thread(void *arg)
{
    Exec *e = arg;

    long long last[e->len];
    IO *ios[e->len];
    for (int i = 0; i < e->len; ++i) {
        ios[i] = NULL;
        last[i] = 0;
    }

    int cnt = 1;
    int *ids = get_ids(e->addrs[0], &cnt);

    for (int i = 0; i < ITERATIONS; ++i) {
        long long time = sys_millis();

        int closed = 0, idx = rand() % e->len, id = rand() % cnt;
        IO *io = ios[idx];

        if (time - last[idx] > 4000) {
            close(io);
            io = conn(ios, idx, e->addrs[idx]);
            closed = 1;
        }

        char fn[MAX_NAME];
        str_print(fn, "/Item?id=%d", ids[id]);
        int res = get(io, fn);
        last[idx] = sys_millis();

        sys_print("%lld,%lld,%s,%d,%d\n",
                  last[idx] / 1000,
                  last[idx] - time,
                  e->addrs[idx],
                  closed,
                  res);
    }

    for (int i = 0; i < e->len; ++i)
        close(ios[i]);

    mem_free(ids);

    return NULL;
}

int main(int argc, char *argv[])
{
    Exec e;

    int err = 0;
    THREADS = str_int(argv[1], &err);
    if (err)
        fail();

    for (int i = 2; i < argc; ++i) {
        e.addrs[i - 2] = argv[i];
        e.len = argc - 2;
    }

    for (int i = 0; i < THREADS; i++)
        sys_thread(exec_thread, &e);

    /* dummy sleep */
    for (;;)
        sys_sleep(30);

}
