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

extern void str_init(); /* required to initialized STR_TO_SID array */

extern long long str_len(const char *s);
extern int str_cmp(const char *l, const char *r);
extern long long str_cpy(char *dest, const char *src);
extern int str_print(char *res, const char *msg, ...);
extern unsigned int str_uint(const char *s, int *error);
extern int str_int(const char *s, int *error);
extern long long str_idx(const char *s, const char *seq);
extern int str_match(const char *s1, const char *s2, char until);
extern char *str_dup(const char *src);
extern char *str_trim(char *s);
extern int str_split(char *s, const char *delims, char *out[], int max);
extern char **str_split_big(char *s, const char *delims, int *parts);
extern double str_real(const char *s, int *error);
extern long long str_long(const char *s, int *error);
extern unsigned long long str_ulong(const char *s, int *error);
extern int str_from_sid(char *dest, long long sid);
extern long long str_to_sid(char *str);
extern char *str_urldecode(char *src);
extern char *str_urlencode(char *src);
extern int str_hexdecode(const char *s, int *error);
