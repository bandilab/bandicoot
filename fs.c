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
#include "string.h"
#include "fs.h"

char fs_path[MAX_FILE_PATH];
char fs_source[MAX_FILE_PATH];
char fs_state[MAX_FILE_PATH];
char fs_state_bak[MAX_FILE_PATH];

static char SID_TO_STR[] = {
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
static char STR_TO_SID[127];

extern int fs_sid_to_str(char *dest, long sid)
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

    return i;
}

extern long fs_str_to_sid(char *str)
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

extern void fs_init(const char *p)
{
    if (str_len(p) > MAX_FILE_PATH)
        sys_die("volume path '%s' is too long\n", p);

    int plen = str_cpy(fs_path, p) - 1;
    for (; plen > 0 && fs_path[plen] == '/'; --plen)
        fs_path[plen] = '\0';

    str_print(fs_source, "%s/.source", fs_path);
    str_print(fs_state, "%s/.state", fs_path);
    str_print(fs_state_bak, "%s/.state.bak", fs_path);

    for (int i = '0'; i <= '9'; ++i)
        STR_TO_SID[i] = i - '0';
    for (int i = 'A'; i <= 'F'; ++i)
        STR_TO_SID[i] = i - 'A' + 10;
    for (int i = 'a'; i <= 'f'; ++i)
        STR_TO_SID[i] = i - 'a' + 10;
}
