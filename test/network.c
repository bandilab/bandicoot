#include "common.h"

int main(int argc, char *argv[])
{
    if (argc == 1) {
        int p = 0;
        int sfd = sys_socket(&p);

        char port[16];
        str_print(port, "%d", p);

        char *a[] = {argv[0], port, NULL};
        int pid = sys_exec(a);
        int fd = sys_accept(sfd);

        char buf[16];
        int read = sys_recv(fd, buf, 6);

        if (str_cmp(buf, "hello") != 0)
            fail();

        sys_wait(pid);
        sys_close_socket(fd);
        sys_close_socket(sfd);
    } else if (argc == 2) {
        int e = 0;
        int p = str_int(argv[1], &e);
        if (e)
            fail();

        int fd = sys_connect(p);
        int sent = sys_send(fd, "hello", 6);
        sys_close_socket(fd);
    } else
        fail();

    return 0;
}
