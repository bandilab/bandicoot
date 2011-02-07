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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "config.h"
#include "system.h"
#include "memory.h"
#include "string.h"

extern char sys_proc(void (*fn)(void *arg), void *arg)
{
    sys_die("sys_proc: to be implemented\n");
    return '0';
}

extern void sys_thread(void *(*fn)(void *arg), void *arg)
{
    sys_die("sys_thread: to be implemented\n");
}


extern int sys_socket(int port)
{
    sys_die("sys_socket: to be implemented\n");
    return 0;
}

extern int sys_accept(int socket)
{
    sys_die("sys_accept: to be implemented\n");
    return 0;
}

extern void sys_signals()
{
    sys_die("sys_signals: to be implemented\n");
}

extern Mon *mon_new()
{
    sys_die("mon_new: to be implemented\n");
    return NULL;
}

extern void mon_lock(Mon *m)
{
    sys_die("mon_lock: to be implemented\n");
}

extern void mon_unlock(Mon *m)
{
    sys_die("mon_unlock: to be implemented\n");
}

extern void mon_wait(Mon *m)
{
    sys_die("mon_wait: to be implemented\n");
}

extern void mon_signal(Mon *m)
{
    sys_die("mon_signal: to be implemented\n");
}

extern void mon_free(Mon *m)
{
    sys_die("mon_free: to be implemented\n");
}
