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

/* FIXME: cleanup MAX_* (there are duplicates or bad names + some constants
          are not required) */
#define MAX_PATH 256
#define MAX_REL_TYPES 128
#define MAX_VARS 128
#define MAX_ATTRS 64
#define MAX_TUPLE_SIZE 4096
#define MAX_ARGS 64
#define MAX_NAME 32
#define MAX_RVARS 128
#define MAX_STMTS 128
#define MAX_STRING 1024
#define MAX_BLOCK 65536

#ifndef NULL
#define NULL ((void*) 0)
#endif
