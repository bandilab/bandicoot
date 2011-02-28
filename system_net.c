/*
Copyright 2008-2010 Ostap Cherkashin
Copyright 2008-2010 Julius Chrobak

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

/* from system.c */
extern IO *new_io(int fd,
                  int (*read)(struct IO *io, void *buf, int size),
                  int (*write)(struct IO *io, const void *buf, int size),
                  void (*close)(struct IO *io));

/* from system_posix.c or system_win32.c */
extern int net_open();
extern void net_close(IO *io);
extern int net_port(int fd);

static int net_read(IO *io, void *buf, int size)
{
    int r = recv(io->fd, buf, size, 0);
    return r < 0 ? -1 : r;
}

static int net_write(IO *io, const void *buf, int size)
{
    int w = send(io->fd, buf, size, 0);
    return w != size ? -1 : w;
}

extern IO *sys_socket(int *port)
{
    int sfd = net_open();

    struct sockaddr_in addr;
    unsigned int size = sizeof(addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(*port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sfd, (struct sockaddr*) &addr, size) == -1)
        sys_die("sys: cannot bind a socket to port %d\n", *port);

    int backlog = 128;
    if (listen(sfd, backlog) == -1)
        sys_die("sys: cannot listen (backlog: %d)\n", backlog);

    *port = net_port(sfd);

    return new_io(sfd, net_read, net_write, net_close);
}

extern IO *sys_accept(IO *sock)
{
    int fd = accept(sock->fd, NULL, NULL);
    if (fd < 0)
        sys_die("sys: cannot accept incoming connection\n");

    return new_io(fd, net_read, net_write, net_close);
}

extern IO *sys_connect(int port)
{
    int fd = net_open();

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(fd, (struct sockaddr*) &addr, sizeof(addr)) == -1)
        sys_die("sys: cannot connect to port %d\n", port);

    return new_io(fd, net_read, net_write, net_close);
}
