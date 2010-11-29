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

#include "common.h"

#define NUM_CHUNKS 100
#define CHUNK_SZ 173

static void test_realloc()
{
    int i, size = CHUNK_SZ;
    int *p = mem_alloc(size);

    for (i = 0; i < NUM_CHUNKS; i++) {
        p = (int*) mem_realloc(p, size);
        *(p + size / sizeof(int) - 1) = 345;
        size += CHUNK_SZ;
    }

    mem_free(p);
}

static void test_cmp()
{
    unsigned char i[sizeof(int)] = {0, 0, 1, 89};
    unsigned char j[sizeof(int)] = {0, 0, 1, 91};

    if (mem_cmp(i, j, sizeof(int)) >= 0)
        fail();
}

int main()
{
    test_realloc();
    test_cmp();
}
