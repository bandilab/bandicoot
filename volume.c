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
#include "system.h"
#include "array.h"
#include "memory.h"
#include "string.h"
#include "number.h"
#include "head.h"
#include "value.h"
#include "tuple.h"
#include "expression.h"
#include "summary.h"
#include "relation.h"
#include "list.h"
#include "transaction.h"
#include "environment.h"

#include "volume.h"

char path[MAX_FILE_PATH];

const int SUFFIX_LEN = 5;
const char *SUFFIX = ".part";

static const int T_READ = 1;
static const int R_READ = 2;
static const int T_WRITE = 3;
static const int R_WRITE = 4;

static char gaddr[MAX_ADDR];

struct {
    char names[MAX_VARS][MAX_NAME];
    int len;
} gvars;

typedef struct {
    char id[MAX_ADDR];
    IO *io;
} Entry;

/* TODO: remove items for closed but unused connections as well */
static List *gvols; /* used to keep connections to the volumes alive */

static void set_path(char *res, const char *name, long long sid, int part)
{
    res += str_cpy(res, path);
    res += str_cpy(res, "/");
    res += str_cpy(res, name);
    res += str_cpy(res, "-");
    res += str_from_sid(res, sid);
    if (part)
        str_cpy(res, SUFFIX);
}

static void vol_remove(const char *name)
{
    char file[MAX_FILE_PATH], *f = file;
    str_print(f, "%s%s%s", path, "/", name);
    sys_remove(file);
}

static int is_partial(const char *file)
{
    int len = str_len(file);
    char *suffix = (char*) (file + len - SUFFIX_LEN);

    return str_cmp(suffix, SUFFIX) == 0 ? 1 : 0;
}

extern long long parse(const char *file, char *rel)
{
    int i = 0;
    for (; file[i] != '\0' && file[i] != '-'; ++i)
        ;

    long long sid = -1;
    if (file[i] == '-' && !is_partial(file)) {
        mem_cpy(rel, file, i);
        rel[i++] = '\0';

        sid = str_to_sid((char*) file + i);
    }

    return sid;
}

static TBuf *read_file(const char *name, long long ver)
{
    char file[MAX_FILE_PATH];
    set_path(file, name, ver, 0);

    IO *fio = sys_open(file, READ);
    TBuf *buf = tbuf_read(fio);
    sys_close(fio);

    return buf;
}

static void write(const char *name, long long ver, TBuf *buf)
{
    char part[MAX_FILE_PATH], file[MAX_FILE_PATH];
    set_path(part, name, ver, 1);
    set_path(file, name, ver, 0);

    IO *fio = sys_open(part, CREATE | WRITE);
    tbuf_write(buf, fio);
    sys_close(fio);
    sys_move(file, part);
}

static int read_var(IO *io, char *name, long long *ver)
{
    if (sys_readn(io, name, MAX_NAME) != MAX_NAME)
        return 0;

    if (sys_readn(io, ver, sizeof(*ver)) != sizeof(*ver))
        return 0;

    return 1;
}

static IO *get_io(const char *vid)
{
    IO *res = NULL;

    for (List *it = gvols; it != NULL; it = it->next) {
        Entry *e = it->elem;
        if (str_cmp(e->id, vid) == 0) {
            res = e->io;
            break;
        }
    }

    return res;
}

static IO *connect(const char *vid)
{
    IO *io = get_io(vid);

    if (io == NULL) {
        io = sys_try_connect(vid, STREAMED);

        if (io != NULL) {
            Entry *e = mem_alloc(sizeof(Entry));
            str_cpy(e->id, vid);
            e->io = io;

            gvols = list_add(gvols, e);
        }
    }

    return io;
}

static int rm_io(void *elem, const void *cmp)
{
    Entry *e = elem;
    if (str_cmp(e->id, cmp) == 0) {
        mem_free(e);

        return 1;
    }

    return 0;
}

static void close(const char *vid)
{
    IO *io = get_io(vid);
    if (io != NULL) {
        list_rm(&gvols, vid, rm_io);
        sys_close(io);
    }
}

static TBuf *read_net(const char *vid, const char *name, long long ver)
{
    TBuf *res = NULL;

    char v[MAX_NAME] = "";
    str_cpy(v, name);

    IO *io = connect(vid);
    if (io == NULL)
        goto exit;

    if (sys_write(io, &T_READ, sizeof(T_READ)) < 0 ||
        sys_write(io, v, sizeof(v)) < 0 ||
        sys_write(io, &ver, sizeof(ver)) < 0)
    {
        io = NULL;
        goto exit;
    }

    res = tbuf_read(io);
    if (res == NULL) {
        io = NULL;
        goto exit;
    }

    /* confirmation of the full read */
    int msg = 0;
    if (sys_readn(io, &msg, sizeof(msg)) != sizeof(msg) || msg != R_READ) {
        io = NULL;
        goto exit;
    }

exit:
    if (io == NULL) {
        if (res != NULL)
            tbuf_free(res);

        res = NULL;
        close(vid);
    }

    return res;
}

static void copy_file(char *name, long long ver, const char *vid)
{
    char file[MAX_FILE_PATH];
    set_path(file, name, ver, 0);

    if (ver <= 1 || sys_exists(file))
        return;

    TBuf *buf = NULL;
    long long time = sys_millis();
    if (str_cmp(vid, "") == 0 || (buf = read_net(vid, name, ver)) == NULL) {
        sys_log('V', "file %s-%016llX copy failed from %s, time %dms\n",
                     name, ver, vid, sys_millis() - time);
        return;
    }

    write(name, ver, buf);
    tbuf_free(buf);

    sys_log('V', "file %s-%016llX copy succeeded from %s, time %dms\n",
                 name, ver, vid, sys_millis() - time);
}

/* FIXME: sync_tx needs to improve
   The below implementation is a safe one which executes the whole
   synchronization with in a transaction (writes to all variables). This
   ensures that:
    * volume does not send partial var list (not yet commited/reverted) to the
      tx
    * the list of variables we get from tx is not only consistent but up to
      date and is not going to get outdated until we finish the copying of the
      missing files from other volumes
*/
static Vars *sync_tx()
{
    char addr[MAX_ADDR] = "";
    Vars *r = vars_new(0), *w = vars_new(gvars.len);
    for (int i = 0; i < gvars.len; ++i)
        vars_put(w, gvars.names[i], 0L);

    long long sid = tx_enter(addr, r, w);

    vars_free(r);
    vars_free(w);

    Vars *disk = vars_new(0);

    /* init the disk state variable */
    int num_files;
    char **files = sys_list(path, &num_files);
    for (int i = 0; i < num_files; ++i) {
        char name[MAX_NAME] = "";
        long long ver = parse(files[i], name);
        if (ver > 0)
            vars_put(disk, name, ver);
    }
    mem_free(files);

    Vars *tx = tx_volume_sync(gaddr, disk);

    /* remove old versions */
    char file[MAX_FILE_PATH];
    for (int i = 0; i < disk->len; ++i)
        if (vars_scan(tx, disk->names[i], disk->vers[i]) < 0) {
            set_path(file, disk->names[i], disk->vers[i], 0);
            sys_remove(file);
        }

    /* sync with other volumes */
    for (int i = 0; i < tx->len; ++i)
        copy_file(tx->names[i], tx->vers[i], tx->vols[i]);

    tx_revert(sid); /* revert is mandatory as this is an artificial tx */

    vars_free(disk);
    return tx;
}

static void *vol_thread(void *io)
{
    char name[MAX_NAME];
    long long ver;

    IO *cio = io;
    while (!cio->err) {
        int msg = 0;
        if (sys_readn(cio, &msg, sizeof(msg)) == sizeof(msg)) {
            if (msg == T_READ && read_var(cio, name, &ver)) {
                TBuf *buf = read_file(name, ver);
                if (buf != NULL) {
                    tbuf_write(buf, cio);
                    tbuf_free(buf);
                    sys_write(cio, &R_READ, sizeof(R_READ));
                }
                sys_log('V', "file %s-%016llX read\n", name, ver);
            } else if (msg == T_WRITE && read_var(cio, name, &ver)) {
                TBuf *buf = tbuf_read(cio);
                if (buf != NULL) {
                    write(name, ver, buf);
                    tbuf_free(buf);
                    sys_write(cio, &R_WRITE, sizeof(R_WRITE));
                }
                sys_log('V', "file %s-%016llX written\n", name, ver);
            }
        }
    }
    sys_close(cio);

    return NULL;
}

static void *serve(void *sio)
{
    for (;;) {
        IO *cio = sys_accept(sio, STREAMED);
        sys_thread(vol_thread, cio);
    }

    sys_close(sio);
    return NULL;
}

static void *exec_cleanup(void *arg)
{
    for (;;) {
        sys_sleep(30);
        vars_free(sync_tx());
    }

    return NULL;
}

static void env_check()
{
    char source[MAX_FILE_PATH];
    str_print(source, "%s/.source", path);

    /* touch the file */
    IO *io = sys_open(source, CREATE | WRITE);
    sys_close(io);

    char *old_buf = sys_load(source), *new_buf = tx_program();
    Env *old = env_new(source, old_buf), *new = env_new("net", new_buf);

    if (!env_compat(old, new))
        sys_die("volume: incompatible volume with the tx\n");

    io = sys_open(source, TRUNCATE | WRITE);
    sys_write(io, new_buf, str_len(new_buf));
    sys_close(io);

    gvars.len = new->vars.len;
    for (int i = 0; i < new->vars.len; ++i)
        str_cpy(gvars.names[i], new->vars.names[i]);

    env_free(old);
    env_free(new);

    mem_free(old_buf);
    mem_free(new_buf);
}

extern char *vol_init(int port, const char *p)
{
    if (str_len(p) > MAX_FILE_PATH)
        sys_die("volume: path '%s' exceeds the maximum length %d\n",
                p, MAX_FILE_PATH);

    int plen = str_cpy(path, p) - 1;
    for (; plen > 0 && path[plen] == '/'; --plen)
        path[plen] = '\0';

    env_check();

    int standalone = port != 0;

    IO *io = sys_socket(&port);
    sys_address(gaddr, port);

    sys_log('V', "started data=%s, port=%d\n", path, port);

    /* clean up partial files */
    int num_files;
    char **files = sys_list(path, &num_files);
    for (int i = 0; i < num_files; ++i)
        if (is_partial(files[i]))
            vol_remove(files[i]);
    mem_free(files);

    /* create new empty files */
    char file[MAX_FILE_PATH];
    for (int i = 0; i < gvars.len; ++i) {
        set_path(file, gvars.names[i], 1L, 0);
        if (!sys_exists(file)) {
            TBuf *buf = tbuf_new();
            write(gvars.names[i], 1L, buf);
            tbuf_free(buf);
        }
    }

    vars_free(sync_tx()); /* let the TX know immediately about the content */

    sys_thread(exec_cleanup, NULL);
    if (standalone)
        serve(io);
    else
        sys_thread(serve, io);

    return gaddr;
}

extern TBuf *vol_read(const char *vid, const char *var, long long ver)
{
    TBuf *res = NULL;
    res = read_net(vid, var, ver);
    if (res == NULL)
        sys_die("volume: read failed %s-%016X'\n", var, ver);

    return res;
}

extern void vol_write(const char *vid,
                      TBuf *buf,
                      const char *var,
                      long long ver)
{
    char v[MAX_NAME] = "", sid[MAX_NAME] = "";
    str_from_sid(sid, ver);
    str_cpy(v, var);

    IO *io = connect(vid);
    if (io == NULL)
        goto exit;

    if (sys_write(io, &T_WRITE, sizeof(T_WRITE)) < 0 ||
        sys_write(io, v, sizeof(v)) < 0 ||
        sys_write(io, &ver, sizeof(ver)) < 0) {
        io = NULL;
        goto exit;
    }

    if (tbuf_write(buf, io) < 0) {
        io = NULL;
        goto exit;
    }

    /* confirmation of the full write */
    int msg = 0;
    if (sys_readn(io, &msg, sizeof(msg)) != sizeof(msg) || msg != R_WRITE)
        io = NULL;

exit:
    if (io == NULL) {
        sys_die("volume: write failed for '%s-%s'\n", v, sid);
        close(vid);
    }
}
