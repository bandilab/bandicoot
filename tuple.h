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

typedef struct {
    int size;

    struct {
        int len;
        int *size;
        int *off;
    } v;

    /* attribute values start right here. */
} Tuple;

extern Tuple *tuple_new(Value vals[], int len);
extern Tuple *tuple_cpy(Tuple *t);
extern Tuple *tuple_join(Tuple *l, Tuple *r, int lpos[], int rpos[], int len);
extern Tuple *tuple_reord(Tuple *t, int pos[], int len);
extern Value tuple_attr(Tuple *t, int pos);
extern void tuple_free(Tuple *t);
extern int tuple_cmp(Tuple *l, Tuple *r, int lpos[], int rpos[], int len);

typedef struct {
    int pos;
    int len;
    int size;
    Tuple **buf;
} TBuf;

extern TBuf *tbuf_new();
extern TBuf *tbuf_read(IO *io);
extern int tbuf_write(TBuf *b, IO *io);
extern Tuple *tbuf_next(TBuf *b);
extern void tbuf_add(TBuf *b, Tuple *t);
extern void tbuf_reset(TBuf *b);
extern void tbuf_free(TBuf *b);
extern void tbuf_clean(TBuf *b);
