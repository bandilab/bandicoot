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

#include "config.h"
#include "array.h"
#include "memory.h"
#include "string.h"
#include "system.h"
#include "head.h"
#include "transaction.h"
#include "volume.h"
#include "mutex.h"

#define RUNNABLE 1
#define WAITING 2
#define COMMITTED 3
#define REVERTED 4

typedef struct {
    long sid;
    char var[MAX_NAME];
    int a_type;
    long version;
    int state;
    long lock;
} Entry;

typedef struct {
    int size;
    Entry **elem;
} E_Array;

#define EMPTY_ARRAY {.size = 0, .elem = 0}

#ifdef LP64
static const long MAX_LONG = 0x7FFFFFFFFFFFFFFF;
#else
static const long MAX_LONG = 0x7FFFFFFF;
#endif

static char *all_vars[MAX_VARS];
static int num_vars;

static E_Array gents;
static long glock;
static long last_sid;

/* FIXME: don't forget to free it */
static void append(E_Array *dest, Entry *e)
{
    dest->elem = (Entry**) mem_realloc(
            dest->elem, sizeof(Entry*) * (dest->size + 1));
    dest->elem[dest->size++] = e;
}

static Entry *add_entry(long sid,
                        const char var[],
                        int a_type,
                        long version,
                        int state)
{

    Entry *e = (Entry*) mem_alloc(sizeof(Entry));
    e->sid = sid;
    str_cpy(e->var, var);
    e->a_type = a_type;
    e->version = version;
    e->state = state;
    e->lock = mutex_new();
    mutex_lock(e->lock);

    append(&gents, e);

    return e;
}

static void list_entries(E_Array *dest, long sid)
{
    for (int i = 0; i < gents.size; ++i) {
        Entry *e = gents.elem[i];
        if (e->sid == sid)
            append(dest, e);
    }
}

static Entry *get_min_waiting(const char var[], int a_type)
{
    long min_sid = MAX_LONG;
    Entry *res = 0;

    for (int i = 0; i < gents.size; ++i) {
        Entry *e = gents.elem[i];
        if (e->state == WAITING && e->sid < min_sid
                && e->a_type == a_type && str_cmp(e->var, var) == 0)
        {
            res = e;
            min_sid = e->sid;
        }
    }

    return res;
}

static void list_waiting(E_Array *dest, long sid, const char var[], int a_type)
{
    for (int i = 0; i < gents.size; ++i) {
        Entry *e = gents.elem[i];
        if (e->state == WAITING && e->sid <= sid
                && e->a_type == a_type && str_cmp(e->var, var) == 0)
            append(dest, e);
    }
}

static long get_wsid(const char var[])
{
    long res = -1;

    for (int i = 0; i < gents.size; ++i) {
        Entry *e = gents.elem[i];
        if (e->a_type == WRITE && e->state == RUNNABLE
                && str_cmp(e->var, var) == 0)
        {
            res = e->sid;
            break;
        }
    }

    return res;
}

static long get_rsid(long sid, const char var[])
{
    long res = -1;

    for (int i = 0; i < gents.size; ++i) {
        Entry *e = gents.elem[i];
        if (e->a_type == WRITE && e->state == COMMITTED
                && e->sid < sid && e->sid > res && str_cmp(e->var, var) == 0)
            res = e->sid;
    }

    return res;
}

static void current_state(long vers[])
{
    for (int i = 0; i < num_vars; ++i)
        vers[i] = 0;

    for (int i = 0; i < gents.size; ++i) {
        Entry *e = gents.elem[i];
        if (e->a_type == WRITE && e->state == COMMITTED) {
            int idx = array_find(all_vars, num_vars, e->var);
            if (vers[idx] < e->version)
                vers[idx] = e->version;
        }
    }
}

static void finish(int sid, int final_state)
{
    E_Array sid_ents = EMPTY_ARRAY;

    mutex_lock(glock);

    list_entries(&sid_ents, sid);

    for (int i = 0; i < sid_ents.size; ++i) {
        Entry *e = sid_ents.elem[i];
        int prev_state = e->state;
        e->state = final_state;

        if (e->a_type == WRITE && prev_state == RUNNABLE) {
            long rsid = e->version;
            if (final_state == REVERTED)
                rsid = get_rsid(e->sid, e->var);

            Entry *we = get_min_waiting(e->var, WRITE);

            long wsid = MAX_LONG;
            if (we != 0) {
                we->state = RUNNABLE;
                mutex_unlock(we->lock);
                wsid = we->sid;
            }

            E_Array rents = EMPTY_ARRAY;
            list_waiting(&rents, wsid, e->var, READ);
            int j; 
            for (j = 0; j < rents.size; ++j) {
                Entry *re = rents.elem[j];
                re->state = RUNNABLE;
                re->version = rsid;
                mutex_unlock(re->lock);
            }
            mem_free(rents.elem);
        }

        mutex_close(e->lock);
    }
    mem_free(sid_ents.elem);

    /* FIXME: what if this fails and we have already notified others
              to proceed */
    long vers[num_vars];
    current_state(vers);
    vol_wstate(all_vars, vers, num_vars);

    mutex_unlock(glock);
}

extern void tx_free()
{
    mutex_lock(glock);

    for (int i = 0; i < gents.size; ++i)
        mem_free(gents.elem[i]);
    for (int i = 0; i < num_vars; ++i)
        mem_free(all_vars[i]);

    mem_free(gents.elem);

    mutex_unlock(glock);
    mutex_close(glock);
}

extern void tx_init(char *vars[], int len)
{
    glock = mutex_new();
    last_sid = 1;
    gents.size = 0;
    gents.elem = 0;
    num_vars = len;

    mutex_lock(glock);
    for (int i = 0; i < num_vars; ++i) {
        all_vars[i] = str_dup(vars[i]);

        Entry *e = add_entry(1, all_vars[i], WRITE, 1, COMMITTED);
        mutex_unlock(e->lock);
        mutex_close(e->lock);
    }
    array_sort(all_vars, num_vars, 0);
    mutex_unlock(glock);
}

static long wait(Entry *e)
{
    mutex_lock(e->lock);
    long version = e->version;
    mutex_unlock(e->lock);

    return version;
}

extern long tx_enter(char *rnames[], long rvers[], int rlen,
                     char *wnames[], long wvers[], int wlen)
{
    long sid;
    int i, state, rw = 0;
    const char *var;
    Entry *re[rlen], *we[wlen];

    mutex_lock(glock);

    sid = ++last_sid;
    for (i = 0; i < wlen; ++i) {
        var = wnames[i];
        state = WAITING;
        if (get_wsid(var) == -1)
            state = RUNNABLE;

        we[i] = add_entry(sid, var, WRITE, sid, state);

        if (state == RUNNABLE)
            mutex_unlock(we[i]->lock);

        rw = 1;
    }
    for (i = 0; i < rlen; ++i) {
        var = rnames[i];
        long rsid = get_rsid(sid, var);
        state = RUNNABLE;

        if (rw) {
            long wsid = get_wsid(var);
            if (wsid > -1L && sid > wsid) {
                rsid = -1L;
                state = WAITING;
            }
        }

        re[i] = add_entry(sid, var, READ, rsid, state);

        if (state == RUNNABLE)
            mutex_unlock(re[i]->lock);
    }

    mutex_unlock(glock);

    for (i = 0; i < rlen; ++i)
        rvers[i] = wait(re[i]);

    for (i = 0; i < wlen; ++i)
        wvers[i] = wait(we[i]);

    return sid;
}

extern void tx_commit(long sid)
{
    finish(sid, COMMITTED);
}

extern void tx_revert(long sid)
{
    finish(sid, REVERTED);
}

extern void tx_state()
{
    mutex_lock(glock);

    sys_print("%-8s %-32s %-5s %-8s %-9s\n",
              "SID", "VARIABLE", "ATYPE", "ASID", "STATE");
    for (int i = 0; i < gents.size; ++i) {
        Entry *e = gents.elem[i];
        char *a_type = "READ";
        if (e->a_type == WRITE)
            a_type = "WRITE";

        char *state = 0;
        switch (e->state) {
            case COMMITTED: state = "COMMITTED"; break;
            case REVERTED: state = "REVERTED"; break;
            case RUNNABLE: state = "RUNNABLE"; break;
            case WAITING: state = "WAITING"; break;
            default:
                sys_die("tx: unknown state %d\n", e->state);
        }

        sys_print("%-8d %-32s %-5s %-8d %-9s\n",
                  e->sid, e->var, a_type, e->version, state);

    }

    mutex_unlock(glock);
}
