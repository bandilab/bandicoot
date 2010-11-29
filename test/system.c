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

    int wfd = sys_open("tmp_file", CREATE | WRITE);
    int rfd = sys_open("tmp_file", READ);

    if (wfd < 0 || rfd < 0)
        fail();

    sys_write(wfd, str, len);
    if (sys_readn(rfd, buf, len) != len)
        fail();

    if (mem_cmp(buf, str, len) != 0)
        fail();

    char *txt = sys_load("tmp_file");
    if (str_cmp(txt, str) != 0)
        fail();

    mem_free(txt);

    sys_remove("tmp_file");

    sys_close(rfd);
    sys_close(wfd);

    if (sys_exists("doesnotexist"))
        fail();

    char **names = sys_lsdir("bin/test/lsdir", &len);
    if (len != 3)
        fail();

    if (array_scan(names, len, ".") == -1)
        fail();
    if (array_scan(names, len, "..") == -1)
        fail();
    if (array_scan(names, len, "one_dir") == -1)
        fail();

    mem_free(names);

    return 0;
}
