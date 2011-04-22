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

extern void str_init(); /* required to initialized STR_TO_SID array */

extern int str_len(const char *s);
extern int str_cmp(const char *l, const char *r);
extern int str_cpy(char *dest, const char *src);
extern int str_print(char *res, const char *msg, ...);
extern unsigned int str_uint(const char *s, int *error);
extern int str_int(const char *s, int *error);
extern int str_idx(const char *s, const char *seq);
extern char *str_dup(const char *src);
extern char *str_trim(char *s);
/* TODO: most of the str_split use cases know how many indexes expected,
          str_split_big is only required to split very large relational inputs

extern char **str_split_big(char *s, char delim, int *parts);
extern int str_split(char *s, char delim, char *idx[], int max);
*/
extern char **str_split(char *s, char delim, int *parts);
extern double str_real(const char *s, int *error);
extern long long str_long(const char *s, int *error);
extern unsigned long long str_ulong(const char *s, int *error);
extern int str_from_sid(char *dest, long sid);
extern long str_to_sid(char *str);
