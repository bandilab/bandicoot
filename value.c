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

#include "config.h"
#include "memory.h"
#include "string.h"
#include "number.h"
#include "head.h"
#include "value.h"

static Value val_new(int size, void *data)
{
    Value res = {.size = size, .data = data};
    return res;
}

extern Value val_new_int(int *v)
{
    return val_new(sizeof(int), v);
}

extern Value val_new_real(double *v)
{
    return val_new(sizeof(double), v);
}

extern Value val_new_str(char *v)
{
    return val_new(str_len(v) + 1, v);
}

extern Value val_new_long(long long *v)
{
    return val_new(sizeof(long long), v);
}

extern int val_int(Value v)
{
    return int_dec(v.data);
}

extern double val_real(Value v)
{
    return real_dec(v.data);
}

extern char *val_str(Value v)
{
    return v.data;
}

extern long long val_long(Value v)
{
    return long_dec(v.data);
}

extern int val_cmp(Value l, Value r)
{
    if (l.size == r.size)
        return mem_cmp(l.data, r.data, l.size);

    return l.size > r.size ? 1 : -1;
}

extern int val_bin_enc(void *mem, Value v)
{
    unsigned char *dest = mem;
    int i;
    for (i = 0; i < v.size; ++i)
        dest[i] = ((unsigned char*) v.data)[i];

    return i;
}

extern char *val_to_str(Value v, Type t, int *len)
{
    char *res = mem_alloc(512);
    int required = 0;
    *len = 0;
    switch (t) {
        case Int:
            *len = str_print(res, "%d", val_int(v));
            break;
        case Real:
            *len = str_print(res, "%g", val_real(v));
            break;
        case Long:
            *len = str_print(res, "%lld", val_long(v));
            break;
        case String:
            required = str_len(val_str(v)) + 1;
            if (required > 512)
                res = mem_realloc(res, required);

            *len = str_cpy(res, val_str(v));
            break;
    }
    res[*len] = '\0';

    return res;
}
