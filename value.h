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
    int size;
    void *data;
} Value;

extern Value val_new_int(int *v);
extern Value val_new_real(double *v);
extern Value val_new_str(char *v);
extern Value val_new_long(long long *v);

extern double val_real(Value v);
extern char *val_str(Value v);
extern long long val_long(Value v);
extern int val_int(Value v);
extern int val_eq(Value l, Value r);
extern int val_bin_enc(void *mem, Value v);
extern int val_to_str(char *dest, Value v, Type t);
