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
    IO *pio = sys_accept(sio);

    char buf[16];
    int read = sys_readn(pio, buf, 6);
    if (read != 6 || str_cmp(buf, "hello") != 0)
        fail();

    sys_wait(pid);
    sys_close(pio);
}

static void test_exchange(IO *sio, char *exec, char *addr, char *mode)
{
    char *a1[] = {exec, "exchange1", addr, NULL};
    int pid1 = sys_exec(a1), cnt1 = 0;
    IO *pio1 = sys_accept(sio);

    char *a2[] = {exec, "exchange2", addr, mode, NULL};
    int pid2 = sys_exec(a2), cnt2 = 0;
    IO *pio2 = sys_accept(sio);

    sys_exchange(pio1, &cnt1, pio2, &cnt2);
    if (str_cmp(mode, "success") == 0 && (cnt1 == 0 || cnt2 == 0))
        fail();
    if (str_cmp(mode, "fail") == 0 && (cnt1 == 0 || cnt2 != 0))
        fail();

    sys_wait(pid1);
    sys_close(pio1);

    sys_wait(pid2);
    sys_close(pio2);
}

static void _basic(char *addr)
{
    IO *io = sys_connect(addr);
    if (sys_write(io, "hello", 6) < 0)
        fail();

    sys_close(io);
}

static void _exchange1(char *addr, char *mode)
{
    IO *io = sys_connect(addr);
    if (sys_write(io, "hello", 6) < 0)
        fail();

    if (str_cmp(mode, "success") == 0) {
        char buf[16];
        int read = sys_read(io, buf, 6);
        if (read != 6 || str_cmp(buf, "hello") != 0)
            fail();
    }

    sys_close(io);
}

static void _exchange2(char *addr, char *mode)
{
    IO *io = sys_connect(addr);
    if (str_cmp(mode, "success") == 0) {
        char buf[16];
        int read = sys_read(io, buf, 6);
        if (read != 6 || str_cmp(buf, "hello") != 0)
            fail();

        if (sys_write(io, "hello", 6) < 0)
            fail();
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
        test_exchange(sio, argv[0], addr, "success");
        test_exchange(sio, argv[0], addr, "fail");

        sys_close(sio);
    } else if (str_cmp(argv[1], "basic") == 0) {
        _basic(argv[2]);
    } else if (str_cmp(argv[1], "exchange1") == 0) {
        _exchange1(argv[2], argv[3]);
    } else if (str_cmp(argv[1], "exchange2") == 0) {
        _exchange2(argv[2], argv[3]);
    } else
        fail();

    return 0;
}
