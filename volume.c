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
#include "system.h"
#include "array.h"
#include "memory.h"
#include "string.h"
#include "number.h"
#include "fs.h"
#include "head.h"
#include "value.h"
#include "tuple.h"
#include "expression.h"
#include "summary.h"
#include "relation.h"
#include "transaction.h"

#include "volume.h"

const int SUFFIX_LEN = 5;
const char *SUFFIX = ".part";

static const int T_READ = 1;
static const int R_READ = 2;
static const int T_WRITE = 3;
static const int R_WRITE = 4;

static char name[32];
static long long id;

static void fpath(char *res, const char *var, long sid, int part)
{
    res += str_cpy(res, fs_path);
    res += str_cpy(res, "/");
    res += str_cpy(res, var);
    res += str_cpy(res, "-");
    res += fs_sid_to_str(res, sid);
    if (part)
        str_cpy(res, SUFFIX);
}

static void vol_remove(const char *name)
{
    char file[MAX_FILE_PATH], *f = file;
    f += str_cpy(f, fs_path);
    f += str_cpy(f, "/");
    f += str_cpy(f, name);
    sys_remove(file);
}

static int is_partial(const char *file)
{
    int len = str_len(file);
    char *suffix = (char*) (file + len - SUFFIX_LEN);

    return str_cmp(suffix, SUFFIX) == 0 ? 1 : 0;
}

extern long parse(const char *file, char *rel)
{
    int i = 0;
    for (; file[i] != '\0' && file[i] != '-'; ++i)
        ;

    long sid = -1;
    if (file[i] == '-' && !is_partial(file)) {
        mem_cpy(rel, file, i);
        rel[i++] = '\0';

        sid = fs_str_to_sid((char*) file + i);
    }

    return sid;
}

static TBuf *read(const char *var, long ver)
{
    char file[MAX_FILE_PATH];
    fpath(file, var, ver, 0);

    IO *fio = sys_open(file, READ);
    TBuf *buf = tbuf_read(fio);
    sys_close(fio);

    return buf;
}

static void write(const char *var, long ver, TBuf *buf)
{
    char part[MAX_FILE_PATH], file[MAX_FILE_PATH];
    fpath(part, var, ver, 1);
    fpath(file, var, ver, 0);

    IO *fio = sys_open(part, CREATE | WRITE);
    tbuf_write(buf, fio);
    sys_close(fio);
    sys_move(file, part);
}

static int read_var(IO *io, char *var, long *ver)
{
    if (sys_readn(io, var, MAX_NAME) != MAX_NAME)
        return 0;

    if (sys_readn(io, ver, sizeof(long)) != sizeof(long))
        return 0;

    return 1;
}

static void copy_file(char *var, long ver, long long vol)
{
    char file[MAX_FILE_PATH];
    fpath(file, var, ver, 0);

    if (ver <= 1 || vol == id || sys_exists(file))
        return;

    if (vol == 0) {
        sys_print("volume: %s, file copy '%s-%016X' failed\n", name, var, ver);
        return;
    }

    /* FIXME: this dies if read fails */
    TBuf *buf = vol_read(vol, var, ver);
    /* FIXME: this write can fail as might alread have this file
              it can happen where the sync is behind the commit, i.e. the
              variables passed intot the tx_volume_sync are not up to date and
              there are newer files on the disk */
    write(var, ver, buf);
    tbuf_free(buf);

    sys_print("volume: %s, file copy '%s-%016X' succeeded\n", name, var, ver);
}

static Vars *sync_tx(long long id)
{
    Vars *disk = vars_new(0);

    /* init the disk state variable */
    int num_files;
    char **files = sys_list(fs_path, &num_files);
    for (int i = 0; i < num_files; ++i) {
        char var[MAX_NAME] = "";
        long ver = parse(files[i], var);
        if (ver > 0)
            vars_put(disk, var, ver);
    }
    mem_free(files);

    Vars *tx = tx_volume_sync(id, disk);

    /* remove old versions */
    char file[MAX_FILE_PATH];
    for (int i = 0; i < disk->len; ++i)
        if (vars_scan(tx, disk->vars[i], disk->vers[i]) < 0) {
            fpath(file, disk->vars[i], disk->vers[i], 0);
            sys_remove(file);
        }

    /* sync with other volumes */
    for (int i = 0; i < tx->len; ++i)
        copy_file(tx->vars[i], tx->vers[i], tx->vols[i]);

    vars_free(disk);
    return tx;
}

static void *exec_server(void *arg)
{
    IO *cio = NULL, *sio = (IO*) arg;
    char var[MAX_NAME];
    long ver;

    for (;;) {
        cio = sys_accept(sio);

        int msg = 0;
        if (sys_readn(cio, &msg, sizeof(int)) == sizeof(int)) {
            if (msg == T_READ && read_var(cio, var, &ver)) {
                TBuf *buf = read(var, ver);
                if (buf != NULL) {
                    tbuf_write(buf, cio);
                    tbuf_free(buf);
                    sys_write(cio, &R_READ, sizeof(int));
                }
            } else if (msg == T_WRITE && read_var(cio, var, &ver)) {
                TBuf *buf = tbuf_read(cio);
                if (buf != NULL) {
                    write(var, ver, buf);
                    tbuf_free(buf);
                    sys_write(cio, &R_WRITE, sizeof(int));
                }
            }
        }

        sys_close(cio);
    }
    sys_close(sio);

    return NULL;
}

static void *exec_cleanup(void *arg)
{
    for (;;) {
        sys_sleep(30);
        vars_free(sync_tx(id));
    }

    return NULL;
}

extern long long vol_init()
{
    int p = 0;
    IO *io = sys_socket(&p);
    id = sys_address(p);
    sys_address_print(name, id);

    /* clean up partial files */
    int num_files;
    char **files = sys_list(fs_path, &num_files);
    for (int i = 0; i < num_files; ++i)
        if (is_partial(files[i]))
            vol_remove(files[i]);
    mem_free(files);

    Vars *tx = sync_tx(id);

    /* create new empty files */
    for (int i = 0; i < tx->len; ++i)
        if (tx->vers[i] == 1) {
            TBuf *buf = tbuf_new();
            write(tx->vars[i], tx->vers[i], buf);
            tbuf_free(buf);
        }

    vars_free(tx);
    vars_free(sync_tx(id)); /* let the TX know immediately about the new files */

    sys_thread(exec_server, io);
    sys_thread(exec_cleanup, NULL);

    return id;
}

extern TBuf *vol_read(long long id, const char *name, long version)
{
    char var[MAX_NAME] = "";
    str_cpy(var, name);

    IO *io = sys_connect(id);

    if (sys_write(io, &T_READ, sizeof(int)) < 0 ||
        sys_write(io, var, sizeof(var)) < 0 ||
        sys_write(io, &version, sizeof(version)) < 0)
        sys_die("volume: read failed to send '%s-%llu'\n", name, version);

    TBuf *res = tbuf_read(io);
    if (res == NULL)
        sys_die("volume: read failed for '%s-%llu'\n", name, version);

    /* confirmation of the full read */
    int msg = 0;
    if (sys_readn(io, &msg, sizeof(int)) != sizeof(int) || msg != R_READ)
        sys_die("volume: read failed to confirm '%s-%llu'\n", name, version);

    sys_close(io);
    return res;
}

extern void vol_write(long long id, TBuf *buf, const char *name, long version)
{
    char var[MAX_NAME] = "", sid[MAX_NAME] = "";
    fs_sid_to_str(sid, version);
    str_cpy(var, name);

    IO *io = sys_connect(id);

    if (sys_write(io, &T_WRITE, sizeof(int)) < 0 ||
        sys_write(io, var, sizeof(var)) < 0 ||
        sys_write(io, &version, sizeof(version)) < 0)
        sys_die("volume: write failed to send '%s-%s'\n", name, sid);

    if (tbuf_write(buf, io) < 0)
        sys_die("volume: write failed for '%s-%s'\n", name, sid);

    /* confirmation of the full write */
    int msg = 0;
    if (sys_readn(io, &msg, sizeof(int)) != sizeof(int) || msg != R_WRITE)
        sys_die("volume: write failed to confirm '%s-%s'\n", name, sid);

    sys_close(io);
}
