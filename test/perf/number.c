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
    int x = 0, mln = 1;
    char *buf = mem_alloc(sizeof(double) * 10 * 1000 * 1000);

    for (; mln <= 10; mln *= 10) {
        long i, count = mln * 1000 * 1000;

        long t = sys_millis();
        for (i = 0; i < count; i++)
            int_enc(buf + i * sizeof(int), i);
        sys_print("encoded %dM ints in %dms\n", mln, sys_millis() - t);

        t = sys_millis();
        for (i = 0; i < count; i++)
            x += int_dec(buf + i * sizeof(int));
        sys_print("decoded %dM ints in %dms\n", mln, sys_millis() - t);

        t = sys_millis();
        for (i = 0; i < count; i++)
            real_enc(buf + i * sizeof(double), i);
        sys_print("encoded %dM reals in %dms\n", mln, sys_millis() - t);

        t = sys_millis();
        for (i = 0; i < count; i++)
            x += (int) real_dec(buf + i * sizeof(double));
        sys_print("decoded %dM reals in %dms\n", mln, sys_millis() - t);
    }

    x += buf[0] + buf[1 * 1000 * 1000];  /* so that optimizer doesn't remove
                                            inline calls */
    mem_free(buf);

    return x;
}
