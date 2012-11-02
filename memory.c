/*
Copyright 2008-2012 Ostap Cherkashin
Copyright 2008-2012 Julius Chrobak

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

#include <stdlib.h>
#include <string.h>

#include "system.h"

extern void *mem_alloc(long long size)
{
    void *p = malloc(size);
    if (p == NULL)
        sys_die("OOM: cannot allocate %lld, exiting\n", size);

    return p;
}

extern void *mem_realloc(void *p, long long nsize)
{
    void *res = realloc(p, nsize);
    if (res == NULL) {
        free(p);
        sys_die("OOM: cannot realloc %p nsize=%lld\n", p, nsize);
    }

    return res;
}

extern void mem_free(void *p)
{
    free(p);
}

extern void mem_cpy(void *dest, const void *src, long long size)
{
    memcpy(dest, src, size);
}

extern void mem_set(void *dest, int val, long long size)
{
    memset(dest, val, size);
}

extern int mem_cmp(const void *l, const void *r, long long size)
{
    return memcmp(l, r, size);
}
