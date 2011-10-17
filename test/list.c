/*
Copyright 2008-2011 Ostap Cherkashin
Copyright 2008-2011 Julius Chrobak

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

static int rm_all(List *head, void *elem, const void *cmp)
{
    return 1;
}

static int rm_none(List *head, void *elem, const void *cmp)
{
    return 0;
}

static int rm_given(List *head, void *elem, const void *cmp)
{
    return str_cmp(elem, cmp) == 0;
}

int main(void)
{
    List *l1 = list_prepend(NULL, "one");
    if (l1 == NULL ||
        l1->next != NULL ||
        l1->prev != NULL ||
        str_cmp(l1->elem, "one") != 0)
        fail();

    List *l2 = list_prepend(l1, "two");
    if (l1->prev != l2)
        fail();

    if (l2 == NULL ||
        l2->next != l1 ||
        l2->prev != NULL ||
        str_cmp(l2->elem, "two") != 0)
        fail();

    List *l3 = list_prepend(l2, "three");
    if (l2->prev != l3)
        fail();

    if (l3 == NULL ||
        l3->next != l2 ||
        l3->prev != NULL ||
        str_cmp(l3->elem, "three") != 0)
        fail();

    List *x = list_next(l2);
    if (x != l1 ||
        x->next != NULL ||
        x->prev != l3 ||
        str_cmp(x->elem, "one") != 0)
        fail();

    l2 = list_prepend(x, "++two++");
    if (l2 == NULL ||
        l2->prev != l3 ||
        l2->prev->next != l2 ||
        l2->next != l1 ||
        l2->next->prev != l2 ||
        str_cmp(l2->elem, "++two++"))
        fail();

    List *y = list_prev(l2);
    if (y != l3 ||
        y->next != l1 ||
        y->next->prev != l3 ||
        y->prev != NULL ||
        str_cmp(y->elem, "three"))
        fail();

    l2 = list_prepend(x, "--two--");
    if (l2 == NULL ||
        l2->prev != l3 ||
        l2->prev->next != l2 ||
        l2->next != l1 ||
        l2->next->prev != l2 ||
        str_cmp(l2->elem, "--two--"))
        fail();

    List *h1 = list_rm(l3, "--two--", rm_given);
    if (h1 != l3 ||
        h1->next != l1 ||
        h1->next->prev != l3 ||
        h1->prev != NULL ||
        str_cmp(h1->elem, "three") != 0 ||
        str_cmp(h1->next->elem, "one") != 0)
        fail();

    List *h2 = list_rm(l3, NULL, rm_none);
    if (h1 != l3 ||
        h1->next != l1 ||
        h1->next->prev != l3 ||
        h1->prev != NULL ||
        str_cmp(h1->elem, "three") != 0 ||
        str_cmp(h1->next->elem, "one") != 0)
        fail();

    List *h3 = list_rm(l3, NULL, rm_all);
    if (h3 != NULL)
        fail();

    return 0;
}
