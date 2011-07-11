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

#include "config.h"
#include "array.h"
#include "memory.h"
#include "string.h"
#include "system.h"
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
static const int T_FINISH = 3;
static const int R_FINISH = 4;
static const int T_SYNC = 5;
static const int R_SYNC = 6;
static const int T_SOURCE = 7;
static const int R_SOURCE = 8;

/* Vol keeps information about the content of a volume */
typedef struct {
    char id[MAX_ADDR];
    Vars *vars;
} Vol;

typedef struct {
    long sid; /* transaction identifier */
    char name[MAX_NAME]; /* variable name */
    int a_type; /* READ or WRITE actions */
    long version; /* either an SID to read or an SID to create/write */
    int state; /* identifies the current entry state, see defines above */
    char wvid[MAX_ADDR]; /* volume for write action */
    Mon *mon; /* monitor on which the caller/transaction is blocked if needed */
} Entry;

struct List {
    void *elem;
    struct List *next;
};
typedef struct List List;

static struct {
    char *buf;
    int len;
} gcode;

static struct {
    char *names[MAX_VARS];
    int len;
} gvars;

static char gstate[MAX_FILE_PATH];
static char gstate_bak[MAX_FILE_PATH];
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
               const void *cmp,
               int (*rm_elem)(void *elem, const void *cmp))
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
static long get_rsid(long sid, const char name[])
{
    long res = -1;

    for (List *it = gents; it != NULL; it = it->next) {
        Entry *e = it->elem;
        if (e->a_type == WRITE && e->state == COMMITTED
                && e->sid < sid && e->sid > res && str_cmp(e->name, name) == 0)
            res = e->sid;
    }

    return res;
}

/* returns true if a given sid of a variable is active (being read) */
static int is_active(const char name[], long sid)
{
    int res = 0;

    for (List *it = gents; !res && it != NULL; it = it->next) {
        Entry *e = it->elem;
        res = (e->state == RUNNABLE && e->a_type == READ
                && e->version == sid && str_cmp(e->name, name) == 0);
    }

    return res;
}

/* returns true if the entry is
    * non-active, non-latest committed write
    * committed read
    * reverted read or write
*/
static int rm_entry(void *elem, const void *cmp)
{
    Entry *e = elem;
    int rm = 0;
    if (e->a_type == WRITE && e->state == COMMITTED) {
        long sid = e->sid;
        char *name = e->name;

        long rsid = get_rsid(MAX_LONG, name);

        int act = is_active(name, sid);

        if (sid < rsid && !act)
            rm = 1;
    } else if ((e->a_type == READ && e->state == COMMITTED) ||
               e->state == REVERTED)
    {
        rm = 1;
    }

    if (rm)
        mem_free(e);

    return rm;
}

static Entry *add_entry(long sid,
                        const char name[],
                        int a_type,
                        long version,
                        int state)
{

    Entry *e = (Entry*) mem_alloc(sizeof(Entry));
    e->sid = sid;
    str_cpy(e->name, name);
    str_cpy(e->wvid, "");
    e->a_type = a_type;
    e->version = version;
    e->state = state;
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

static Entry *get_min_waiting(const char name[], int a_type)
{
    long min_sid = MAX_LONG;
    Entry *res = 0;

    for (List *it = gents; it != NULL; it = it->next) {
        Entry *e = it->elem;
        if (e->state == WAITING && e->sid < min_sid
                && e->a_type == a_type && str_cmp(e->name, name) == 0)
        {
            res = e;
            min_sid = e->sid;
        }
    }

    return res;
}

static List *list_waiting(long sid, const char name[], int a_type)
{
    List *dest = NULL;

    for (List *it = gents; it != NULL; it = it->next) {
        Entry *e = it->elem;
        if (e->state == WAITING && e->sid <= sid
                && e->a_type == a_type && str_cmp(e->name, name) == 0)
            dest = add(dest, e);
    }

    return dest;
}

static long get_wsid(const char name[])
{
    long res = -1;

    for (List *it = gents; it != NULL; it = it->next) {
        Entry *e = it->elem;
        if (e->a_type == WRITE && e->state == RUNNABLE
                && str_cmp(e->name, name) == 0)
        {
            res = e->sid;
            break;
        }
    }

    return res;
}

static int rm_volume(void *elem, const void *cmp)
{
    Vol *e = elem;
    if (str_cmp(e->id, cmp) == 0) {
        vars_free(e->vars);
        mem_free(e);

        return 1;
    }

    return 0;
}

static Vol *replace_volume(const char *vid, Vars *vars)
{
    Vol *vol = (Vol*) mem_alloc(sizeof(Vol));
    vol->vars = vars;
    str_cpy(vol->id, vid);

    rm(&gvols, vid, rm_volume);
    gvols = add(gvols, vol);

    return vol;
}

static Vol *get_volume(const char *vid)
{
    Vol *vol = NULL;
    for (List *it = gvols; it != NULL && vol == NULL; it = it->next) {
        Vol *v = it->elem;
        if (str_cmp(v->id, vid) == 0)
            vol = v;
    }

    return vol;
}

static void closest_vol(char *vid, const char *addr, const char *name, long ver)
{
    Vol *v = NULL;
    str_cpy(vid, "");

    for (List *it = gvols; it != NULL; it = it->next) {
        Vol *vol = it->elem;

        if (str_len(name) == 0 || ver < 1L ||
            vars_scan(vol->vars, name, ver) > -1)
            v = it->elem;

        if (v != NULL && str_match(addr, vol->id, ':'))
            break;
    }

    if (v != NULL)
        str_cpy(vid, v->id);
}

static void set_vols(Vars *v, const char *addr)
{
    for (int i = 0; i < v->len; ++i)
        closest_vol(v->vols[i], addr, v->names[i], v->vers[i]);
}

static void current_state(long vers[])
{
    for (int i = 0; i < gvars.len; ++i)
        vers[i] = 0;

    for (List *it = gents; it != NULL; it = it->next) {
        Entry *e = it->elem;
        if (e->a_type == WRITE && e->state == COMMITTED) {
            int idx = array_scan(gvars.names, gvars.len, e->name);
            if (vers[idx] < e->version)
                vers[idx] = e->version;
        }
    }
}

extern void wstate()
{
    long vers[gvars.len];
    current_state(vers);

    char sid[MAX_NAME];
    char *buf = mem_alloc(gvars.len * (MAX_NAME + MAX_NAME));

    sys_move(gstate_bak, gstate);

    int off = 0;
    for (int i = 0; i < gvars.len; ++i) {
        str_from_sid(sid, vers[i]);
        off += str_print(buf + off, "%s,%s\n", gvars.names[i], sid);
    }

    IO *io = sys_open(gstate, CREATE | WRITE);
    sys_write(io, buf, off);
    sys_close(io);

    mem_free(buf);

    sys_remove(gstate_bak);
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
                rsid = get_rsid(e->sid, e->name);
            else {
                Vol *vol = get_volume(e->wvid);
                if (vol != NULL)
                    vars_put(vol->vars, e->name, e->version);
            }

            Entry *we = get_min_waiting(e->name, WRITE);

            long wsid = MAX_LONG;
            if (we != NULL) {
                wsid = we->sid;
                sig = add(sig, we);
            }

            List *rents = list_waiting(wsid, e->name, READ);
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
        vars_free(vol->vars);
        mem_free(gvols->elem);
    }

    for (int i = 0; i < gvars.len; ++i)
        mem_free(gvars.names[i]);

    mem_free(gcode.buf);

    mon_unlock(gmon);
    mon_free(gmon);
}

static void tx_init(const char *source, const char *state)
{
    gmon = mon_new();
    gents = NULL;
    gvols = NULL;
    gcode.buf = sys_load(source);
    gcode.len = str_len(gcode.buf) + 1;
    str_cpy(gstate, state);
    str_print(gstate_bak, "%s.backup", gstate);

    last_sid = 1;
    gvars.len = 0;

    if (sys_exists(gstate_bak))
        sys_move(gstate, gstate_bak);

    /* create the state file if it does not exist */
    IO *io = sys_open(gstate, CREATE | WRITE);
    sys_close(io);

    mon_lock(gmon);

    long vers[MAX_VARS];
    char *names[MAX_VARS];
    int len;

    char *buf = sys_load(gstate);
    char **lines = str_split(buf, '\n', &len);
    if (str_len(lines[len - 1]) == 0)
        --len;

    for (int i = 0; i < len; ++i) {
        int cnt;
        char **name_sid = str_split(lines[i], ',', &cnt);
        if (cnt != 2)
            sys_die("bad line %s:%d\n", gstate, i + 1);

        names[i] = str_dup(name_sid[0]);
        vers[i] = str_to_sid(name_sid[1]);

        mem_free(name_sid);
    }

    mem_free(lines);
    mem_free(buf);

    gvars.len = len;

    for (int i = 0; i < gvars.len; ++i) {
        gvars.names[i] = names[i];
        long sid = vers[i];

        if (sid > last_sid)
            last_sid = sid;

        Entry *e = add_entry(sid, gvars.names[i], WRITE, sid, COMMITTED);
        mon_free(e->mon);
    }

    Env *env = env_new(source, gcode.buf);

    for (int i = 0; i < env->vars.len; ++i) {
        /* FIXME: we also need to remove vars from gvars if they do
                  not exist in the env */
        char *name = env->vars.names[i];
        int idx = array_scan(gvars.names, gvars.len, env->vars.names[i]);
        if (idx < 0) {
            gvars.names[gvars.len++] = str_dup(name);
            Entry *e = add_entry(1, name, WRITE, 1, COMMITTED);
            mon_free(e->mon);
        }
    }

    wstate();
    mon_unlock(gmon);

    env_free(env);
}

static Vars *volume_sync(const char *vid, Vars *in)
{
    mon_lock(gmon);

    replace_volume(vid, in);

    /* populate out variable with the (WRITE/COMMITED) variables */
    Vars *out = vars_new(0);
    for (List *it = gents; it != NULL; it = it->next) {
        Entry *e = it->elem;
        if (e->a_type == WRITE && e->state == COMMITTED)
            vars_put(out, e->name, e->version);
    }
    set_vols(out, vid);

    mon_unlock(gmon);

    sys_log('T', "volume %s sync called\n", vid);
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
extern long enter(const char *eid, Vars *rvars, Vars *wvars, Mon *m)
{
    int rw = 0;
    long sid = 0;
    Entry *re[rvars->len];
    Entry *we[wvars->len];

    mon_lock(gmon);

    char wvid[MAX_ADDR] = "";
    closest_vol(wvid, eid, "", 0);

    sid = ++last_sid;
    for (int i = 0; i < wvars->len; ++i) {
        const char *name = wvars->names[i];
        int state = WAITING;
        if (get_wsid(name) == -1)
            state = RUNNABLE;

        we[i] = add_entry(sid, name, WRITE, sid, state);
        str_cpy(we[i]->wvid, wvid);

        rw = 1;
    }

    for (int i = 0; i < rvars->len; ++i) {
        const char *name = rvars->names[i];
        long rsid = get_rsid(sid, name);

        int state = RUNNABLE;
        if (rw) {
            long wsid = get_wsid(name);
            if (wsid > -1L && sid > wsid) {
                rsid = -1L;
                state = WAITING;
            }
        }

        re[i] = add_entry(sid, name, READ, rsid, state);
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

    set_vols(rvars, eid);
    for (int i = 0; i < wvars->len; ++i)
        str_cpy(wvars->vols[i], wvid);

    mon_unlock(gmon);

    return sid;
}

extern void tx_attach(const char *address)
{
    gio = sys_connect(address);
}

static void *tx_thread(void *io)
{
    long sid = 0;
    char vid[MAX_ADDR] = "";

    for (;;) {
        int msg = 0;
        if (sys_readn(io, &msg, sizeof(int)) != sizeof(int))
            goto exit;

        if (msg == T_ENTER) {
            if (sid != 0)
                goto exit;

            char eid[MAX_ADDR] = "";
            if (sys_readn(io, eid, MAX_ADDR) != MAX_ADDR)
                goto exit;

            Vars *rvars = vars_read(io);
            if (rvars == NULL)
                goto exit;

            Vars *wvars = vars_read(io);
            if (wvars == NULL) {
                vars_free(rvars);
                goto exit;
            }

            sid = enter(eid, rvars, wvars, NULL);

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
            long s = sid;
            sid = 0; /* at this point we cannot revert anymore */

            /* FIXME: we can commit and then fail to notify the client */
            msg = R_FINISH;
            if (sys_write(io, &msg, sizeof(int)) < 0 ||
                sys_write(io, &final_state, sizeof(int)) < 0)
                goto exit;

            sys_log('T', "%016X finished\n", s);
        } else if (msg == T_SYNC) {
            if (sys_readn(io, vid, MAX_ADDR) != MAX_ADDR)
                goto exit;

            Vars *in = vars_read(io);
            if (in == NULL)
                goto exit;

            Vars *out = volume_sync(vid, in);
            msg = R_SYNC;

            if (sys_write(io, &msg, sizeof(int)) < 0)
                goto exit;
            
            int res = vars_write(out, io);
            vars_free(out);
            if (res < 0)
                goto exit;
        } else if (msg == T_SOURCE) {
            msg = R_SOURCE;
            if (sys_write(io, &msg, sizeof(int)) < 0 ||
                sys_write(io, &gcode.len, sizeof(int)) < 0 ||
                sys_write(io, gcode.buf, gcode.len) < 0)
                goto exit;
        }
    }

exit:
    if (sid != 0) {
        sys_log('T', "transaction %016X failed\n", sid);
        finish(sid, REVERTED);
    }

    if (str_cmp(vid, "") != 0) {
        mon_lock(gmon);
        rm(&gvols, vid, rm_volume);
        mon_unlock(gmon);

        sys_log('T', "volume %s disconnected\n", vid);
    }

    sys_close(io);
    return NULL;
}

static void *serve(void *sio)
{
    for (;;) {
        IO *cio = sys_accept(sio);
        sys_thread(tx_thread, cio);
    }

    sys_close(sio);
    return NULL;
}

extern void tx_server(const char *source, const char *state, int *port)
{
    int standalone = *port != 0;
    tx_init(source, state);

    IO *sio = sys_socket(port);

    sys_log('T', "started source=%s, state=%s, port=%d\n", 
                 source, state, *port);

    if (standalone)
        serve(sio);
    else {
        sys_thread(serve, sio);
        char address[MAX_ADDR];
        str_print(address, "127.0.0.1:%d", *port);
        tx_attach(address);
    }
}

extern char *tx_program()
{
    int msg = T_SOURCE;
    if (sys_write(gio, &msg, sizeof(int)) < 0)
        sys_die("T_SOURCE failed\n");

    int size = 0;
    if (sys_readn(gio, &msg, sizeof(int)) != sizeof(int) ||
        msg != R_SOURCE ||
        sys_readn(gio, &size, sizeof(int)) != sizeof(int) ||
        size < 0 /* FIXME: || size > MAX_CODE_SIZE */)
        sys_die("R_ENTER failed\n");

    char *code = mem_alloc(size);
    if (sys_readn(gio, code, size) != size)
        sys_die("R_ENTER failed to retrieve the program\n");

    return code;
}

extern long tx_enter(const char *eid, Vars *rvars, Vars *wvars)
{
    int msg = T_ENTER;
    if (sys_write(gio, &msg, sizeof(int)) < 0 ||
        sys_write(gio, eid, MAX_ADDR) < 0 ||
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

extern Vars *tx_volume_sync(const char *vid, Vars *in)
{
    int msg = T_SYNC;
    if (sys_write(gio, &msg, sizeof(int)) < 0 ||
        sys_write(gio, vid, MAX_ADDR) < 0)
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
                  e->sid, e->name, a_type, e->version, state);

    }

    sys_print("%-8s %-32s %-8s\n",
              "VOLUME", "VARIABLE", "SID");
    for (List *it = gvols; it != NULL; it = it->next) {
        Vol *vol = it->elem;
        for (int i = 0; i < vol->vars->len; ++i)
            sys_print("%-8d %-32s %-8d\n",
                      vol->id, vol->vars->names[i], vol->vars->vers[i]);
    }

    mon_unlock(gmon);
}
