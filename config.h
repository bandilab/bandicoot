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

/* FIXME: temporary until we implement VOLUME process with proper id */
#define VOLUME_ID 1L

#define MAX_FILE_PATH 256

/* maximum number of named relational type declarations */
#define MAX_TYPES 128

/* maximum number of attributes per relational type */
#define MAX_ATTRS 64

/* maximum length of indetifiers */
#define MAX_NAME 32

/* maximum number of global variables and re-reads of temporary/input
   variables */
#define MAX_VARS 128

/* maximum number of statements per function */
#define MAX_STMTS 128

/* maximum length of a string as constant */
#define MAX_STRING 1024

/* size of block for IO operations */
#define MAX_BLOCK 65536

/* maximum number of volume nodes in a cluster per variable */
#define MAX_VOLS 8

#ifndef NULL
#define NULL ((void*) 0)
#endif
