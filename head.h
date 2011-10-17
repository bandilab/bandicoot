/*
Copyright 2008-2011 Ostap Cherkashin
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

typedef enum {Int, Real, String, Long} Type;

extern Type type_from_str(const char *t, int *error);
extern char *type_to_str(Type t);

typedef struct {
    int len;
    char *names[MAX_ATTRS];
    Type types[MAX_ATTRS];
} Head;

extern Head *head_new(char *names[], Type types[], int len);
extern Head *head_cpy(Head *h);
extern Head *head_join(Head *l, Head *r, int lpos[], int rpos[], int *len);
extern Head *head_project(Head *h, char *names[], int len);
extern Head *head_rename(Head *h,
                         char *from[],
                         char *to[],
                         int len,
                         int pos[],
                         int *plen);
extern int head_common(Head *l, Head *r, int lpos[], int rpos[]);
extern int head_attr(Head *h, char *name, int *pos, Type *t);
extern int head_find(Head *h, char *name);

extern int head_eq(Head *l, Head *r);
