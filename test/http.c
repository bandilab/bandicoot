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

static Http_Req* parse(const char *test)
{
    char path[MAX_PATH];
    str_print(path, "test/reqs/%s", test);
    int fd = sys_open(path, READ | WRITE);

    Http_Req *req = http_parse(fd);

    sys_close(fd);

    return req;
}

#define FAIL(t) do { if (parse(t) != NULL) fail(); } while (0)
#define OK(t) \
    do { \
        Http_Req *r = parse(t); \
        if (r == NULL) { fail(); } \
        else { mem_free(r); } \
    } while (0)

int main()
{
    /* N.B. it is important that the request files are in dos format (\r\n) */
    Http_Req *req = parse("post_ok_1");
    if (req == NULL)
        fail();
    if (req->method != POST)
        fail();
    if (str_cmp(req->path, "/some_form") != 0)
        fail();
    if (str_cmp(req->body, "12") != 0)
        fail();
    mem_free(req);

    FAIL("malformed_1");
    FAIL("malformed_2");
    FAIL("bad_method");
    FAIL("bad_version");
    FAIL("post_no_clen_header");
    FAIL("post_no_clen_value");
    FAIL("post_clen_value_is_nan");
    FAIL("post_body_lt_clen");

    OK("post_clen_last");
    OK("post_clen_first");

    /* TODO: implement the following tests
    OK("post_body_gt_clen");
    OK("many_reqs_in_one");

    headers > 8192
    */

    if (http_200(512) != -200)
        fail();
    if (http_404(512) != -404)
        fail();
    if (http_405(512) != -405)
        fail();
    if (http_500(512) != -500)
        fail();
    if (http_chunk(512, NULL, 0) != -200)
        fail();
    if (http_opts(512) != -200)
        fail();

    return 0;
}
