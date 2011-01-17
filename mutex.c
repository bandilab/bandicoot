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
#include "mutex.h"
#include "system.h"

extern long mutex_new()
{
    void *res = mem_alloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(res, 0);

    return (long) res;
}

extern void mutex_lock(long m)
{
    int res = pthread_mutex_lock((pthread_mutex_t*) m);
    if (res != 0) {
        errno = res;
        sys_die("MUTEX: lock failed\n");
    }
}

extern void mutex_unlock(long m)
{
    int res = pthread_mutex_unlock((pthread_mutex_t*) m);
    if (res != 0) {
        errno = res;
        sys_die("MUTEX: unlock failed\n");
    }
}

extern void mutex_close(long m)
{
    pthread_mutex_t *mutex = (pthread_mutex_t*) m;
    int res = pthread_mutex_destroy(mutex);
    if (res != 0) {
        errno = res;
        sys_die("MUTEX: destroy failed\n");
    }

    mem_free(mutex);
}

extern Sem sem_new(int val)
{
    Sem l = {.lock = mutex_new(), .val = val};
    return l;
}

extern void sem_dec(Sem *l)
{
    int cont = 1;
    while (cont) {
        mutex_lock(l->lock);
        if (l->val > 0) {
            l->val--;
            cont = 0;
        }
        mutex_unlock(l->lock);
        sys_yield();
    }
}

extern void sem_inc(Sem *l)
{
    mutex_lock(l->lock);
    l->val++;
    mutex_unlock(l->lock);
}

extern void sem_wait(Sem *l, int val)
{
    while (val > 0) {
        mutex_lock(l->lock);
        if (val == l->val)
            val = 0;
        mutex_unlock(l->lock);
        sys_yield();
    }
}

extern void sem_close(Sem *l)
{
    mutex_close(l->lock);
}
