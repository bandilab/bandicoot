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

struct Rel {
    Head *head;
    TBuf *body;
    void *ctxt;

    void (*eval)(struct Rel *r, Vars *v, Arg *a);
    void (*free)(struct Rel *r);
};

typedef struct Rel Rel;

/* evaluate a relation with the corresponding arguments */
extern void rel_eval(Rel *r, Vars *v, Arg *a);

/* free a relation */
extern void rel_free(Rel *r);

/* check if two relations are identical (relations must be evaluated first) */
extern int rel_eq(Rel *l, Rel *r);

/* load a relation stored in a variable identified by name. this applies to
   global and temporary variables as well as to the relational parameters
   passed to functions */
extern Rel *rel_load(Head *head, const char *name);

/* store a relation in a variable identified by name */
extern Rel *rel_store(const char *name, Rel *r);

/* a function call */
extern Rel *rel_call(char **r, int rlen,
                     char **w, int wlen,
                     char **t, int tlen,
                     Rel **stmts, int slen,
                     Expr **pexprs, int plen,
                     Rel *rexpr, char *rname,
                     Head *ret);

/* natural join of two relations */
extern Rel *rel_join(Rel *l, Rel *r);

/* union of two relations */
extern Rel *rel_union(Rel *l, Rel *r);

/* difference of two relations (l \ r) */
extern Rel *rel_diff(Rel *l, Rel *r);

/* projection of a relation to the corresponding attribtues */
extern Rel *rel_project(Rel *r, char *attrs[], int len);

/* rename the attributes within a relation */
extern Rel *rel_rename(Rel *r, char *from[], char *to[], int len);

/* select tuples from a relation satisfying a boolean expression */
extern Rel *rel_select(Rel *r, Expr *bool_expr);

/* extend a relation with more attributes specified as primitive expressions */
extern Rel *rel_extend(Rel *r, char *names[], Expr *e[], int len);

/* summarize a relation with the help of aggregate functions */
extern Rel *rel_sum_unary(Rel *r,
                          char *names[],
                          Type types[],
                          Sum *sums[],
                          int len);

/* group the relation "r" per relation "per" and summarize each group */
extern Rel *rel_sum(Rel *r,
                    Rel *per,
                    char *names[],
                    Type types[],
                    Sum *sums[],
                    int len);
