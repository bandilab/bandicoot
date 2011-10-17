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

#include "common.h"

static void test_basic(IO *sio, char *exec, char *addr)
{
    char *a[] = {exec, "basic", addr, NULL};
    int pid = sys_exec(a);
    IO *pio = sys_accept(sio, IO_STREAM);

    char buf[16];
    int read = sys_readn(pio, buf, 6);
    if (read != 6 || str_cmp(buf, "hello") != 0)
        fail();

    sys_wait(pid);
    sys_close(pio);
}

static char OK = 48;
static char WAIT = 49;
static char DISCONNECT = 50;
static char ERR_PARTIAL = 51;
static char ERR_COMPLETE = 52;

static void test_proxy(IO *sio, char *exec, char *addr, char cm, char pm)
{
    char cmode[2]; cmode[0] = cm; cmode[1] = '\0';
    char *a1[] = {exec, "client", addr, cmode, NULL};
    int pid1 = sys_exec(a1);
    IO *pio1 = sys_accept(sio, IO_STREAM);

    char pmode[2]; pmode[0] = pm; pmode[1] = '\0';
    char *a2[] = {exec, "processor", addr, pmode, NULL};
    int pid2 = sys_exec(a2);
    IO *pio2 = sys_accept(sio, IO_CHUNK);

    int ccnt = 0, pcnt = 0;
    sys_proxy(pio1, &ccnt, pio2, &pcnt);
    int err = pio1->stop || pio2->stop & ~IO_TERM;
    if (cm == OK && pm == OK) {
        if (err || pcnt == 0)
            fail();
    } else if (cm == WAIT && pm == ERR_COMPLETE) {
        if (!err || pcnt > 0 || pio2->stop & IO_TERM)
            fail();
    } else if (cm == WAIT && pm == ERR_PARTIAL) {
        if (!err || pcnt == 0 || pio2->stop & IO_TERM)
            fail();
    } else if (cm == DISCONNECT && pm == OK) {
        if (!err || pio1->stop & ~IO_CLOSE)
            fail();
    }

    sys_close(pio1);
    sys_wait(pid1);

    sys_close(pio2);
    sys_wait(pid2);
}

static void _basic(char *addr)
{
    IO *io = sys_connect(addr, IO_STREAM);
    if (sys_write(io, "hello", 6) < 0)
        fail();

    sys_close(io);
}

static void _client(char *addr, char *m)
{
    IO *io = sys_connect(addr, IO_STREAM);
    if (sys_write(io, "hello", 6) < 0)
        return;

    char mode = *m;
    if (mode == OK) {
        char buf[6];
        int read = sys_read(io, buf, 6);
        if (read != 6 || str_cmp(buf, "hello") != 0)
            return;
    } else if (mode == WAIT)
        sys_iready(io, -1);

    sys_close(io);
}

static void _processor(char *addr, char *m)
{
    IO *io = sys_connect(addr, IO_CHUNK);
    char buf[6];
    int size = 6, read = sys_read(io, buf, size);
    if (read != size || str_cmp(buf, "hello") != 0)
        fail();

    char mode = *m;
    if (mode != ERR_COMPLETE) {
        if (sys_write(io, "hello", size) != size)
            return;
    }

    if (mode == OK) {
        if (sys_term(io) != 0)
            return;

        sys_iready(io, -1);
    }

    sys_close(io);
}

int main(int argc, char *argv[])
{
    sys_init(0);

    if (argc == 1) {
        int p = 0;
        IO *sio = sys_socket(&p);

        char addr[MAX_ADDR];
        str_print(addr, "127.0.0.1:%d", p);

        test_basic(sio, argv[0], addr);

        test_proxy(sio, argv[0], addr, WAIT, ERR_PARTIAL);
        test_proxy(sio, argv[0], addr, OK, OK);
        test_proxy(sio, argv[0], addr, WAIT, ERR_COMPLETE);
        test_proxy(sio, argv[0], addr, DISCONNECT, OK);

        sys_close(sio);
    } else if (str_cmp(argv[1], "basic") == 0) {
        _basic(argv[2]);
    } else if (str_cmp(argv[1], "client") == 0) {
        _client(argv[2], argv[3]);
    } else if (str_cmp(argv[1], "processor") == 0) {
        _processor(argv[2], argv[3]);
    } else
        fail();

    return 0;
}
