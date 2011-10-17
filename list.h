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

struct List {
    void *elem;
    struct List *next;
    struct List *prev;
};
typedef struct List List;

static void list_del(List *e)
{
    if (e->prev != NULL)
        e->prev->next = e->next;
    if (e->next != NULL)
        e->next->prev = e->prev;

    mem_free(e);
}

static List *list_next(List *e)
{
    List *res = e->next;
    list_del(e);

    return res;
}

static List *list_prev(List *e)
{
    List *res = e->prev;
    list_del(e);

    return res;
}

static List *list_prepend(List *dest, void *elem)
{
    List *new = mem_alloc(sizeof(List));
    new->elem = elem;
    new->next = dest;
    new->prev = NULL;

    if (dest != NULL) {
        new->prev = dest->prev;
        if (dest->prev != NULL)
            dest->prev->next = new;

        dest->prev = new;
    }

    return new;
}

/* remove elements from the list using rm function:
   - non-zero result indicates that the passed element should be removed
   - rm is reponsible for freeing the actual elem
   - new list head is passed to rm in case a nested loop is required */
static List *list_rm(List *list,
                     const void *cmp,
                     int (*rm)(List *head, void *elem, const void *cmp))
{
    List *it = list, *res = NULL, *tmp = NULL;
    while (it != NULL) {
        tmp = it->next;
        if (rm(res, it->elem, cmp))
            list_del(it);
        else if (res == NULL)
            res = it;

        it = tmp;
    }

    return res;
}
