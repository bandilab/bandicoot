/*
Copyright 2008-2012 Ostap Cherkashin
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

#define _POSIX_SOURCE
#define _BSD_SOURCE

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <poll.h>

#include "config.h"
#include "system.h"
#include "memory.h"
#include "string.h"

#include "system.c"

extern void sys_init(int log)
{
    errno = 0;
    signal(SIGPIPE, SIG_IGN);
    str_init();
    glog = log;
}

extern void sys_die(const char *msg, ...)
{
    va_list ap;

    va_start(ap, msg);
    vprintf(msg, ap);
    va_end(ap);

    if (errno)
        printf("[system error: '%s']\n", strerror(errno));

    exit(PROC_FAIL);
}

extern IO *sys_open(const char *path, int mode)
{
    return _sys_open(path, mode, 0);
}

extern int sys_exec(char *const argv[])
{
    pid_t pid;
    if ((pid = fork()) == 0) {
        execvp(argv[0], argv);
        sys_die("sys: execv of %s failed\n", argv[0]);
    } else if (pid < 0)
        sys_die("sys: cannot create a child process\n");

    return pid;
}

extern int sys_kill(int pid)
{
    return kill(pid, SIGKILL);
}

extern char sys_wait(int pid)
{
    int res, status;
    do {
        res = waitpid(pid, &status, WUNTRACED | WCONTINUED);
    } while (res < 0 && errno == EINTR);

    if (res < 0)
        sys_die("sys: wait for child pid %d failed\n", pid);

    return WIFEXITED(status) ? WEXITSTATUS(status) : PROC_FAIL;
}

extern void sys_sleep(int secs)
{
    sleep(secs);
}

extern void sys_thread(void *(*fn)(void *arg), void *arg)
{
    pthread_t t;
    if (pthread_create(&t, NULL, fn, arg) != 0)
        sys_die("sys: cannot create a thread\n");

    if (pthread_detach(t) != 0)
        sys_die("sys: cannot detach from a thread\n");
}

extern IO *sys_accept(IO *sock, int chunked)
{
    int fd = -1;
    do {
        fd = accept(sock->fd, NULL, NULL);
    } while ((fd < 0) && (errno == ECONNABORTED));

    if (fd < 0)
        sys_die("sys: cannot accept incoming connection\n");

    return new_net_io(fd, chunked);
}

extern void net_close(IO *io)
{
    int res = shutdown(io->fd, SHUT_RDWR);
    if (res < 0 && errno != ENOTCONN)
        sys_die("sys: cannot shutdown socket\n");

    res = close(io->fd);
    if (res < 0)
        sys_die("sys: cannot close socket\n");
}

extern int net_port(int fd)
{
    struct sockaddr_in addr;
    unsigned int size = sizeof(addr);
    if (getsockname(fd, (struct sockaddr*) &addr, &size) < 0)
        sys_die("sys: cannot lookup the socket details\n");

    return ntohs(addr.sin_port);
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

extern void mon_wait(Mon *m, int ms)
{
    int res = 0;
    if (ms < 0) {
        res = pthread_cond_wait(m->cond, m->mutex);
    } else {
        long long end = sys_millis() + ms;
        struct timespec timeout = {
            .tv_sec = end / 1000LL,
            .tv_nsec = (end % 1000LL) * 1000000LL
        };

        res = pthread_cond_timedwait(m->cond, m->mutex, &timeout);
    }

    if (res != 0 && res != ETIMEDOUT) {
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
