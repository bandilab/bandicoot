/*
Copyright 2008-2012 Ostap Cherkashin
Copyright 2008-2011 Julius Chrobak

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

typedef struct {
    int size;
    int len;
    char **names;
    char **vols;
    long long *vers;
    TBuf **vals;
} Vars;

extern Vars *vars_new(int len);
extern Vars *vars_read(IO *io);
extern int vars_write(Vars *v, IO *io);
extern int vars_scan(Vars *v, const char *var, long long ver);
extern void vars_add(Vars *v, const char *var, long long ver, TBuf *val);
extern void vars_free(Vars *v);
extern void vars_cpy(Vars *dest, Vars *src);
