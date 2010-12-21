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

struct Sum {
    int cnt;
    int pos;

    Type type;
    union {
        int i;
        double d;
        long long l;
    } def;

    union {
        int i;
        double d;
        long long l;
    } res;

    void *ctxt;

    void (*reset)(struct Sum *self);
    void (*update)(struct Sum *self, Tuple *t);
};
typedef struct Sum Sum;

#define sum_reset(s) (s->reset(s))
#define sum_update(s, t) (s->update(s, t))

extern Sum *sum_cnt();
extern Sum *sum_avg(int pos, Type t, Value def);
extern Sum *sum_min(int pos, Type t, Value def);
extern Sum *sum_max(int pos, Type t, Value def);
extern Sum *sum_add(int pos, Type t, Value def);

extern Value sum_value(Sum *s);
