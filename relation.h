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
    int len;
    char **names;
    char **vols;
    long *vers;
} Vars;

extern Vars *vars_new(int len);
extern Vars *vars_read(IO *io);
extern int vars_write(Vars *v, IO *io);
extern int vars_scan(Vars *v, const char *var, long ver);
extern void vars_put(Vars *v, const char *var, long ver);
extern void vars_free(Vars *v);
extern void vars_cpy(Vars *dest, Vars *src);

struct Rel {
    Head *head;
    TBuf *body;
    void *ctxt;

    void (*init)(struct Rel *self, Vars *rvars, Arg *arg);
    void (*free)(struct Rel *self);
};

typedef struct Rel Rel;

static void rel_init(Rel *r, Vars *rvars, Arg *arg)
{
    if (r->init != NULL)
        r->init(r, rvars, arg);
    if (r->body != NULL)
        tbuf_reset(r->body);
}

static void rel_free(Rel *r)
{
    r->free(r);
    if (r->body != NULL)
        tbuf_free(r->body);

    mem_free(r->head);
    mem_free(r);
}

static Tuple *rel_next(Rel *r)
{
    return tbuf_next(r->body);
}

extern Rel *rel_param(Head *head);
extern Rel *rel_clone(Rel *r);
extern Rel *rel_load(Head *head, const char *name);
extern Rel *rel_err(int code, char *msg);

/* relations passed to both rel_store & rel_eq must be rel_init()'ed */
extern void rel_store(const char *vid, const char *name, long vers, Rel *r);
extern int rel_eq(Rel *l, Rel *r);

extern Rel *rel_join(Rel *l, Rel *r);
extern Rel *rel_union(Rel *l, Rel *r);
extern Rel *rel_diff(Rel *l, Rel *r);
extern Rel *rel_project(Rel *r, char *attrs[], int len);
extern Rel *rel_rename(Rel *r, char *from[], char *to[], int len);
extern Rel *rel_select(Rel *r, Expr *bool_expr);
extern Rel *rel_extend(Rel *r, char *names[], Expr *e[], int len);
extern Rel *rel_sum_unary(Rel *r,
                          char *names[],
                          Type types[],
                          Sum *sums[],
                          int len);
extern Rel *rel_sum(Rel *r,
                    Rel *per,
                    char *names[],
                    Type types[],
                    Sum *sums[],
                    int len);
