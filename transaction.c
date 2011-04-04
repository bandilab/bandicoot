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
#include "fs.h"
#include "head.h"
#include "volume.h"
#include "value.h"
#include "tuple.h"
#include "expression.h"
#include "summary.h"
#include "relation.h"
#include "environment.h"
#include "transaction.h"

#define RUNNABLE 1
#define WAITING 2
#define COMMITTED 3
#define REVERTED 4

/* Vol keeps information about the content of a volume */
typedef struct {
    long long id;
    Vars *v;
} Vol;

typedef struct {
    long sid; /* transaction identifier */
    char var[MAX_NAME]; /* variable name */
    int a_type; /* READ or WRITE actions */
    long version; /* either an SID to read or an SID to create/write */
    int state; /* identifies the current entry state, see defines above */
    long long wvol_id; /* volume for write action */
    Mon *mon; /* monitor on which the caller/transaction is blocked if needed */
} Entry;

/* generic List structure */
struct List {
    void *elem;
    struct List *next;
};

typedef struct List List;

static List *next(List *e)
{
    List *res = e->next;
    mem_free(e);
    return res;
}

static List *add(List *dest, void *elem)
{
    List *new = mem_alloc(sizeof(List));
    new->elem = elem;
    new->next = dest;

    return new;
}

/* rm_elem method is used to:
   - identify elems to be removed
   - must mem_free the whole elem

   elem is current elem from the list to be removed
   cmp which can be used to compare the elem to */
static void rm(List **list,
               void *cmp,
               int (*rm_elem)(void *elem, void *cmp))
{
    List *it = *list, **prev = list;
    while (it) {
        int rm = rm_elem(it->elem, cmp);
        if (rm) {
            *prev = it->next;

            mem_free(it);

            it = *list;
            prev = list;
        } else {
            prev = &it->next;
            it = it->next;
        }
    }
}

#ifdef LP64
static const long MAX_LONG = 0x7FFFFFFFFFFFFFFF;
#else
static const long MAX_LONG = 0x7FFFFFFF;
#endif

static char *all_vars[MAX_VARS];
static int num_vars;

static List *gents;
static List *gvols;
static Mon *gmon;
static long last_sid;

/* determines a version to read */
static long get_rsid(long sid, const char var[])
{
    long res = -1;

    for (List *it = gents; it != NULL; it = it->next) {
        Entry *e = it->elem;
        if (e->a_type == WRITE && e->state == COMMITTED
                && e->sid < sid && e->sid > res && str_cmp(e->var, var) == 0)
            res = e->sid;
    }

    return res;
}

/* returns true if a given sid of a variable is active (being read) */
static int is_active(const char var[], long sid)
{
    int res = 0;

    for (List *it = gents; !res && it != NULL; it = it->next) {
        Entry *e = it->elem;
        res = (e->state == RUNNABLE && e->a_type == READ
                && e->version == sid && str_cmp(e->var, var) == 0);
    }

    return res;
}

/* returns true if the entry is
    * non-active, non-latest committed write
    * committed read
    * reverted read or write
*/
static int rm_entry(void *elem, void *cmp)
{
    Entry *e = elem;
    int rm = 0;
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

    if (rm)
        mem_free(e);

    return rm;
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
    e->wvol_id = 0L;
    e->mon = mon_new();

    gents = add(gents, e);

    return e;
}

static List *list_entries(long sid)
{
    List *dest = NULL;

    for (List *it = gents; it != NULL; it = it->next) {
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

    for (List *it = gents; it != NULL; it = it->next) {
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

static List *list_waiting(long sid, const char var[], int a_type)
{
    List *dest = NULL;

    for (List *it = gents; it != NULL; it = it->next) {
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

    for (List *it = gents; it != NULL; it = it->next) {
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

static int rm_volume(void *elem, void *cmp)
{
    Vol *e = elem, *c = cmp;
    if (e->id == c->id) {
        mem_free(e->v);
        mem_free(e);

        return 1;
    }

    return 0;
}

static Vol *replace_volume(long long vol_id, Vars *v)
{
    Vol *vol = (Vol*) mem_alloc(sizeof(Vol));
    vol->id = vol_id;
    vol->v = v;

    /* FIXME: we can possibly overwrite variables written via tx_commit 
       an obsolete tx_volume_sync message from a volume can arrive after a
       commit due to some network delays. It would not have the latest
       commited variables in it.
     */
    rm(&gvols, vol, rm_volume);
    gvols = add(gvols, vol);

    return vol;
}

/* populate volume ids for given variables */
static void set_vols(Vars *v)
{
    for (int i = 0; i < v->len; ++i) {
        char *var = v->vars[i];
        long ver = v->vers[i];
        for (List *vit = gvols; vit != NULL; vit = vit->next) {
            Vol *vol = vit->elem;
            if (vars_scan(vol->v, var, ver) > -1) {
                int j;
                for (j = 0; j < MAX_VOLUMES; ++j)
                    if (v->vols[i][j] == 0L) {
                        v->vols[i][j] = vol->id;
                        break;
                    }

                if (j == MAX_VOLUMES)   /* replace first volume id */
                    v->vols[i][0] = vol->id;
            }
        }
    }
}

static Vol *get_volume(long long vol_id)
{
    Vol *vol = NULL;
    for (List *it = gvols; it != NULL && vol == NULL; it = it->next) {
        Vol *v = it->elem;
        if (v->id == vol_id)
            vol = v;
    }

    return vol;
}

static void current_state(long vers[])
{
    for (int i = 0; i < num_vars; ++i)
        vers[i] = 0;

    for (List *it = gents; it != NULL; it = it->next) {
        Entry *e = it->elem;
        if (e->a_type == WRITE && e->state == COMMITTED) {
            int idx = array_scan(all_vars, num_vars, e->var);
            if (vers[idx] < e->version)
                vers[idx] = e->version;
        }
    }
}

extern void wstate()
{
    long vers[num_vars];
    current_state(vers);

    int SID_LEN = fs_sid_len;
    char sid[SID_LEN];
    char *buf = mem_alloc(num_vars * (MAX_NAME + SID_LEN));

    sys_move(fs_state_bak, fs_state);

    int off = 0;
    for (int i = 0; i < num_vars; ++i) {
        fs_sid_to_str(sid, vers[i]);
        off += str_print(buf + off, "%s,%s\n", all_vars[i], sid);
    }

    IO *io = sys_open(fs_state, CREATE | WRITE);
    sys_write(io, buf, off);
    sys_close(io);

    mem_free(buf);

    sys_remove(fs_state_bak);
}

static void finish(int sid, int final_state)
{
    mon_lock(gmon);

    List *sig = NULL; /* entries to signal (unlock) */
    List *it = list_entries(sid);
    for (; it != NULL; it = next(it)) {
        Entry *e = it->elem;
        int prev_state = e->state;
        e->state = final_state;

        if (e->a_type == WRITE && prev_state == RUNNABLE) {

            long rsid = e->version;
            if (final_state == REVERTED)
                rsid = get_rsid(e->sid, e->var);
            else {
                long long vol_id = e->wvol_id;
                Vol *vol = get_volume(vol_id);
                if (vol != NULL)
                    vol->v = vars_put(vol->v, e->var, e->version);
            }

            Entry *we = get_min_waiting(e->var, WRITE);

            long wsid = MAX_LONG;
            if (we != NULL) {
                wsid = we->sid;
                sig = add(sig, we);
            }

            List *rents = list_waiting(wsid, e->var, READ);
            for (; rents != NULL; rents = next(rents)) {
                Entry *re = rents->elem;
                re->version = rsid;
                sig = add(sig, re);
            }
        }

        mon_free(e->mon);
    }

    wstate();
    rm(&gents, NULL, rm_entry);

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

    for (; gvols != NULL; gvols = next(gvols)) {
        Vol *vol = gvols->elem;
        mem_free(vol->v);
        mem_free(gvols->elem);
    }

    for (int i = 0; i < num_vars; ++i)
        mem_free(all_vars[i]);

    mon_unlock(gmon);
    mon_free(gmon);
}

extern void tx_init()
{
    gmon = mon_new();
    gents = NULL;
    gvols = NULL;

    last_sid = 1;
    num_vars = 0;

    if (sys_exists(fs_state_bak))
        sys_move(fs_state, fs_state_bak);

    long vers[MAX_VARS];
    char *vars[MAX_VARS];
    int len;

    char *buf = sys_load(fs_state);
    char **lines = str_split(buf, '\n', &len);
    if (str_len(lines[len - 1]) == 0)
        --len;

    for (int i = 0; i < len; ++i) {
        int cnt;
        char **name_sid = str_split(lines[i], ',', &cnt);
        if (cnt != 2)
            sys_die("bad line %s:%d\n", fs_state, i + 1);

        vars[i] = str_dup(name_sid[0]);
        vers[i] = fs_str_to_sid(name_sid[1]);

        mem_free(name_sid);
    }

    mem_free(lines);
    mem_free(buf);

    mon_lock(gmon);
    num_vars = len;

    for (int i = 0; i < num_vars; ++i) {
        all_vars[i] = vars[i];
        long sid = vers[i];

        if (sid > last_sid)
            last_sid = sid;

        Entry *e = add_entry(sid, all_vars[i], WRITE, sid, COMMITTED);
        mon_free(e->mon);
    }
    mon_unlock(gmon);
}

extern void tx_deploy(const char *new_src)
{
    char *old_src = fs_source;
    if (sys_empty(fs_path)) {
        IO *io = sys_open(old_src, CREATE | WRITE);
        sys_close(io);

        io = sys_open(fs_state, CREATE | WRITE);
        sys_close(io);
    } else
        tx_init();

    Env *new_env = env_new(new_src);
    Env *old_env = env_new(old_src);

    if (!env_compat(old_env, new_env))
        sys_die("cannot deploy incompatible source file '%s'\n", new_src);

    int new_len = new_env->vars.len;
    char **new_vars = new_env->vars.names;

    /* create new variables  */
    for (int i = 0; i < new_len; ++i) {
        char *var = str_dup(new_vars[i]);
        int idx = array_scan(all_vars, num_vars, new_vars[i]);
        if (idx < 0) {
            all_vars[num_vars++] = var;
            Entry *e = add_entry(1, var, WRITE, 1, COMMITTED);
            mon_free(e->mon);
        }
    }

    wstate();
    sys_cpy(old_src, new_src);

    env_free(new_env);
    env_free(old_env);
}

extern Vars *tx_volume_sync(long long vol_id, Vars *in)
{
    mon_lock(gmon);

    if (in != NULL)
        replace_volume(vol_id, in);

    /* populate out variable with the (WRITE/COMMITED) variables */
    Vars *out = vars_new(0);
    for (List *it = gents; it != NULL; it = it->next) {
        Entry *e = it->elem;
        if (e->a_type == WRITE && e->state == COMMITTED)
            out = vars_put(out, e->var, e->version);
    }
    set_vols(out);

    mon_unlock(gmon);

    return out;
}

static void wait(Vars *s, Entry *entries[])
{
    for (int i = 0; i < s->len; ++i) {
        Entry *e = entries[i];
        mon_lock(e->mon);
        while (e->state == WAITING)
            mon_wait(e->mon);

        s->vers[i] = e->version;
        mon_unlock(e->mon);
    }
}

/* the m input parameter is for testing purposes (test/transaction.c) */
extern long tx_enter_full(Vars *rvars, Vars *wvars, Mon *m)
{
    long sid;
    int i, state, rw = 0;
    const char *var;
    Entry *re[rvars->len], *we[wvars->len];

    mon_lock(gmon);

    sid = ++last_sid;
    for (i = 0; i < wvars->len; ++i) {
        var = wvars->vars[i];
        state = WAITING;
        if (get_wsid(var) == -1)
            state = RUNNABLE;

        we[i] = add_entry(sid, var, WRITE, sid, state);

        /* FIXME: populate wvol_id based on an algorithm
           executor id is probably required as well for the calculation */
        we[i]->wvol_id = VOLUME_ID;

        rw = 1;
    }
    for (i = 0; i < rvars->len; ++i) {
        var = rvars->vars[i];
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

    wait(rvars, re);
    wait(wvars, we);

    set_vols(rvars);

    /* FIXME: populate wvols based on previous result */
    for (int i = 0; i < wvars->len; ++i)
        wvars->vols[i][0] = VOLUME_ID;

    return sid;
}

extern long tx_enter(Vars *rvars, Vars *wvars)
{
    return tx_enter_full(rvars, wvars, NULL);
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
    for (List *it = gents; it != NULL; it = it->next) {
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

    sys_print("%-8s %-32s %-8s\n",
              "VOLUME", "VARIABLE", "SID");
    for (List *it = gvols; it != NULL; it = it->next) {
        Vol *vol = it->elem;
        for (int i = 0; i < vol->v->len; ++i)
            sys_print("%-8d %-32s %-8d\n",
                      vol->id, vol->v->vars[i], vol->v->vers[i]);
    }

    mon_unlock(gmon);
}
