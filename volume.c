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
#include "transaction.h"
#include "value.h"
#include "tuple.h"
#include "expression.h"
#include "summary.h"
#include "relation.h"

static void create_empty(State *s)
{
    for (int i = 0; i < s->len; ++i) {
        char *var = s->vars[i];
        long ver = s->vers[i];

        if (ver == 1) {
            sys_close(vol_open(var, ver, CREATE | WRITE));
            Rel *empty = rel_empty();
            rel_store(var, ver, empty);
            rel_free(empty);
        }
    }
}

static int look_up(char const var[MAX_NAME], long ver, State *s)
{
    int idx = -1;
    for (int i = 0; i < s->len && idx < 0; ++i)
        if (str_cmp(var, s->vars[i]) == 0 && ver == s->vers[i])
            idx = i;

    return idx;
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

extern void vol_init()
{
    State disk = {.len = 0}, tx = {.len = 0};

    tx_volume_sync(&disk, &tx);

    /* init the disk state variable + clean up old files */
    int num_files;
    char **files = sys_list(fs_path, &num_files);
    for (int i = 0; i < num_files; ++i) {
        char var[MAX_NAME] = "";
        long ver = parse(files[i], var);
        if (ver > 0) {
            if (look_up(var, ver, &tx) < 0)
                vol_remove(var, ver);
            else {
                disk.vers[disk.len] = ver;
                str_cpy(disk.vars[disk.len++], var);
            }
        }
    }
    mem_free(files);

    create_empty(&tx);
}

extern IO *vol_open(const char *name, long version, int mode)
{
    char file[MAX_FILE_PATH];
    fs_fpath(file, name, version);

    return sys_open(file, mode);
}

extern void vol_remove(const char *name, long version)
{
    char file[MAX_FILE_PATH];
    fs_fpath(file, name, version);
    sys_remove(file);
}
