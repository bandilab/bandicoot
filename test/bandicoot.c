/*
Copyright 2012 Ostap Cherkashin

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

static void post_books(char *fn)
{
    IO *io = sys_connect("localhost:12345", IO_STREAM);
    Http_Args args = {.len = 0};
    if (http_post(io, fn, &args) != 200)
        fail();

    char *data = "title string,price real\nA,1.0\nB,2.0";
    if (http_chunk(io, data, str_len(data)) != 200)
        fail();

    if (http_chunk(io, NULL, 0) != 200)
        fail();

    Http_Resp *resp = http_parse_resp(io);
    if (resp == NULL || resp->status != 200)
        fail();

    http_free_resp(resp);
    sys_close(io);
}

static void get(char *fn)
{
    IO *io = sys_connect("localhost:12345", IO_STREAM);

    Http_Args args = {.len = 0};
    if (http_get(io, fn, &args) != 200)
        fail();

    Http_Resp *resp = http_parse_resp(io);
    if (resp == NULL || resp->status != 200)
        fail();

    sys_close(io);
}

int main(void)
{
    char *argv[] = {"bin/bandicoot", "start",
                    "-p", "12345",
                    "-d", "bin/volume",
                    "-s", "bin/state",
                    "-c", "test/test_calls.b", NULL};

    int pid = sys_exec(argv);
    sys_sleep(1);

    get("/Return");
    get("/IndirectReturn");
    post_books("/Echo");
    post_books("/IndirectEcho");
    post_books("/TmpReturn");
    post_books("/IndirectTmpReturn");
    post_books("/StoreReturn");
    post_books("/IndirectStoreReturn");
    post_books("/ReRead");
    post_books("/IndirectReRead");

    sys_kill(pid);
}
