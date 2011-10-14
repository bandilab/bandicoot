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
};
typedef struct List List;

static List *list_next(List *e)
{
    List *res = e->next;
    mem_free(e);
    return res;
}

static List *list_add(List *dest, void *elem)
{
    List *new = mem_alloc(sizeof(List));
    new->elem = elem;
    new->next = dest;

    return new;
}

/* rm_elem method is used to:
   - identify elems to be removed
   - must mem_free the whole elem

   elem is current elem from the list to be removed
   cmp which can be used to compare the elem to */
static void list_rm(List **list,
                    const void *cmp,
                    int (*rm_elem)(void *elem, const void *cmp))
{
    List *it = *list, **prev = list;
    while (it) {
        int rm = rm_elem(it->elem, cmp);
        if (rm) {
            *prev = it->next;

            mem_free(it);

            it = *list;
            prev = list;
        } else {
            prev = &it->next;
            it = it->next;
        }
    }
}
