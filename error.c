/*
Copyright 2012 Ostap Cherkashin

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
#include <stdarg.h>
#include <stdio.h>

#include "error.h"
#include "string.h"
#include "memory.h"

extern Error *error_new(char *fmt, ...)
{
    va_list ap;

    char *msg = NULL;
    va_start(ap, fmt);
    vasprintf(&msg, fmt, ap);
    va_end(ap);

    Error *err = mem_alloc(sizeof(Error) + str_len(msg) + 1);
    err->msg = (char*) (err + 1);
    str_cpy(err->msg, msg);
    mem_free(msg);

    return err;
}
