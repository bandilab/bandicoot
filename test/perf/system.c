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

#include "../common.h"

int main(void)
{
    int file_mb[] = { 64, 128};
    int block[] = { 4096, 8192, 65536 };
    int num_files = sizeof(file_mb) / sizeof(int);
    int num_blocks = sizeof(block) / sizeof(int);
    int i = 0, j = 0;


    for (i = 0; i < num_files; i++) {
        for (j = 0; j < num_blocks; j++) {
            int x, b = block[j];
            int blocks = (file_mb[i] * 1000 * 1000) / b;
            char buf[b], fname[32];

            str_print(fname, "io_perf_%dMB_%dB", file_mb[i], b);
            mem_set(buf, 1 + i * j, b);

            IO *wio = sys_open(fname, CREATE | WRITE);
            IO *rio = sys_open(fname, READ);

            long t = sys_millis();
            for (x = 0; x < blocks; x++)
                sys_write(wio, buf, b);
            sys_print("w %dMB - %dms, block: %dB\n",
                    file_mb[i], sys_millis() - t, b);

            t = sys_millis();
            for (x = 0; x < blocks; x++)
                sys_readn(rio, buf, b);
            sys_print("r %dMB - %dms, block: %dB\n",
                      file_mb[i], sys_millis() - t, b);

            sys_close(wio);
            sys_close(rio);
            sys_remove(fname);
        }
    }
}
