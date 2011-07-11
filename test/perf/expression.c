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

int main()
{
    char *names[] = {"id", "int_val", "real_val", "str_val"};
    Type types[] = {Int, Int, Real, String};
    Head *h = head_new(names, types, 4);

    int i, j, SIZE = 1000;
    Tuple **t = mem_alloc(SIZE * sizeof(Tuple*));
    for (i = 0; i < SIZE; ++i) {
        Value vals[4];
        int v_int = 1000;
        double v_real = 0.25;
        char *v_str = "string_1";
        vals[0] = val_new_int(&i);
        vals[1] = val_new_int(&v_int);
        vals[2] = val_new_real(&v_real);
        vals[3] = val_new_str(v_str);
        t[i] = tuple_new(vals, 4);
    }

    Expr *n1, *n2, *n3;
    int sa, ia, ra;
    Type sat, iat, rat;
    head_attr(h, "str_val", &sa, &sat);
    head_attr(h, "int_val", &ia, &iat);
    head_attr(h, "real_val", &ra, &rat);
    n1 = expr_eq(expr_str("string_1"), expr_attr(sa, sat));
    n2 = expr_eq(expr_int(1000), expr_attr(ia, iat));
    n3 = expr_eq(expr_real(0.25), expr_attr(ra, rat));
    Expr *e = expr_and(n1, expr_and(n2, n3));

    long time = sys_millis();
    for (j = 0; j < SIZE; ++j)
        for (i = 0; i < SIZE; ++i)
            expr_bool_val(e, t[i], NULL);

    sys_print("%dK expressions evaluated in %dms\n", j, sys_millis() - time);

    for (i = 0; i < SIZE; ++i)
        tuple_free(t[i]);

    return 0;
}
