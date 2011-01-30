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

typedef struct {
    int value;
    void *mutex;
    void *cond;
} Mon;

extern Mon *mon_new();
extern void mon_lock(Mon *m);
extern void mon_unlock(Mon *m);
extern void mon_wait(Mon *m);
extern void mon_signal(Mon *m);
extern void mon_free(Mon *m);
