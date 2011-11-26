/*
Copyright 2008-2011 Ostap Cherkashin
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

static void init_gen(Rel *r, Vars *rvars, Arg *arg) {}
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

static const char *HTTP_REQ = "%s /%s HTTP/1.1\r\n"
                              "Content-Length: %d\r\n\r\n";

extern int http_req(const char *method, const char *addr, const char *fn,
                    const char *data, int size)
{
    int ok = 0;

    IO *io = sys_connect(addr, IO_STREAM);
    if (io != NULL) {
        char head[64];
        int head_size = str_print(head, HTTP_REQ, method, fn, size);
        if (sys_write(io, head, head_size) == head_size &&
            sys_write(io, data, size) == size)
        {
            char buf[8192];
            int read = sys_read(io, buf, sizeof(buf));
            if (read > 0) {
                buf[read - 1] = '\0';

                ok = str_idx(buf, "200 OK") > -1;
            }
        }

        sys_close(io);
    }

    return ok;
}

extern void gen_rel_str(int num_chunks, int chunk_size,
                        char *out_data[], int out_size[])
{
    for (int i = 0; i < num_chunks; ++i) {
        int start = i * chunk_size;
        Rel *r = gen_rel(start, start + chunk_size);

        char *buf = NULL;
        int size = 0, len = 0, block = 0, j = 0;
        do {
            buf = mem_realloc(buf, size += MAX_BLOCK);
            block = rel_unpack(r, buf + len, MAX_BLOCK, j++);
            len += block;
        } while (block > 0);

        out_data[i] = buf;
        out_size[i] = len;
        rel_free(r);
    }
}
