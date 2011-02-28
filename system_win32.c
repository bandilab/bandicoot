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
#include <windows.h>
#include <winsock.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "config.h"
#include "system.h"
#include "memory.h"
#include "string.h"

extern void sys_die(const char *msg, ...)
{

    DWORD dw = GetLastError();
    LPVOID lpMsgBuf;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                  FORMAT_MESSAGE_FROM_SYSTEM |
                  FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL,
                  dw,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPTSTR) &lpMsgBuf,
                  0,
                  NULL);

    va_list ap;

    va_start(ap, msg);
    vprintf(msg, ap);
    va_end(ap);

    printf("[system error: '%s']\n", lpMsgBuf);

    exit(PROC_FAIL);
}

extern int _sys_open(const char *path, int mode, int binary);

extern int sys_open(const char *path, int mode)
{
    return _sys_open(path, mode, O_BINARY);
}

extern int sys_exec(char *const a[])
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&pi, sizeof(pi));
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    char c[1024] = "";
    for (int i = 1; a[i] != NULL; ++i)
        str_print(c, "%s %s", c, a[i]);

    if (!CreateProcess(a[0], c, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) 
        sys_die("sys: cannot create process\n");

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return pi.dwProcessId;
}

extern char sys_wait(int pid)
{
    HANDLE ph = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (ph == NULL)
        return -1;

    WaitForSingleObject(ph, INFINITE);

    DWORD result = -1;
    if(!GetExitCodeProcess(ph, &result))
        sys_die("sys: cannot get process %d exit code\n", pid);

    CloseHandle(ph);

    return result;
}

extern void sys_thread(void *(*fn)(void *arg), void *arg)
{
    DWORD id;
    HANDLE h = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)fn, arg, 0, &id);
 
    if (h == NULL)
        sys_die("sys: cannot create a thread\n");
}

static int socket_new()
{
    static int init;
    if (init == 0) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData))
            sys_die("sys: cannot initialise winsock2\n");
    }

    SOCKET fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == INVALID_SOCKET)
        sys_die("sys: cannot create a socket\n");

    char on = 1;
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

    int sz = sizeof(addr);
    if (getsockname(sfd, (struct sockaddr*) &addr, &sz) == -1)
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

extern void sys_close_socket(int fd)
{
    if (closesocket(fd) != 0)
        sys_die("sys: cannot close socket\n");
}

extern int sys_recv(int fd, void *buf, int size)
{
    int r = recv(fd, buf, size, 0);

    return (r == SOCKET_ERROR) ? -1 : r;
}

extern int sys_recvn(int fd, void *buf, int size)
{
    int r, idx = 0;
    do {
        r = recv(fd, buf + idx, size - idx, 0);
        idx += r;
    } while (idx < size && r != SOCKET_ERROR);

    return idx;
}

extern int sys_send(int fd, const void *buf, int size)
{
    int w = send(fd, buf, size, 0);

    return (w == SOCKET_ERROR || w != size) ? -1 : w;
}


extern int sys_accept(int socket)
{
    unsigned int res = accept(socket, NULL, NULL);
    if (res == INVALID_SOCKET)
        sys_die("sys: cannot accept incoming connection\n");

    return res;
}

extern void sys_signals()
{
}

extern Mon *mon_new()
{
    Mon *res = mem_alloc(sizeof(Mon));
    res->value = 0;
    res->mutex = mem_alloc(sizeof(CRITICAL_SECTION));

    if (!InitializeCriticalSectionAndSpinCount(res->mutex, 0))
        sys_die("monitor: initialize critical section failed\n");

    res->cond = CreateEvent(NULL, 1, 0, NULL);
    if (res->cond == NULL)
        sys_die("monitor: create event failed\n");

    return res;
}

extern void mon_lock(Mon *m)
{
    EnterCriticalSection(m->mutex);
}

extern void mon_unlock(Mon *m)
{
    LeaveCriticalSection(m->mutex);
}

extern void mon_wait(Mon *m)
{
    if (!ResetEvent(m->cond))
        sys_die("monitor: reset failed\n");
    LeaveCriticalSection(m->mutex);
    if (WAIT_OBJECT_0 != WaitForSingleObject(m->cond, INFINITE))
        sys_die("monitor: wait failed\n");
    EnterCriticalSection(m->mutex);
}

extern void mon_signal(Mon *m)
{
    if (!SetEvent(m->cond))
        sys_die("monitor: signal failed\n");
}

extern void mon_free(Mon *m)
{
    CloseHandle(m->cond);
    DeleteCriticalSection(m->mutex);
    mem_free(m);
}
