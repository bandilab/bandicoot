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

#include <pthread.h>
#include <errno.h>

#include "memory.h"
#include "monitor.h"
#include "system.h"

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
