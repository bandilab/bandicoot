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
#include "value.h"
#include "tuple.h"
#include "volume.h"
#include "expression.h"
#include "summary.h"
#include "relation.h"
#include "environment.h"
#include "transaction.h"

static const int RUNNABLE = 1;
static const int WAITING = 2;
static const int COMMITTED = 3;
static const int REVERTED = 4;

#ifdef LP64
static const long MAX_LONG = 0x7FFFFFFFFFFFFFFF;
#else
static const long MAX_LONG = 0x7FFFFFFF;
#endif

static const int T_ENTER = 1;
static const int R_ENTER = 2;
static const int T_FINISH = 3; /* FIXME: it looks like we need only T_COMMIT */
static const int R_FINISH = 4;
static const int T_SYNC = 5;
static const int R_SYNC = 6;

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

struct List {
    void *elem;
    struct List *next;
};
typedef struct List List;

static char *all_vars[MAX_VARS];
static int num_vars;
static IO* gio;

static List *gents;
static List *gvols;
static Mon *gmon;
static long last_sid;

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
    Vol *e = elem;
    long long id = *((long long*)cmp);
    if (e->id == id) {
        vars_free(e->v);
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
    rm(&gvols, &vol_id, rm_volume);
    gvols = add(gvols, vol);

    return vol;
}

/* FIXME: populate volume id for given variables with optimization */
static void set_vols(Vars *v)
{
    for (int i = 0; i < v->len; ++i) {
        char *var = v->vars[i];
        long ver = v->vers[i];
        for (List *vit = gvols; vit != NULL; vit = vit->next) {
            Vol *vol = vit->elem;
            if (vars_scan(vol->v, var, ver) > -1) {
                v->vols[i] = vol->id;
                break;
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
                    vars_put(vol->v, e->var, e->version);
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

extern void commit(long sid)
{
    finish(sid, COMMITTED);
}

extern void revert(long sid)
{
    finish(sid, REVERTED);
}

extern void tx_free()
{
    mon_lock(gmon);

    for (; gents != NULL; gents = next(gents))
        mem_free(gents->elem);

    for (; gvols != NULL; gvols = next(gvols)) {
        Vol *vol = gvols->elem;
        vars_free(vol->v);
        mem_free(gvols->elem);
    }

    for (int i = 0; i < num_vars; ++i)
        mem_free(all_vars[i]);

    mon_unlock(gmon);
    mon_free(gmon);
}

static void tx_init()
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

static Vars *volume_sync(long long vol_id, Vars *in)
{
    mon_lock(gmon);

    replace_volume(vol_id, in);

    /* populate out variable with the (WRITE/COMMITED) variables */
    Vars *out = vars_new(0);
    for (List *it = gents; it != NULL; it = it->next) {
        Entry *e = it->elem;
        if (e->a_type == WRITE && e->state == COMMITTED)
            vars_put(out, e->var, e->version);
    }
    set_vols(out);

    mon_unlock(gmon);

    char vol[32];
    sys_address_print(vol, vol_id);
    sys_print("tx: volume %s sync done\n", vol);

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
extern long enter(Vars *rvars, Vars *wvars, Mon *m)
{
    long sid;
    int i, state, rw = 0;
    const char *var;
    Entry *re[rvars->len], *we[wvars->len];

    mon_lock(gmon);

    /* FIXME: populate wvol_id based on an algorithm
       executor id is probably required as well for the calculation */
    long long wvol_id = ((Vol*)gvols->elem)->id;

    sid = ++last_sid;
    for (i = 0; i < wvars->len; ++i) {
        var = wvars->vars[i];
        state = WAITING;
        if (get_wsid(var) == -1)
            state = RUNNABLE;

        we[i] = add_entry(sid, var, WRITE, sid, state);
        we[i]->wvol_id = wvol_id;

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

    mon_lock(gmon);
    set_vols(rvars);

    /* FIXME: populate wvols based on previous result */
    for (int i = 0; i < wvars->len; ++i)
        wvars->vols[i] = wvol_id;
    mon_unlock(gmon);

    return sid;
}

extern void tx_attach(int port)
{
    gio = sys_connect(sys_address(port));
}

static void *tx_thread(void *io)
{
    long sid = 0;
    long long vid = 0;

    for (;;) {
        int msg = 0;
        if (sys_readn(io, &msg, sizeof(int)) != sizeof(int))
            goto exit;

        if (msg == T_ENTER) {
            if (sid != 0)
                goto exit;

            Vars *rvars = vars_read(io);
            if (rvars == NULL)
                goto exit;

            Vars *wvars = vars_read(io);
            if (wvars == NULL) {
                vars_free(rvars);
                goto exit;
            }

            sid = enter(rvars, wvars, NULL);

            msg = R_ENTER;
            if (sys_write(io, &msg, sizeof(int)) < 0 ||
                sys_write(io, &sid, sizeof(long)) < 0 ||
                vars_write(rvars, io) < 0 ||
                vars_write(wvars, io) < 0)
            {
                tx_revert(sid);
                goto exit;
            }

            vars_free(rvars);
            vars_free(wvars);
        } else if (msg == T_FINISH) {
            int final_state = 0;
            long read_sid = 0;
            if (sys_readn(io, &read_sid, sizeof(long)) != sizeof(long) ||
                read_sid != sid ||
                sys_readn(io, &final_state, sizeof(int)) != sizeof(int) ||
                (final_state != COMMITTED && final_state != REVERTED))
                goto exit;

            /* TODO: what if sid does not exist? */
            finish(sid, final_state);
            sid = 0; /* at this point we cannot revert anymore */

            /* FIXME: we can commit and then fail to notify the client */
            msg = R_FINISH;
            if (sys_write(io, &msg, sizeof(int)) < 0 ||
                sys_write(io, &final_state, sizeof(int)) < 0)
                goto exit;
        } else if (msg == T_SYNC) {
            /* FIXME: if the sync fails we need to remove the volume from
                      the list */
            if (sys_readn(io, &vid, sizeof(long long)) != sizeof(long long))
                goto exit;

            Vars *in = vars_read(io);
            if (in == NULL)
                goto exit;

            Vars *out = volume_sync(vid, in);
            msg = R_SYNC;

            /* we can ignore io errors while responding to volume sync */
            sys_write(io, &msg, sizeof(int));
            vars_write(out, io);

            vars_free(out);
        }
    }

exit:
    if (sid != 0) {
        sys_print("tx: transaction %016X failed\n", sid);
        finish(sid, REVERTED);
    }

    if (vid != 0) {
        mon_lock(gmon);
        rm(&gvols, &vid, rm_volume);
        mon_unlock(gmon);

        char vol[32];
        sys_address_print(vol, vid);
        sys_print("tx: volume %s disconnected\n", vol);
    }

    sys_close(io);
    return NULL;
}

static void *server_thread(void *sio)
{
    for (;;) {
        IO *cio = sys_accept(sio);
        sys_thread(tx_thread, cio);
    }

    sys_close(sio);
    return NULL;
}

extern void tx_server(int *port)
{
    tx_init();

    IO *sio = sys_socket(port);
    sys_thread(server_thread, sio);
}

extern long tx_enter(Vars *rvars, Vars *wvars)
{
    int msg = T_ENTER;
    if (sys_write(gio, &msg, sizeof(int)) < 0 ||
        vars_write(rvars, gio) < 0 ||
        vars_write(wvars, gio) < 0)
        sys_die("T_ENTER failed\n");

    long sid = 0;
    Vars *r = NULL, *w = NULL;
    if (sys_readn(gio, &msg, sizeof(int)) != sizeof(int) ||
        msg != R_ENTER ||
        sys_readn(gio, &sid, sizeof(long)) != sizeof(long) ||
        (r = vars_read(gio)) == NULL ||
        (w = vars_read(gio)) == NULL)
        sys_die("R_ENTER failed\n");

    vars_cpy(rvars, r);
    vars_cpy(wvars, w);

    vars_free(r);
    vars_free(w);

    return sid;
}

static void net_finish(long sid, int final_state)
{
    int msg = T_FINISH;
    if (sys_write(gio, &msg, sizeof(int)) < 0 ||
        sys_write(gio, &sid, sizeof(long)) < 0 ||
        sys_write(gio, &final_state, sizeof(int)) < 0)
        sys_die("T_FINISH %s failed, sid=%016X\n",
                final_state == COMMITTED ? "commit" : "revert",
                sid);

    if (sys_readn(gio, &msg, sizeof(int)) != sizeof(int) ||
        msg != R_FINISH ||
        sys_readn(gio, &msg, sizeof(int)) != sizeof(int) ||
        msg != final_state)
        sys_die("R_FINISH %s failed, sid=%016X\n",
                final_state == COMMITTED ? "commit" : "revert",
                sid);
}

extern void tx_commit(long sid)
{
    net_finish(sid, COMMITTED);
}

extern void tx_revert(long sid)
{
    net_finish(sid, REVERTED);
}

extern Vars *tx_volume_sync(long long vol_id, Vars *in)
{
    int msg = T_SYNC;
    if (sys_write(gio, &msg, sizeof(int)) < 0 ||
        sys_write(gio, &vol_id, sizeof(long long)) < 0)
        sys_die("T_SYNC failed\n");

    Vars *out = NULL;
    if (vars_write(in, gio) < 0 ||
        sys_readn(gio, &msg, sizeof(int)) != sizeof(int) ||
        msg != R_SYNC ||
        (out = vars_read(gio)) == NULL)
        sys_die("R_SYNC failed\n");

    return out;
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

        char *state = NULL;
        if (e->state == COMMITTED) state = "COMMITTED";
        else if (e->state == REVERTED) state = "REVERTED";
        else if (e->state == RUNNABLE) state = "RUNNABLE";
        else if (e->state == WAITING) state = "WAITING";
        else sys_die("tx: unknown state %d\n", e->state);

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
