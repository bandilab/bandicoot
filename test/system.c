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

#include "common.h"

#include <unistd.h>

int main(void)
{
    char str[] = "hello world\n";
    int len = str_len(str);
    char buf[len];

    IO *wio1 = sys_open("tmp_file1", CREATE | WRITE);
    IO *wio2 = sys_open("tmp_file2", CREATE | WRITE);
    IO *rio1 = sys_open("tmp_file1", READ);
    IO *rio2 = sys_open("tmp_file2", READ);

    if (wio1 == NULL || rio1 == NULL || wio2 == NULL || rio2 == NULL)
        fail();

    sys_write(wio1, str, len);
    if (sys_readn(rio1, buf, len) != len)
        fail();

    if (mem_cmp(buf, str, len) != 0)
        fail();

    sys_write(wio2, str, len);
    if (sys_readn(rio2, buf, len) != len)
        fail();

    if (mem_cmp(buf, str, len) != 0)
        fail();

    char *txt = sys_load("tmp_file1");
    if (str_cmp(txt, str) != 0)
        fail();
    mem_free(txt);

    txt = sys_load("tmp_file2");
    if (str_cmp(txt, str) != 0)
        fail();
    mem_free(txt);

    sys_close(rio1);
    sys_close(rio2);
    sys_close(wio1);
    sys_close(wio2);

    wio1 = sys_open("tmp_file1", WRITE);
    dup2(wio1->fd, 1);

    IO *sout = sys_open("-", WRITE);
    sys_write(sout, str, len);
    sys_close(sout);
    sys_close(wio1);

    rio1 = sys_open("tmp_file1", READ);
    dup2(rio1->fd, 0);

    char *buf2 = sys_load("-");
    if (mem_cmp(str, buf2, len) != 0)
        fail();
    sys_close(rio1);
    mem_free(buf2);

    sys_remove("tmp_file1");
    sys_remove("tmp_file2");

    if (sys_exists("doesnotexist"))
        fail();
    if (!sys_exists("bin/test/lsdir"))
        fail();

    char **names = sys_list("bin/test/lsdir", &len);
    if (len != 1)
        fail();
    if (array_scan(names, len, "one_dir") == -1)
        fail();
    mem_free(names);

    if (!sys_empty("bin/test/lsdir/one_dir"))
        fail();

    return 0;
}
