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

static int glog;

/* defined in system_posix.c or system_win32.c */
static void net_close(IO *io);
static int net_port(int fd);

static IO *new_io(int fd,
                  int (*read)(struct IO *io, void *buf, int size),
                  int (*write)(struct IO *io, const void *buf, int size),
                  void (*close)(struct IO *io))
{
    IO *io = mem_alloc(sizeof(IO));
    io->fd = fd;
    io->read = read;
    io->write = write;
    io->close = close;

    return io;
}

static int fs_read(IO *io, void *buf, int size)
{
    int res = read(io->fd, buf, size);
    if (res < 0)
        sys_die("sys: cannot read %d bytes from file %d\n", size, io->fd);

    return res;
}

static int fs_write(IO *io, const void *buf, int size)
{
    int w = write(io->fd, buf, size);
    if (w != size)
        sys_die("sys: cannot write %d bytes to file %d\n", size, io->fd);

    return w;
}

static void fs_close(IO *io)
{
    if (close(io->fd) < 0)
        sys_die("sys: cannot close file descriptor\n");
}

static int net_open()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    char on[sizeof(int)];
    mem_set(on, 1, sizeof(on));
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, on, sizeof(on)) == -1)
        return -1;

    mem_set(on, 1, sizeof(on));
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, on, sizeof(on)) == -1)
        return -1;

    return fd;
}

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

static IO *_sys_open(const char *path, int mode, int binary)
{
    int flags = 0;
    if ((mode & READ) && (mode & WRITE))
        flags |= O_RDWR;
    else if (mode & READ)
        flags |= O_RDONLY;
    else if (mode & WRITE)
        flags |= O_WRONLY;
    else
        sys_die("sys: bad open mode (%x)\n", mode);

    if (mode & CREATE)
        flags |= O_CREAT;

    if (mode & TRUNCATE)
        flags |= O_TRUNC;

    int fd = open(path, flags | binary, S_IRUSR | S_IWUSR);
    if (fd < 0)
        sys_die("sys: cannot open %s\n", path);

    return new_io(fd, fs_read, fs_write, fs_close);
}

extern int sys_read(IO *io, void *buf, int size)
{
    if (size == 0)
        return 0;

    return io->read(io, buf, size);
}

extern int sys_readn(IO *io, void *buf, int size)
{
    if (size == 0)
        return 0;

    int r, idx = 0;
    do {
        r = io->read(io, buf + idx, size - idx);
        idx += r;
    } while (r > 0 && idx < size);

    return idx;
}

extern int sys_write(IO *io, const void *buf, int size)
{
    if (size == 0)
        return 0;

    return io->write(io, buf, size);
}

extern void sys_close(IO *io)
{
    io->close(io);
    mem_free(io);
}

extern int sys_iready(IO *io, int sec)
{
    struct timeval tv = {.tv_sec = sec, .tv_usec = 0};

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(io->fd, &rfds);

    int ready = select(io->fd + 1, &rfds, NULL, NULL, &tv);
    if (ready < 0 && errno != EINTR)
        sys_die("sys: select failed\n");

    return ready == 1 && FD_ISSET(io->fd, &rfds);
}

extern void sys_address(char *result, int port)
{
    if (gethostname(result, MAX_ADDR) < 0)
        sys_die("sys: cannot lookup the local host name\n");

    str_print(result + str_len(result), ":%d", port);
}

static struct sockaddr_in sys_address_dec(const char *address)
{
    int colon = str_idx(address, ":");
    if (colon < 0)
        sys_die("sys: invalid address '%s'\n", address);
    colon += 1;

    char host[colon];
    mem_cpy(host, address, colon);
    host[colon - 1] = '\0';

    struct addrinfo hints;
    mem_set(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *info = NULL;
    if (getaddrinfo(host, address + colon, &hints, &info))
        sys_die("sys: address lookup failure '%s'\n", address);

    /* FIXME: we pick up only the first address from the list. */

    struct sockaddr_in res;
    mem_cpy(&res, info->ai_addr, sizeof(res));
    freeaddrinfo(info);

    return res;
}

extern IO *sys_socket(int *port)
{
    int sfd = net_open();
    if (sfd < 0)
        sys_die("sys: cannot create a socket for port %d\n", *port);

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

static IO *_sys_connect(struct sockaddr_in addr)
{
    IO *res = NULL;
    int fd = net_open();
    int sz = sizeof(addr);
    if (fd >= 0 && connect(fd, (struct sockaddr*) &addr, sz) != -1)
        res = new_io(fd, net_read, net_write, net_close);

    return res;
}

extern IO *sys_connect(const char *address)
{
    struct sockaddr_in addr = sys_address_dec(address);
    IO *res = _sys_connect(addr);
    if (res == NULL)
        sys_die("sys: cannot connect to %s:%d\n",
                inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

    return res;
}

/* FIXME: this method dies in case of a name lookup failure */
extern IO *sys_try_connect(const char *address)
{
    struct sockaddr_in addr = sys_address_dec(address);
    return _sys_connect(addr);
}

extern int sys_exists(const char *path)
{
    struct stat st;

    int res = stat(path, &st);
    if (res == -1) {
        if (errno == ENOENT)
            return 0;
        else
            sys_die("sys: cannot check %s existance\n", path);
    }

    return 1;
}

extern void sys_remove(const char *path)
{
    if (unlink(path) == -1)
        sys_die("sys: cannot remove %s\n", path);
}

extern void sys_move(const char *dest, const char *src)
{
    if (rename(src, dest) == -1)
        sys_die("sys: cannot move %s -> %s\n", src, dest);
}

extern void sys_cpy(const char *dest, const char *src)
{
    char *buf = sys_load(src);
    if (sys_exists(dest))
        sys_remove(dest);

    IO *io = sys_open(dest, CREATE | WRITE);
    sys_write(io, buf, str_len(buf));
    sys_close(io);
    mem_free(buf);
}

extern char *sys_load(const char *path)
{
    int len = 0, read = 0;
    IO *io = sys_open(path, READ);
    char buf[4096], *res = mem_alloc(1);

    res[0] = '\0';
    while ((read = sys_readn(io, buf, 4096)) > 0) {
        res = mem_realloc(res, len + read + 1);
        mem_cpy(res + len, buf, read);
        len += read;
        res[len] = '\0';
    }
    sys_close(io);

    return res;
}

extern int sys_empty(const char *dir)
{
    int num_files;
    char **files = sys_list(dir, &num_files);
    int res = num_files == 0;
    mem_free(files);

    return res;
}

extern char **sys_list(const char *path, int *len)
{
    char *mem = mem_alloc(MAX_FILE_PATH);
    int n = 0, off = 0, *offs = mem_alloc(sizeof(int));
    offs[0] = 0;

    DIR *d = opendir(path);
    if (d == NULL)
        sys_die("sys: cannot open directory '%s'\n", path);

    struct dirent *e;
    while ((e = readdir(d)) != 0) {
        if (str_cmp(e->d_name, ".") != 0 && str_cmp(e->d_name, "..") != 0) {
            off += str_cpy(mem + off, e->d_name) + 1;
            n++;

            mem = mem_realloc(mem, off + MAX_FILE_PATH);
            offs = mem_realloc(offs, (n + 1) * sizeof(int));
            offs[n] = off;
        }
    }
    closedir(d);

    char **res = mem_alloc(n * sizeof(void*) + off);
    mem_cpy(res + n, mem, off);

    int i = 0;
    for (; i < n; ++i)
        res[i] = ((void*) res) + n * sizeof(void*) + offs[i];

    *len = n;

    mem_free(mem);
    mem_free(offs);

    return res;
}

extern void sys_print(const char *msg, ...)
{
    va_list ap;

    va_start(ap, msg);
    vfprintf(stdout, msg, ap);
    fflush(stdout);
    va_end(ap);
}

extern void sys_log(char module, const char *msg, ...)
{
    if (!glog)
        return;

    char time[32], all[32 + str_len(msg)];
    sys_time(time);
    str_print(all, "[%c] %s, %s", module, time, msg);

    va_list ap;
    va_start(ap, msg);
    vfprintf(stdout, all, ap);
    fflush(stdout);
    va_end(ap);
}

extern long sys_millis()
{
    struct timeval t;
    gettimeofday(&t, NULL);

    return t.tv_sec * 1000L + t.tv_usec / 1000L;
}

/* FIXME: time and gmtime methods are not thread-safe */
extern void sys_time(char *buf)
{
    time_t t = time(NULL);
    strftime(buf, 32, "%Y-%m-%d %H:%M:%S UTC", gmtime(&t));
}

extern void sys_exit(char status)
{
    exit(status);
}
