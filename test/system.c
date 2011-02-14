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

int main(void)
{
    char str[] = "hello world\n";
    int len = str_len(str);
    char buf[len];

    int wfd1 = sys_open("tmp_file1", CREATE | WRITE);
    int wfd2 = sys_open("tmp_file2", CREATE | WRITE);
    int rfd1 = sys_open("tmp_file1", READ);
    int rfd2 = sys_open("tmp_file2", READ);

    if (wfd1 < 0 || rfd1 < 0 || wfd2 < 0 || rfd2 < 0)
        fail();

    sys_write(wfd1, str, len);
    if (sys_readn(rfd1, buf, len) != len)
        fail();

    if (mem_cmp(buf, str, len) != 0)
        fail();

    sys_send(wfd2, str, len);
    if (sys_recv(rfd2, buf, len) != len)
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

    sys_remove("tmp_file1");
    sys_remove("tmp_file2");

    sys_close(rfd1);
    sys_close(rfd2);
    sys_close(wfd1);
    sys_close(wfd2);

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
