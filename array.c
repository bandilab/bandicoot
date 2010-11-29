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

#include "string.h"

extern int array_scan(char *keys[], int len, const char *key)
{
    int i;
    for (i = 0; i < len; ++i)
        if (str_cmp(keys[i], key) == 0)
            break;

    return i == len ? -1 : i;
}

extern int array_find(char *keys[], int len, const char *key)
{
    int low = 0, high = len - 1;

    while (low <= high) {
        int mid = (low + high) / 2;

        if (str_cmp(key, keys[mid]) < 0)
            high = mid - 1;
        else if (str_cmp(key, keys[mid]) > 0)
            low = mid + 1;
        else
            return mid;
    }

    return -1;
}

static void swap(char *keys[], int i, int j, int map[])
{
    char *sv = keys[i];
    keys[i] = keys[j];
    keys[j] = sv;

    if (map != 0) {
        int iv = map[i];
        map[i] = map[j];
        map[j] = iv;
    }
}

static void sort(char *keys[], int left, int right, int map[])
{
    if (left >= right)
        return;

    swap(keys, left, (left + right) / 2, map);
    int last = left;
    for (int i = left + 1; i <= right; ++i)
        if (str_cmp(keys[i], keys[left]) < 0)
            swap(keys, ++last, i, map);

    swap(keys, left, last, map);

    sort(keys, left, last - 1, map);
    sort(keys, last + 1, right, map);
}

extern void array_sort(char *keys[], int len, int map[])
{
    for (int i = 0; map != 0 && i < len; ++i)
        map[i] = i;

    sort(keys, 0, len - 1, map);
}
