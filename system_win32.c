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
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "config.h"
#include "system.h"
#include "memory.h"
#include "string.h"

extern int sys_exec(char *const argv[])
{
    sys_die("sys_exec: to be implemented\n");
    return '0';
}

extern char sys_wait(int pid)
{
    sys_die("sys_wait: to be implemented\n");
    return 0;
}

extern void sys_thread(void *(*fn)(void *arg), void *arg)
{
    DWORD id;
    HANDLE h = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)fn, arg, 0, &id);
 
    if (h == NULL)
        sys_die("sys: cannot create a thread\n");
}

extern int sys_socket(int *port)
{
    sys_die("sys_socket: to be implemented\n");
    return 0;
}

extern int sys_connect(int port)
{
    sys_die("sys_connect: to be implemented\n");
    return 0;
}

extern int sys_accept(int socket)
{
    sys_die("sys_accept: to be implemented\n");
    return 0;
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
