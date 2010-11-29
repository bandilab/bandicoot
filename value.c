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

extern int val_eq(Value l, Value r)
{
    return (l.size == r.size) ? mem_cmp(l.data, r.data, l.size) == 0 : 0;
}

extern int val_bin_enc(void *mem, Value v)
{
    unsigned char *dest = mem;
    int i;
    for (i = 0; i < v.size; ++i)
        dest[i] = ((unsigned char*) v.data)[i];

    return i;
}

extern int val_to_str(char *dest, Value v, Type t)
{
    int res = -1;
    switch (t) {
        case Int:
            res = str_print(dest, "%d", val_int(v));
            break;
        case Real:
            res = str_print(dest, "%g", val_real(v));
            break;
        case Long:
            res = str_print(dest, "%lld", val_long(v));
            break;
        case String:
            res = str_cpy(dest, val_str(v));
            break;
    }

    return res;
}
