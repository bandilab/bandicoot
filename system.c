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

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "config.h"
#include "system.h"
#include "memory.h"
#include "string.h"

extern int _sys_open(const char *path, int mode, int binary)
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

    int fd = open(path, flags | binary, S_IRUSR | S_IWUSR);
    if (fd < 0)
        sys_die("sys: cannot open %s\n", path);

    return fd;
}

extern void sys_close(int fd)
{
    if (close(fd) < 0)
        sys_die("sys: cannot close file descriptor\n");
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

    int fd = sys_open(dest, CREATE | WRITE);
    sys_write(fd, buf, str_len(buf));
    sys_close(fd);
    mem_free(buf);
}

extern int sys_read(int fd, void *buf, int size)
{
    int res = read(fd, buf, size);
    if (res < 0)
        sys_die("sys: cannot read %d bytes from fd:%d\n", size, fd);

    return res;
}

extern int readn(int fd,
                 void *buf,
                 int size,
                 int (*rfn)(int, void*, int))
{
    int r, idx = 0;
    do {
        r = rfn(fd, buf + idx, size - idx);
        idx += r;
    } while (idx < size && r > 0);

    return idx;
}

extern int sys_readn(int fd, void *buf, int size)
{
    return readn(fd, buf, size, sys_read);
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

extern int sys_write(int fd, const void *buf, int size)
{
    int w = write(fd, buf, size);

    return (w < 0 || w != size) ? -1 : w;
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

extern void sys_exit(char status)
{
    exit(status);
}
