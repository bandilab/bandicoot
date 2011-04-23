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

static Http_Req* parse(const char *test)
{
    char path[MAX_FILE_PATH];
    str_print(path, "test/reqs/%s", test);
    IO *io = sys_open(path, READ | WRITE);

    Http_Req *req = http_parse(io);

    sys_close(io);

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

    req = parse("post_gt_8192");
    if (req == NULL)
        fail();
    if (req->method != POST)
        fail();
    if (str_cmp(req->path, "/store_storage_r1") != 0)
        fail();

    Head *h = NULL;
    TBuf *b = rel_pack_sep(req->body, &h);
    if (b == NULL || h == NULL)
        fail();

    tbuf_clean(b);
    tbuf_free(b);
    mem_free(h);
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

    int p = 0;
    sys_init(0);
    IO *bad_io = sys_socket(&p);

    if (http_200(bad_io) != -200)
        fail();
    if (http_404(bad_io) != -404)
        fail();
    if (http_405(bad_io) != -405)
        fail();
    if (http_500(bad_io) != -500)
        fail();
    if (http_chunk(bad_io, NULL, 0) != -200)
        fail();
    if (http_opts(bad_io) != -200)
        fail();

    sys_close(bad_io);

    return 0;
}
