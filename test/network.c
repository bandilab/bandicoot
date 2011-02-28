#include "common.h"

int main(int argc, char *argv[])
{
    if (argc == 1) {
        int p = 0;
        IO *sio = sys_socket(&p);

        char port[16];
        str_print(port, "%d", p);

        char *a[] = {argv[0], port, NULL};
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
        int e = 0;
        int p = str_int(argv[1], &e);
        if (e)
            fail();

        IO *io = sys_connect(p);
        if (sys_write(io, "hello", 6) < 0)
            fail();

        sys_close(io);
    } else
        fail();

    return 0;
}
