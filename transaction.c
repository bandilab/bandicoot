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
    Mon *mon;
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
static Mon *gmon;
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
    e->mon = mon_new();

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
    mon_lock(gmon);

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
            if (we != NULL) {
                wsid = we->sid;
                sig = add(sig, we);
            }

            E_List *rents = list_waiting(wsid, e->var, READ);
            for (; rents != NULL; rents = next(rents)) {
                Entry *re = rents->elem;
                re->version = rsid;
                sig = add(sig, re);
            }
        }

        mon_free(e->mon);
    }

    long vers[num_vars];
    current_state(vers);
    vol_wstate(all_vars, vers, num_vars);

    clean_up();

    for (; sig != NULL; sig = next(sig)) {
        Entry *e = sig->elem;
        mon_lock(e->mon);
        e->state = RUNNABLE;
        mon_signal(e->mon);
        mon_unlock(e->mon);
    }

    mon_unlock(gmon);
}

extern void tx_free()
{
    mon_lock(gmon);

    for (; gents != NULL; gents = next(gents))
        mem_free(gents->elem);

    for (int i = 0; i < num_vars; ++i)
        mem_free(all_vars[i]);

    mon_unlock(gmon);
    mon_free(gmon);
}

extern void tx_init(char *vars[], int len)
{
    gmon = mon_new();
    last_sid = 1;
    gents = NULL;
    num_vars = len;

    mon_lock(gmon);
    for (int i = 0; i < num_vars; ++i) {
        all_vars[i] = str_dup(vars[i]);

        Entry *e = add_entry(1, all_vars[i], WRITE, 1, COMMITTED);
        mon_free(e->mon);
    }
    array_sort(all_vars, num_vars, 0);
    mon_unlock(gmon);
}

static long wait(Entry *e)
{
    mon_lock(e->mon);
    while (e->state == WAITING)
        mon_wait(e->mon);

    long version = e->version;
    mon_unlock(e->mon);

    return version;
}

/* the m input parameter is for testing purposes (test/transaction.c) */
extern long tx_enter_full(char *rnames[], long rvers[], int rlen,
                          char *wnames[], long wvers[], int wlen,
                          Mon *m)
{
    long sid;
    int i, state, rw = 0;
    const char *var;
    Entry *re[rlen], *we[wlen];

    mon_lock(gmon);

    sid = ++last_sid;
    for (i = 0; i < wlen; ++i) {
        var = wnames[i];
        state = WAITING;
        if (get_wsid(var) == -1)
            state = RUNNABLE;

        we[i] = add_entry(sid, var, WRITE, sid, state);
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
    }

    mon_unlock(gmon);

    if (m != NULL) {
        mon_lock(m);
        m->value++;
        mon_signal(m);
        mon_unlock(m);
    }

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
    mon_lock(gmon);

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

    mon_unlock(gmon);
}
