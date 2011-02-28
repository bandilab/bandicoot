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

#include "system_net.c"

extern void sys_die(const char *msg, ...)
{

    DWORD dw = GetLastError();
    LPSTR lpMsgBuf = NULL;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                  FORMAT_MESSAGE_FROM_SYSTEM |
                  FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL,
                  dw,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPSTR) &lpMsgBuf,
                  0,
                  NULL);

    va_list ap;

    va_start(ap, msg);
    vprintf(msg, ap);
    va_end(ap);

    printf("[system error: '%s']\n", str_trim(lpMsgBuf));

    exit(PROC_FAIL);
}

extern IO *_sys_open(const char *path, int mode, int binary);

extern IO *sys_open(const char *path, int mode)
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

extern int net_open()
{
    static int init;
    if (init == 0) {
        WSADATA wsa_data;
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data))
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

extern void net_close(IO *io)
{
    if (closesocket(io->fd) != 0)
        sys_die("sys: cannot close socket\n");
}

extern int net_port(int fd)
{
    struct sockaddr_in addr;
    int size = sizeof(addr);
    if (getsockname(fd, (struct sockaddr*) &addr, &size) == -1)
        sys_die("sys: cannot lookup the socket details\n");

    return ntohs(addr.sin_port);
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
