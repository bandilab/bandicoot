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

static void test_scan()
{
    char *empty_words[] = {};
    char *four_words[] = {"abc", "def", "jhk", "lmn"};

    if (array_scan(empty_words, 0, "does not exist") != -1)
        fail();
    if (array_scan(four_words, 4, "abc") != 0)
        fail();
    if (array_scan(four_words, 4, "jhk") != 2)
        fail();
    if (array_scan(four_words, 4, "lmn") != 3)
        fail();
    if (array_scan(four_words, 4, "does not exist") != -1)
        fail();
}

static void test_find()
{
    char *empty_words[] = {};
    char *three_words[] = {"a", "b", "c"};
    char *four_words[] = {"a", "b", "c", "d"};

    if (array_find(empty_words, 0, "does not exist") != -1)
        fail();

    if (array_find(three_words, 3, "new_id") != -1)
        fail();

    if (array_find(four_words, 4, "a") != 0)
        fail();
    if (array_find(four_words, 4, "b") != 1)
        fail();
    if (array_find(four_words, 4, "c") != 2)
        fail();
    if (array_find(four_words, 4, "d") != 3)
        fail();
    if (array_find(four_words, 4, "does not exist") != -1)
        fail();
}

static void test_sort()
{
    char *empty_words[] = {};
    char *one_word[] = {"one"};
    char *three_words[] = {"just", "a", "test"};
    char *four_words[] = {"aa", "aaa", "", "a"};
    int map[4];

    array_sort(empty_words, 0, map);

    array_sort(one_word, 1, map);
    if (str_cmp(one_word[0], "one") != 0)
        fail();
    if (map[0] != 0)
        fail();

    array_sort(three_words, 3, map);
    if (str_cmp(three_words[0], "a") != 0)
        fail();
    if (str_cmp(three_words[1], "just") != 0)
        fail();
    if (str_cmp(three_words[2], "test") != 0)
        fail();

    /* just:0    a:1 test:2
          a:1 just:0 test:2 */

    if (map[0] != 1 || map[1] != 0 || map[2] != 2)
        fail();

    array_sort(four_words, 4, map);
    if (str_cmp(four_words[0], "") != 0)
        fail();
    if (str_cmp(four_words[1], "a") != 0)
        fail();
    if (str_cmp(four_words[2], "aa") != 0)
        fail();
    if (str_cmp(four_words[3], "aaa") != 0)
        fail();

    /* aa:0 aaa:1  '':2   a:3
       '':2   a:3  aa:0 aaa:1 */

    if (map[0] != 2 || map[1] != 3 || map[2] != 0 || map[3] != 1)
        fail();
}

int main()
{
    test_scan();
    test_find();
    test_sort();

    return 0;
}
