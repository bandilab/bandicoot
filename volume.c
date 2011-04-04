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
#include "volume.h"
#include "value.h"
#include "tuple.h"
#include "expression.h"
#include "summary.h"
#include "relation.h"
#include "transaction.h"

static void create_empty(Vars *v)
{
    for (int i = 0; i < v->len; ++i) {
        char *var = v->vars[i];
        long ver = v->vers[i];

        if (ver == 1) {
            sys_close(vol_open(var, ver, CREATE | WRITE));
            Rel *empty = rel_empty();
            rel_store(var, ver, empty);
            rel_free(empty);
        }
    }
}

static void vol_remove(const char *name, long version)
{
    char file[MAX_FILE_PATH];
    fs_fpath(file, name, version);
    sys_remove(file);
}

extern long parse(const char *file, char *rel)
{
    int i = 0;
    for (; file[i] != '\0' && file[i] != '-'; ++i)
        ;

    long sid = -1;
    if (file[i] == '-') {
        mem_cpy(rel, file, i);
        rel[i++] = '\0';

        sid = fs_str_to_sid((char*) file + i);
    }

    return sid;
}

static void sync_tx(long long id)
{
    Vars *tx = NULL, *disk = NULL;

    tx = tx_volume_sync(id, disk);
    disk = vars_new(tx->len);

    create_empty(tx);

    /* init the disk state variable + clean up old files */
    int num_files;
    char **files = sys_list(fs_path, &num_files);
    for (int i = 0; i < num_files; ++i) {
        char var[MAX_NAME] = "";
        long ver = parse(files[i], var);
        if (ver > 0) {
            if (vars_scan(tx, var, ver) < 0)
                vol_remove(var, ver);
            else {
                disk->vers[disk->len] = ver;
                str_cpy(disk->vars[disk->len++], var);
            }
        }
    }
    mem_free(files);

    Vars *tx2 = tx_volume_sync(id, disk);

    mem_free(tx);
    mem_free(tx2);
}

extern void vol_init()
{
    long long id = VOLUME_ID;

    sync_tx(id);
}

extern IO *vol_open(const char *name, long version, int mode)
{
    char file[MAX_FILE_PATH];
    fs_fpath(file, name, version);

    return sys_open(file, mode);
}
