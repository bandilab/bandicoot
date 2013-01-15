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

int THREADS;

typedef struct {
    char *addrs[CONNS];
    int len;
} Exec;

static void get(IO *io, char *fn)
{
    Http_Args args = {.len = 0};
    if (http_get(io, fn, &args) != 200)
        fail();

    Http_Resp *resp = http_parse_resp(io);
    if (resp == NULL || resp->status != 200)
        fail();

    http_free_resp(resp);
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

static void *exec_thread(void *arg)
{
    Exec *e = arg;

    long long last[e->len];
    IO *ios[e->len];
    for (int i = 0; i < e->len; ++i) {
        ios[i] = NULL;
        last[i] = 0;
    }

    for (int i = 0; i < ITERATIONS; ++i) {
        long long time = sys_millis();

        int closed = 0, idx = rand() % e->len;
        IO *io = ios[idx];

        if (time - last[idx] > 4000) {
            close(io);
            io = conn(ios, idx, e->addrs[idx]);
            closed = 1;
        }

        get(io, "/cnt");
        last[idx] = sys_millis();

        sys_print("%lld,%lld,%s,%d\n",
                  last[idx] / 1000,
                  last[idx] - time,
                  e->addrs[idx],
                  closed);
    }

    for (int i = 0; i < e->len; ++i)
        close(ios[i]);

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
