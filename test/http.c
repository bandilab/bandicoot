/*
Copyright 2008-2012 Ostap Cherkashin
Copyright 2008-2012 Julius Chrobak

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

    Http_Req *req = http_parse_req(io);

    sys_close(io);

    return req;
}

#define FAIL(t) do { if (parse(t) != NULL) fail(); } while (0)
#define OK(t) \
    do { \
        Http_Req *r = parse(t); \
        if (r == NULL) { fail(); } \
        else { http_free_req(r); } \
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
    Http_Req *req = http_parse_req(io);
    sys_close(io);

    int res = 0;
    if (req != NULL) {
        res = req->args->len == cnt;
        http_free_req(req);
    }

    return res;
}

void test_req()
{
    OK("get_ok_1");
    OK("get_ok_2");
    OK("post_clen_last");
    OK("post_clen_first");
    OK("post_with_args");
    OK("post_chunked_ok_1");
    OK("post_chunked_ok_2");

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
    if (req->len != 2)
        fail();
    if (str_cmp(req->body, "12") != 0)
        fail();
    http_free_req(req);

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
    http_free_req(req);

    req = parse("post_gt_8192");
    if (req == NULL)
        fail();
    if (req->method != POST)
        fail();
    if (str_cmp(req->path, "/store_storage_r1") != 0)
        fail();

    Head *h = NULL;
    TBuf *b = NULL;
    Error *err = pack_csv2rel(req->body, &h, &b);
    if (err != NULL || b == NULL || h == NULL)
        fail();

    tbuf_clean(b);
    tbuf_free(b);
    mem_free(h);
    http_free_req(req);

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
}

char *_http_200(IO *io, int *status)
{
    *status = http_200(io);
    http_chunk(io, NULL, 0);
    return "";
}

char *_http_400(IO *io, int *status)
{
    *status = http_400(io);
    return "";
}

char *_http_404(IO *io, int *status)
{
    char *response = "error response body";
    *status = http_404(io, response);
    return response;
}

char *_http_405(IO *io, int *status)
{
    *status = http_405(io, GET);
    return "";
}

char *_http_500(IO *io, int *status)
{
    *status = http_405(io, GET);
    return "";
}

void ok_resp(char* (*response)(IO *io, int *status))
{
    IO *io = sys_open("bin/tmp_file1", CREATE | WRITE);
    int status = 0;
    char *body = response(io, &status);
    sys_close(io);

    io = sys_open("bin/tmp_file1", READ);
    Http_Resp *resp = http_parse_resp(io);
    if (resp == NULL || resp->status != status)
        fail();
    if (resp->body == NULL)
        fail();
    if (str_cmp(body, resp->body) != 0)
        fail();
    http_free_resp(resp);
    sys_close(io);

    sys_remove("bin/tmp_file1");
}

void fail_resp(char *file)
{
    IO *io = sys_open(file, READ);
    Http_Resp *resp = http_parse_resp(io);
    if (resp != NULL)
        fail();
    sys_close(io);
}

void test_resp()
{
    ok_resp(_http_200);
    ok_resp(_http_400);
    ok_resp(_http_404);
    ok_resp(_http_405);
    ok_resp(_http_500);

    fail_resp("test/resp/bad_version");
    fail_resp("test/resp/bad_status_code");
    fail_resp("test/resp/malformed_1");
    fail_resp("test/resp/malformed_2");

    char *b = mem_alloc(MAX_BLOCK + 1);
    for (int i = 0; i < MAX_BLOCK; ++i)
        b[i] = 'a';
    b[MAX_BLOCK] = '\0';

    IO *io = sys_open("bin/200_ok_1", CREATE | WRITE);
    http_200(io);
    http_chunk(io, "Hello World!", 12);
    http_chunk(io, b, MAX_BLOCK);
    http_chunk(io, "The End.", 8);
    http_chunk(io, NULL, 0);
    sys_close(io);

    mem_free(b);

    io = sys_open("bin/200_ok_1", READ);
    Http_Resp *resp = http_parse_resp(io);
    if (resp == NULL || resp->len != 12 + 8 + MAX_BLOCK ||
        resp->len != str_len(resp->body))
        fail();
    http_free_resp(resp);
    sys_close(io);
}

static char *fname = "bin/tmp_file";

static void _serve(char method, char *fn, char *data, Http_Args *args)
{
    IO *io = sys_open(fname, READ);
    Http_Req *req = http_parse_req(io);
    if (req == NULL)
        fail();
    if (req->method != method)
        fail();
    if (str_idx(req->path, fn) != 0)
        fail();
    if (method == POST && str_cmp(req->body, data) != 0)
        fail();
    if (method == GET && str_len(req->body) != 0)
        fail();
    for (int i = 0; i < args->len; ++i) {
        Http_Args *rargs = req->args;
        if (args->len != rargs->len)
            fail();
        if (str_cmp(args->names[i], rargs->names[i]) != 0 ||
            str_cmp(args->vals[i], rargs->vals[i]) != 0)
            fail();
    }
    sys_close(io);

    io = sys_open(fname, TRUNCATE | CREATE | WRITE);
    if (http_200(io) != 200)
        fail();
    if (http_chunk(io, data, str_len(data)) != 200)
        fail();
    if (http_chunk(io, NULL, 0) != 200)
        fail();
    sys_close(io);

    io = sys_open(fname, READ);
    Http_Resp *resp = http_parse_resp(io);
    if (resp == NULL)
        fail();
    if (resp->status != 200)
        fail();
    if (str_cmp(resp->reason, "OK") != 0)
        fail();
    if (str_cmp(resp->body, data) != 0)
        fail();
    sys_close(io);

    http_free_req(req);
    http_free_resp(resp);
}

static void add(Http_Args *args, char *n, char *v)
{
    args->names[args->len] = str_dup(n);
    args->vals[(args->len)++] = str_dup(v);
}

static void _post(char *fn, char *body, Http_Args *args, int res)
{
    int p = (res < 0) ? -1 * res : res;

    IO *io = sys_open(fname, TRUNCATE | CREATE | WRITE);
    if (http_post(io, fn, args) != res)
        fail();
    if (http_chunk(io, body, str_len(body)) != p)
        fail();
    if (http_chunk(io, NULL, 0) != p)
        fail();
    sys_close(io);
}

static void test_post()
{
    Http_Args *args = mem_alloc(sizeof(Http_Args));
    args->len = 0;
    char *fn = "/test_fn", *data = "Hello World!";

    _post(fn, data, args, 200);
    _serve(POST, fn, data, args);

    add(args, "a", "+*%&/(>8");
    _post(fn, data, args, 200);
    _serve(POST, fn, data, args);

    add(args, "b", "value with a space");
    _post(fn, data, args, 200);
    _serve(POST, fn, data, args);

    add(args, "invalid_character", "ç");
    _post(fn, data, args, -200);

    http_free_args(args);
}

static void _get(char *fn, Http_Args *args, int res)
{
    IO *io = sys_open(fname, TRUNCATE | CREATE | WRITE);
    if (http_get(io, fn, args) != res)
        fail();
    sys_close(io);
}

static void test_get()
{
    Http_Args *args = mem_alloc(sizeof(Http_Args));
    args->len = 0;
    char *fn = "/test_fn", *data = "Hello World!";

    _get(fn, args, 200);
    _serve(GET, fn, data, args);

    add(args, "a", "+*%&/(>8");
    _get(fn, args, 200);
    _serve(GET, fn, data, args);

    add(args, "b", "value with a space");
    _get(fn, args, 200);
    _serve(GET, fn, data, args);

    add(args, "invalid_character", "ç");
    _get(fn, args, -200);

    http_free_args(args);
}

int main()
{
    test_req();
    test_resp();

    test_post();
    test_get();

    return 0;
}
