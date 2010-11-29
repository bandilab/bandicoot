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

static int equal(Head *h1, Head *h2)
{
    return head_eq(h1, h2) && head_eq(h2, h1);
}

static void test_join()
{
    char *n1[] = {"id", "string_val", "int_val", "real_val"};
    Type t1[] = {Int, String, Int, Real};

    char *n2[] = {"r2_id", "r2_string_val", "id", "r2_real_val"};
    Type t2[] = {Int, String, Int, Real};

    Head *h1 = head_new(n1, t1, 4);
    Head *h2 = head_new(n2, t2, 4);

    int lpos[MAX_ATTRS], rpos[MAX_ATTRS], len;
    Head *h = head_join(h1, h2, lpos, rpos, &len);

    if (array_find(h->names, h->len, "id") == -1)
        fail();
    if (array_find(h->names, h->len, "r2_id") == -1)
        fail();
    if (array_find(h->names, h->len, "real_val") == -1)
        fail();
    if (array_find(h->names, h->len, "r2_real_val") == -1)
        fail();
    if (array_find(h->names, h->len, "string_val") == -1)
        fail();
    if (array_find(h->names, h->len, "r2_string_val") == -1)
        fail();
    if (array_find(h->names, h->len, "int_val") == -1)
        fail();

    mem_free(h1);
    mem_free(h2);
    mem_free(h);
}

static void test_project()
{
    char *n[] = {"string_val", "int_val", "real_val", "id"};
    char *nprj[] = {"real_val", "id"};
    Type t[] = {String, Int, Real, Int};

    Head *h = head_new(n, t, 4);

    Head *res = head_project(h, nprj, 2);
    if (res->len != 2)
        fail();

    int a1, a2;
    Type t1, t2;
    int exists = head_attr(h, "id", &a1, &t1) && head_attr(res, "id", &a2, &t2);
    if (!exists || a1 != 0 || a1 != a2 || t1 != Int || t1 != t2)
        fail();

    exists = head_attr(h, "real_val", &a1, &t1) &&
             head_attr(res, "real_val", &a2, &t2);
    if (!exists || a1 != 2 || a2 != 1 || t1 != Real || t1 != t2)
        fail();

    exists = head_attr(res, "id", &a1, &t1) &&
             head_attr(res, "real_val", &a2, &t2);
    if (!exists || a1 != 0 || t1 != Int || a2 != 1 || t2 != Real)
         fail();

    if (head_find(res, "string_val"))
        fail();
    if (head_find(res, "non_existing"))
        fail();
    if (head_find(res, "int_val"))
        fail();

    mem_free(h);
    mem_free(res);
}

static void test_eq()
{
    char *en[] = {};
    Type et[] = {};

    char *n1[] = {"name", "id", "real_val"};
    Type t1[] = {String, Int, Real};

    char *n2[] = {"id", "real_val", "name"};
    Type t2[] = {Int, Real, String};

    char *n3[] = {"id", "real_val", "name"};
    Type t3[] = {String, Real, String};

    char *n4[] = {"id", "real_val", "name", "one_more"};
    Type t4[] = {Int, Real, String, String};

    Head *eh = head_new(en, et, 0);
    Head *h1 = head_new(n1, t1, 3);
    Head *h2 = head_new(n2, t2, 3);
    Head *h3 = head_new(n3, t3, 3);
    Head *h4 = head_new(n4, t4, 4);

    if (!equal(eh, eh))
        fail();

    if (equal(eh, h1))
        fail();

    if (!equal(h1, h2))
        fail();

    if (equal(h2, h3))
        fail();

    if (equal(h2, h4))
        fail();

    if (!equal(h4, h4))
        fail();

    mem_free(eh);
    mem_free(h1);
    mem_free(h2);
    mem_free(h3);
    mem_free(h4);
}

static void check_rename(char *old_names[],
                         char *new_names[],
                         Type types[],
                         int len)
{
    Head *old_h = head_new(old_names, types, len);
    Head *old_cpy = head_cpy(old_h);

    int pos[len], plen;
    Head *new_h = head_rename(old_h, old_names, new_names, len, pos, &plen);
    Head *n = head_new(new_names, types, len);

    if (!equal(old_h, old_cpy))
        fail();

    if (!equal(new_h, n))
        fail();

    mem_free(old_cpy);
    mem_free(old_h);
    mem_free(new_h);
    mem_free(n);
}

static void test_rename()
{
    char *n1[] = {"name", "id", "real_val"};
    char *n1_rename[] = {"new_name", "new_id", "new_real_val"};
    Type t1[] = {String, Int, Real};

    char *n2[] = {"id", "real_val", "name", "one_more"};
    char *n2_rename[] = {"name", "real_val", "one_more", "id"};
    Type t2[] = {Int, Real, String, String};

    check_rename(n1, n1_rename, t1, 3);
    check_rename(n2, n2_rename, t2, 4);
}

static void check_cpy(char *names[], Type types[], int len)
{
    Head *src = head_new(names, types, len);
    Head *dst1 = head_cpy(src);
    Head *dst2 = head_cpy(src);

    mem_free(src);

    if (!head_eq(dst1, dst2))
        fail();

    mem_free(dst1);
    mem_free(dst2);
}

static void test_cpy()
{
    char *en[] = {};
    Type et[] = {};

    char *n1[] = {"name", "id", "real_val"};
    Type t1[] = {String, Int, Real};

    char *n2[] = {"id", "real_val", "name"};
    Type t2[] = {Int, Real, String};

    char *n3[] = {"id", "real_val", "name"};
    Type t3[] = {String, Real, String};

    char *n4[] = {"id", "real_val", "name", "one_more"};
    Type t4[] = {Int, Real, String, String};

    check_cpy(en, et, 0);
    check_cpy(n1, t1, 3);
    check_cpy(n2, t2, 3);
    check_cpy(n3, t3, 3);
    check_cpy(n4, t4, 4);
}

static void test_common()
{
    char *n1[] = {"name", "id", "real_val"};
    Type t1[] = {String, Int, Real};

    char *n2[] = {"id", "real_val", "name"};
    Type t2[] = {Int, Real, String};

    char *n3[] = {"id3", "real_val3", "name3"};
    Type t3[] = {Int, Real, String};

    char *n4[] = {"a", "id", "real_val", "name"};
    Type t4[] = {Int, Int, String, Real};

    Head *h1 = head_new(n1, t1, 3);
    Head *h2 = head_new(n2, t2, 3);
    Head *h3 = head_new(n3, t3, 3);
    Head *h4 = head_new(n4, t4, 4);

    int lpos[MAX_ATTRS], rpos[MAX_ATTRS];
    if (3 != head_common(h1, h2, lpos, rpos))
        fail();

    if (lpos[0] != 0 || lpos[1] != 1 || lpos[2] != 2 ||
        rpos[0] != 0 || rpos[1] != 1 || rpos[2] != 2)
        fail();
            
    if (0 != head_common(h1, h3, lpos, rpos))
        fail();

    if (1 != head_common(h1, h4, lpos, rpos))
        fail();

    if (lpos[0] != 0 || rpos[0] != 1)
        fail();

    mem_free(h1);
    mem_free(h2);
    mem_free(h3);
    mem_free(h4);
}

int main(void)
{
    test_rename();
    test_eq();
    test_cpy();
    test_join();
    test_project();
    test_common();

    return 0;
}
