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
#include <pthread.h>
#include <time.h>
#include <signal.h>

#include "config.h"
#include "system.h"
#include "memory.h"
#include "string.h"

extern int _sys_open(const char *path, int mode, int binary);

extern int sys_open(const char *path, int mode)
{
    return _sys_open(path, mode, 0x0);
}

extern int sys_exec(char *const argv[])
{
    pid_t pid;
    if ((pid = fork()) == 0)
        execv(argv[0], argv);
    else if (pid == -1)
        sys_die("sys: cannot create a child process\n");

    return pid;
}

extern char sys_wait(int pid)
{
    int res, status;
    do {
        res = waitpid(pid, &status, WUNTRACED | WCONTINUED);
    } while (res == -1 && errno == EINTR);

    if (res == -1)
        sys_die("sys: wait for child pid %d failed\n", pid);

    return WIFEXITED(status) ? WEXITSTATUS(status) : PROC_FAIL;
}

extern void sys_thread(void *(*fn)(void *arg), void *arg)
{
    pthread_t t;
    if (pthread_create(&t, 0, fn, arg) != 0)
        sys_die("sys: cannot create a thread\n");

    if (pthread_detach(t) != 0)
        sys_die("sys: cannot detach from a thread\n");
}

static int socket_new()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
        sys_die("sys: cannot create a socket\n");

    int on = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1)
        sys_die("sys: cannot setsockopt(SO_REUSEADDR)\n");

    return fd;
}

extern int sys_socket(int *port)
{
    int sfd = socket_new();

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

    if (getsockname(sfd, (struct sockaddr*) &addr, &size) == -1)
        sys_die("sys: cannot lookup the socket details\n");

    *port = ntohs(addr.sin_port);

    return sfd;
}

extern int sys_connect(int port)
{
    int fd = socket_new();

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(fd, (struct sockaddr*) &addr, sizeof(addr)) == -1)
        sys_die("sys: cannot connect to port %d\n", port);

    return fd;
}
extern int sys_accept(int socket)
{
    int res = accept(socket, NULL, NULL);
    if (res == -1)
        sys_die("sys: cannot accept incoming connection\n");

    return res;
}

extern void sys_signals()
{
    signal(SIGPIPE, SIG_IGN);
}

extern Mon *mon_new()
{
    Mon *res = mem_alloc(sizeof(Mon) +
                         sizeof(pthread_mutex_t) +
                         sizeof(pthread_cond_t));

    res->value = 0;
    res->mutex = res + 1;
    res->cond = res->mutex + sizeof(pthread_mutex_t);

    pthread_mutex_init(res->mutex, NULL);
    pthread_cond_init(res->cond, NULL);

    return res;
}

extern void mon_lock(Mon *m)
{
    int res = pthread_mutex_lock(m->mutex);
    if (res != 0) {
        errno = res;
        sys_die("monitor: lock failed\n");
    }
}

extern void mon_unlock(Mon *m)
{
    int res = pthread_mutex_unlock(m->mutex);
    if (res != 0) {
        errno = res;
        sys_die("monitor: unlock failed\n");
    }
}

extern void mon_wait(Mon *m)
{
    int res = pthread_cond_wait(m->cond, m->mutex);
    if (res != 0) {
        errno = res;
        sys_die("monitor: wait failed\n");
    }
}

extern void mon_signal(Mon *m)
{
    int res = pthread_cond_signal(m->cond);
    if (res != 0) {
        errno = res;
        sys_die("monitor: signal failed\n");
    }
}

extern void mon_free(Mon *m)
{
    int res = pthread_mutex_destroy(m->mutex);
    if (res != 0) {
        errno = res;
        sys_die("monitor: mutex destroy failed\n");
    }

    res = pthread_cond_destroy(m->cond);
    if (res != 0) {
        errno = res;
        sys_die("monitor: cond destroy failed\n");
    }

    mem_free(m);
}
