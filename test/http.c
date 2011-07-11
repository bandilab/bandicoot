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
        else { http_free(r); } \
    } while (0)


static int long_header_test(int cnt)
{
    /* long header test */
    int i = 0;
    char *p = mem_alloc(4 + MAX_STRING);
    p[i++] = 'a';
    p[i++] = '=';
    for (; i < MAX_STRING + 2; ++i)
        p[i] = 'x';
    p[i++] = '&';
    p[i++] = '\0';

    int idx = 0, len = str_len(p);
    char *params = mem_alloc(cnt * len + 1);
    for (int i = 0; i < cnt; ++i)
        idx += str_cpy(params + idx, p);
    params[idx - 1] = '\0'; /* -1 to remove the last & sign */

    char *template = sys_load("test/reqs/long_header_template");
    char *all = mem_alloc(str_len(template) + str_len(params));
    str_print(all, template, params);

    char *path = "bin/long_header";
    IO *io = sys_open(path, CREATE | TRUNCATE | WRITE);
    sys_write(io, all, str_len(all));
    sys_close(io);

    mem_free(p);
    mem_free(params);
    mem_free(template);
    mem_free(all);

    io = sys_open(path, READ | WRITE);
    Http_Req *req = http_parse(io);
    sys_close(io);

    int res = 0;
    if (req != NULL) {
        res = req->args->len == cnt;
        http_free(req);
    }

    return res;
}

int main()
{
    OK("get_ok_1");
    OK("get_ok_2");
    OK("post_clen_last");
    OK("post_clen_first");
    OK("post_with_args");

    FAIL("malformed_1");
    FAIL("malformed_2");
    FAIL("malformed_3");
    FAIL("malformed_4");
    FAIL("bad_method");
    FAIL("bad_version");
    FAIL("post_no_clen_header");
    FAIL("post_no_clen_value");
    FAIL("post_clen_value_is_nan");
    FAIL("post_body_lt_clen");

    /* TODO: implement the following tests
    OK("post_body_gt_clen");
    OK("many_reqs_in_one");
    */

    if (!long_header_test(16))
        fail();
    if (!long_header_test(32))
        fail();

    if (!long_header_test(MAX_ATTRS))
        fail();

    if (long_header_test(MAX_ATTRS + 1))
        fail();

    Http_Req *req = parse("post_ok_1");
    if (req == NULL)
        fail();
    if (req->method != POST)
        fail();
    if (str_cmp(req->path, "/some_form") != 0)
        fail();
    if (str_cmp(req->body, "12") != 0)
        fail();
    http_free(req);

    req = parse("get_ok_2");
    if (req == NULL)
        fail();
    if (req->method != GET)
        fail();
    if (str_cmp(req->path, "/load_one_r1"))
        fail();
    if (req->args->len != 3)
        fail();
    if (str_cmp(req->args->names[0], "a") == 0 &&
        str_cmp(req->args->vals[0], "abc") != 0) {
        fail();
    } else if (str_cmp(req->args->names[1], "b") == 0 &&
               str_cmp(req->args->vals[1], "1.1") != 0) {
        fail();
    } else if (str_cmp(req->args->names[2], "c") == 0 &&
               str_cmp(req->args->vals[2], " a") != 0) {
        fail();
    }
    http_free(req);

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
    http_free(req);

    int p = 0;
    sys_init(0);
    IO *bad_io = sys_socket(&p);

    if (http_200(bad_io) != -200)
        fail();
    if (http_404(bad_io, NULL) != -404)
        fail();
    if (http_405(bad_io, GET) != -405)
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
