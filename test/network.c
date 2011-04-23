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

int main(int argc, char *argv[])
{
    sys_init(0);

    if (argc == 1) {
        int p = 0;
        IO *sio = sys_socket(&p);

        char addr[MAX_ADDR];
        str_print(addr, "127.0.0.1:%d", p);

        char *a[] = {argv[0], addr, NULL};
        int pid = sys_exec(a);
        IO *pio = sys_accept(sio);

        char buf[16];
        int read = sys_readn(pio, buf, 6);
        if (read != 6 || str_cmp(buf, "hello") != 0)
            fail();

        sys_wait(pid);
        sys_close(pio);
        sys_close(sio);
    } else if (argc == 2) {
        IO *io = sys_connect(argv[1]);
        if (sys_write(io, "hello", 6) < 0)
            fail();

        sys_close(io);
    } else
        fail();

    return 0;
}
