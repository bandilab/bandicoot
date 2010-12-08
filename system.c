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

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>
#include <time.h>

#include "system.h"
#include "memory.h"
#include "string.h"

extern int sys_open(const char *path, int mode)
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

    int fd = open(path, flags, S_IRUSR | S_IWUSR);
    if (fd < 0)
        sys_die("sys: cannot open %s\n", path);

    return fd;
}

extern int sys_exists(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        if (errno == ENOENT)
            return 0;
        else
            sys_die("sys: cannot check %s existance\n", path);
    }

    close(fd);

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

    int fd = sys_open(dest, CREATE | WRITE);
    sys_write(fd, buf, str_len(buf));
    sys_close(fd);
}

extern int sys_read(int fd, void *buf, int size)
{
    int res = read(fd, buf, size);
    if (res == -1)
        sys_die("sys: cannot read %d bytes from fd:%d\n", size, fd);

    return res;
}

extern int sys_readn(int fd, void *buf, int size)
{
    int r, idx = 0;
    do {
        r = sys_read(fd, buf + idx, size - idx);
        idx += r;
    } while (idx < size && r > 0);

    return idx;
}

extern char *sys_load(const char *path)
{
    int len = 0, read = 0, fd = sys_open(path, READ);
    char buf[4096], *res = mem_alloc(1);

    res[0] = '\0';
    while ((read = sys_readn(fd, buf, 4096)) > 0) {
        res = mem_realloc(res, len + read + 1);
        mem_cpy(res + len, buf, read);
        len += read;
        res[len] = '\0';
    }
    sys_close(fd);

    return res;
}

extern void sys_write(int fd, const void *buf, int size)
{
    if (write(fd, buf, size) < 0)
        sys_die("sys: cannot write data\n");
}

extern void sys_close(int fd)
{
    if (close(fd) < 0)
        sys_die("sys: cannot close file descriptor\n");
}

extern char **sys_lsdir(const char *path, int *len)
{
    char *mem = mem_alloc(256);
    int n = 0, off = 0, *offs = mem_alloc(sizeof(int));
    offs[0] = 0;

    DIR *d = opendir(path);
    if (d == NULL)
        sys_die("sys: cannot open directory '%s'\n", path);

    struct dirent *e;
    while ((e = readdir(d)) != 0) {
        off += str_cpy(mem + off, e->d_name) + 1;
        n++;

        mem = mem_realloc(mem, off + 256);
        offs = mem_realloc(offs, (n + 1) * sizeof(int));
        offs[n] = off;
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

extern long sys_millis()
{
    struct timeval t;
    gettimeofday(&t, NULL);

    return t.tv_sec * 1000L + t.tv_usec / 1000L;
}

extern void sys_time(char *buf)
{
    time_t t = time(NULL);
    strftime(buf, 32, "%Y-%m-%d %H:%M:%S UTC", gmtime(&t));
}

extern void sys_die(const char *msg, ...)
{
    va_list ap;

    va_start(ap, msg);
    vprintf(msg, ap);
    va_end(ap);

    printf("[system error: '%s']\n", strerror(errno));

    exit(EXIT_FAILURE);
}

extern int sys_proc(void (*fn)(void *arg), void *arg)
{
    pid_t pid;
    if ((pid = fork()) == 0) {
        fn(arg);
        sys_exit(0);
    }

    if (pid == -1)
        sys_die("sys: cannot create a new process\n");

    int res, status;
    do {
        res = waitpid(pid, &status, WUNTRACED | WCONTINUED);
    } while (res == -1 && errno == EINTR);

    if (res == -1)
        sys_die("sys: wait for child pid %d failed\n", pid);

    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

extern void sys_exit(int status)
{
    exit(status);
}

extern void sys_thread(void *(*fn)(void *arg), void *arg)
{
    pthread_t t;
    if (pthread_create(&t, 0, fn, arg) != 0)
        sys_die("sys: cannot create a thread\n");

    if (pthread_detach(t) != 0)
        sys_die("sys: cannot detach from a thread\n");
}

extern int sys_socket(int port)
{
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1)
        sys_die("sys: cannot create a socket\n");

    int on = 1;
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1)
        sys_die("sys: cannot setsockopt(SO_REUSEADDR)\n");

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sfd, (struct sockaddr*) &addr, sizeof(addr)) == -1)
        sys_die("sys: cannot bind a socket to port %d\n", port);

    int backlog = 50;
    if (listen(sfd, backlog) == -1)
        sys_die("sys: cannot listen (backlog: %d)\n", backlog);

    return sfd;
}

extern int sys_accept(int socket)
{
    int res = accept(socket, NULL, NULL);
    if (res == -1)
        sys_die("sys: cannot accept incoming connection\n");

    return res;
}

extern int sys_empty(const char *dir)
{
    int num_files;
    char **files = sys_lsdir(dir, &num_files);
    /* FIXME: this might not be portable condition */
    int res = (num_files == 2);
    mem_free(files);

    return res;
}
