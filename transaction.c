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

struct E_List {
    Entry *elem;
    struct E_List *next;
};

typedef struct E_List E_List;

#ifdef LP64
static const long MAX_LONG = 0x7FFFFFFFFFFFFFFF;
#else
static const long MAX_LONG = 0x7FFFFFFF;
#endif

static char *all_vars[MAX_VARS];
static int num_vars;

static E_List *gents;
static long glock;
static long last_sid;

static E_List *next(E_List *e)
{
    E_List *res = e->next;
    mem_free(e);
    return res;
}

static E_List *add(E_List *dest, Entry *e)
{
    E_List *new = mem_alloc(sizeof(E_List));
    new->elem = e;
    new->next = dest;

    return new;
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

    gents = add(gents, e);

    return e;
}

static E_List *list_entries(long sid)
{
    E_List *dest = NULL;

    for (E_List *it = gents; it != NULL; it = it->next) {
        Entry *e = it->elem;
        if (e->sid == sid)
            dest = add(dest, e);
    }

    return dest;
}

static Entry *get_min_waiting(const char var[], int a_type)
{
    long min_sid = MAX_LONG;
    Entry *res = 0;

    for (E_List *it = gents; it != NULL; it = it->next) {
        Entry *e = it->elem;
        if (e->state == WAITING && e->sid < min_sid
                && e->a_type == a_type && str_cmp(e->var, var) == 0)
        {
            res = e;
            min_sid = e->sid;
        }
    }

    return res;
}

/* finds if the given version (sid) of a variable is active (being read) */
static int is_active(const char var[], long sid)
{
    int res = 0;

    for (E_List *it = gents; !res && it != NULL; it = it->next) {
        Entry *e = it->elem;
        res = (e->state == RUNNABLE && e->a_type == READ
                && e->version == sid && str_cmp(e->var, var) == 0);
    }

    return res;
}

static E_List *list_waiting(long sid, const char var[], int a_type)
{
    E_List *dest = NULL;

    for (E_List *it = gents; it != NULL; it = it->next) {
        Entry *e = it->elem;
        if (e->state == WAITING && e->sid <= sid
                && e->a_type == a_type && str_cmp(e->var, var) == 0)
            dest = add(dest, e);
    }

    return dest;
}

static long get_wsid(const char var[])
{
    long res = -1;

    for (E_List *it = gents; it != NULL; it = it->next) {
        Entry *e = it->elem;
        if (e->a_type == WRITE && e->state == RUNNABLE
                && str_cmp(e->var, var) == 0)
        {
            res = e->sid;
            break;
        }
    }

    return res;
}

/* determines a version to read */
static long get_rsid(long sid, const char var[])
{
    long res = -1;

    for (E_List *it = gents; it != NULL; it = it->next) {
        Entry *e = it->elem;
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

    for (E_List *it = gents; it != NULL; it = it->next) {
        Entry *e = it->elem;
        if (e->a_type == WRITE && e->state == COMMITTED) {
            int idx = array_find(all_vars, num_vars, e->var);
            if (vers[idx] < e->version)
                vers[idx] = e->version;
        }
    }
}

/* removes entries from gents which are
    * non-active, non-latest committed writes
    * committed reads
    * reverted reads and writes
*/
static void clean_up()
{
    E_List *it = gents, **prev = &gents;
    int rm = 0;

    while (it) {
        Entry *e = it->elem;
        if (e->a_type == WRITE && e->state == COMMITTED) {
            long sid = e->sid;
            char *var = e->var;

            long rsid = get_rsid(MAX_LONG, var);

            int act = is_active(var, sid);

            if (sid < rsid && !act)
                rm = 1;
        } else if ((e->a_type == READ && e->state == COMMITTED)
                        || e->state == REVERTED)
        {
            rm = 1;
        }

        if (rm) {
            *prev = it->next;

            if (e->a_type == WRITE && e->state == COMMITTED)
                vol_remove(e->var, e->sid);

            mem_free(it->elem);
            mem_free(it);

            rm = 0;
            it = gents;
            prev = &gents;
        } else {
            prev = &it->next;
            it = it->next;
        }
    }
}

static void finish(int sid, int final_state)
{
    mutex_lock(glock);

    E_List *sig = NULL; /* entries to signal (unlock) */
    E_List *it = list_entries(sid);

    for (; it != NULL; it = next(it)) {
        Entry *e = it->elem;
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
                wsid = we->sid;
                sig = add(sig, we);
            }

            E_List *rents = list_waiting(wsid, e->var, READ);
            int j; 
            for (; rents != NULL; rents = next(rents)) {
                Entry *re = rents->elem;
                re->state = RUNNABLE;
                re->version = rsid;
                sig = add(sig, re);
            }
        }

        mutex_close(e->lock);
    }

    long vers[num_vars];
    current_state(vers);
    vol_wstate(all_vars, vers, num_vars);

    clean_up();

    for (; sig != NULL; sig = next(sig))
        mutex_unlock(sig->elem->lock);

    mutex_unlock(glock);
}

extern void tx_free()
{
    mutex_lock(glock);

    for (; gents != NULL; gents = next(gents))
        mem_free(gents->elem);

    for (int i = 0; i < num_vars; ++i)
        mem_free(all_vars[i]);


    mutex_unlock(glock);
    mutex_close(glock);
}

extern void tx_init(char *vars[], int len)
{
    glock = mutex_new();
    last_sid = 1;
    gents = NULL;
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

/* the Sem *s input parameter introduced for testing (test/transaction.c) */
extern long tx_enter_full(char *rnames[], long rvers[], int rlen,
                          char *wnames[], long wvers[], int wlen,
                          Sem *s)
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

    if (s != NULL)
        sem_inc(s);

    for (i = 0; i < rlen; ++i)
        rvers[i] = wait(re[i]);

    for (i = 0; i < wlen; ++i)
        wvers[i] = wait(we[i]);

    return sid;
}

extern long tx_enter(char *rnames[], long rvers[], int rlen,
                     char *wnames[], long wvers[], int wlen)
{
    return tx_enter_full(rnames, rvers, rlen,
                         wnames, wvers, wlen,
                         NULL);
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
    for (E_List *it = gents; it != NULL; it = it->next) {
        Entry *e = it->elem;
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
