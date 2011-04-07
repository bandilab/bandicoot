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

    vars_free(disk);
    return tx;
}

static void *exec_cleanup(void *arg)
{
    long long id = *((long*)arg);

    for (;;) {
        sys_sleep(30);
        vars_free(sync_tx(id));
    }

    return NULL;
}

extern void vol_init()
{
    /* FIXME: need to generate a new id based on host and port */
    long long id = VOLUME_ID;

    /* clean up partial files */
    int num_files;
    char **files = sys_list(fs_path, &num_files);
    for (int i = 0; i < num_files; ++i)
        if (is_partial(files[i]))
            vol_remove(files[i]);

    Vars *tx = sync_tx(id);

    /* create new empty files */
    for (int i = 0; i < tx->len; ++i)
        if (tx->vers[i] == 1) {
            Rel *empty = rel_empty();
            rel_store(tx->vars[i], tx->vers[i], empty);
            rel_free(empty);
        }

    vars_free(tx);
    vars_free(sync_tx(id)); /* let the TX know immediately about the new files */

    sys_thread(exec_cleanup, &id);
}

extern TBuf *vol_read(const char *name, long version)
{
    char file[MAX_FILE_PATH];
    fpath(file, name, version, 0);

    IO *io = sys_open(file, READ);
    TBuf *res = tbuf_read(io);
    if (res == NULL)
        sys_die("volume: failed to load '%s'\n", file);

    sys_close(io);
    return res;
}

extern void vol_write(TBuf *buf, const char *name, long version)
{
    char part[MAX_FILE_PATH], file[MAX_FILE_PATH];
    fpath(part, name, version, 1);
    fpath(file, name, version, 0);

    IO *io = sys_open(part, CREATE | WRITE);
    if (tbuf_write(buf, io) < 0)
        sys_die("volume: failed to store '%s'\n", part);
    sys_close(io);
    sys_move(file, part);
}
