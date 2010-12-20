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
#include "number.h"
#include "system.h"
#include "head.h"
#include "volume.h"
#include "value.h"
#include "tuple.h"
#include "expression.h"
#include "summary.h"
#include "relation.h"
#include "environment.h"

static char path[MAX_PATH];
static char source[MAX_PATH];
static char state[MAX_PATH];
static char state_bak[MAX_PATH];

static int SID_LEN = 17;
static char SID_TO_STR[] = {
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
static char STR_TO_SID[127];

static void sid_to_str(char *dest, long sid)
{
#ifdef LP64
    int i = 0, shift = 60;
#else
    int i = 0, shift = 28;

    while (i < 8)
        dest[i++] = SID_TO_STR[0];
#endif

    while (shift >= 0) {
        dest[i++] = SID_TO_STR[(sid >> shift) & 0x0F];
        shift -= 4;
    }

    dest[i] = '\0';
}

static long str_to_sid(char *str)
{
#ifdef LP64
    int i = 0, shift = 60;
#else
    int i = 8, shift = 28;
#endif

    long sid = 0;
    while (shift >= 0) {
        sid = sid | (STR_TO_SID[(int) str[i++]] & 0x0F) << shift;
        shift -= 4;
    }

    return sid;
}

static int read_state(char *names[], long vers[])
{
    if (sys_exists(state_bak))
        sys_move(state, state_bak);

    int len;
    char *buf = sys_load(state);
    char **lines = str_split(buf, '\n', &len);
    if (str_len(lines[len - 1]) == 0)
        --len;

    for (int i = 0; i < len; ++i) {
        int cnt;
        char **name_sid = str_split(lines[i], ',', &cnt);
        if (cnt != 2)
            sys_die("bad line %s:%d\n", state, i + 1);

        names[i] = str_dup(name_sid[0]);
        vers[i] = str_to_sid(name_sid[1]);

        mem_free(name_sid);
    }

    mem_free(lines);
    mem_free(buf);

    return len;
}

static long parse(const char *file, char *rel)
{
    int i = 0;
    for (; file[i] != '\0' && file[i] != '-'; ++i)
        ;

    long sid = -1;
    if (file[i] == '-') {
        mem_cpy(rel, file, i);
        rel[i++] = '\0';

        sid = str_to_sid((char*) file + i);
    }

    return sid;
}

static void fpath(char *res, const char *var, long sid)
{
    res += str_cpy(res, path);
    res += str_cpy(res, "/");
    res += str_cpy(res, var);
    res += str_cpy(res, "-");
    sid_to_str(res, sid);
}

static void remove(char *var, long sid)
{
    char file[MAX_PATH];
    fpath(file, var, sid);
    sys_remove(file);
}

static void cleanup()
{
    long vers[MAX_VARS];
    char *vars[MAX_VARS];
    int len = read_state(vars, vers);

    int num_files;
    char **files = sys_list(path, &num_files);
    for (int i = 0; i < num_files; ++i) {
        char rel[MAX_NAME] = "";
        long sid = parse(files[i], rel);
        int idx = array_scan(vars, len, rel);

        if (sid > 0 && idx > -1 && vers[idx] != sid)
            remove(rel, sid);
    }

    for (int i = 0; i < len; ++i)
        if (vers[i] > 1) {
            char from[MAX_PATH];
            char to[MAX_PATH];
            fpath(from, vars[i], vers[i]);
            fpath(to, vars[i], 1);

            sys_move(to, from);
            vers[i] = 1;
        }

    mem_free(files);
    vol_wstate(vars, vers, len);

    for (int i = 0; i < len; ++i)
        mem_free(vars[i]);
}

extern void vol_wstate(char *names[], long vers[], int len)
{
    char sid[SID_LEN];
    char *buf = mem_alloc(len * (MAX_NAME + SID_LEN));

    sys_move(state_bak, state);

    int off = 0;
    for (int i = 0; i < len; ++i) {
        sid_to_str(sid, vers[i]);
        off += str_print(buf + off, "%s,%s\n", names[i], sid);
    }

    int fd = sys_open(state, CREATE | WRITE);
    sys_write(fd, buf, off);
    sys_close(fd);

    mem_free(buf);

    sys_remove(state_bak);
}

static void init(const char *p)
{
    if (str_len(p) > MAX_PATH)
        sys_die("volume path '%s' is too long\n", p);

    int plen = str_cpy(path, p) - 1;
    for (; path[plen] == '/' && plen > 0; --plen)
        path[plen] = '\0';

    str_print(source, "%s/.source", path);
    str_print(state, "%s/.state", path);
    str_print(state_bak, "%s/.state.bak", path);

    for (int i = '0'; i <= '9'; ++i)
        STR_TO_SID[i] = i - '0';
    for (int i = 'A'; i <= 'F'; ++i)
        STR_TO_SID[i] = i - 'A' + 10;
    for (int i = 'a'; i <= 'f'; ++i)
        STR_TO_SID[i] = i - 'a' + 10;
}

extern char *vol_init(const char *p)
{
    init(p);
    cleanup();

    return source;
}

extern void vol_deploy(const char *p, const char *new_src)
{
    init(p);
    if (sys_empty(path)) {
        int fd = sys_open(source, CREATE | WRITE);
        sys_close(fd);

        fd = sys_open(state, CREATE | WRITE);
        sys_close(fd);
    } else
        cleanup();

    Env *new_env = env_new(new_src);
    Env *old_env = env_new(source);

    if (!env_compat(old_env, new_env))
        sys_die("cannot deploy source file '%s'\n", new_src);

    int old_len = old_env->vars.len;
    int new_len = new_env->vars.len;
    char **old_vars = old_env->vars.names;
    char **new_vars = new_env->vars.names;
    long new_vers[new_len];

    for (int i = 0; i < old_len; ++i)
        if (array_scan(new_vars, new_len, old_vars[i]) < 0)
            remove(old_vars[i], 1);

    for (int i = 0; i < new_len; ++i) {
        new_vers[i] = 1;
        if (array_scan(old_vars, old_len, new_vars[i]) < 0)
            sys_close(vol_open(new_vars[i], 1, CREATE | WRITE));
    }

    vol_wstate(new_vars, new_vers, new_len);
    sys_cpy(source, new_src);

    env_free(new_env);
    env_free(old_env);
}

extern int vol_open(const char *name, long version, int mode)
{
    char file[MAX_PATH];
    fpath(file, name, version);

    return sys_open(file, mode);
}
