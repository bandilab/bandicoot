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

#include "config.h"
#include "system.h"
#include "string.h"
#include "memory.h"
#include "array.h"
#include "http.h"

static const char *HTTP_400 =
    "HTTP/1.1 400 Bad Request\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 0\r\n\r\n";

static const char *HTTP_404 =
    "HTTP/1.1 404 Not Found\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: %d\r\n\r\n"
    "%s\r\n";

static const char *HTTP_405 =
    "HTTP/1.1 405 Method Not Allowed\r\n"
    "Allow: %s\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 0\r\n\r\n";

static const char *HTTP_500 =
    "HTTP/1.1 500 Internal Server Error\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 0\r\n\r\n";

static const char *HTTP_OPTS =
    "HTTP/1.1 200 OK\r\n"
    "Access-Control-Allow-Origin: *\r\n"
    "Access-Control-Allow-Methods: OPTIONS, GET, POST\r\n"
    "Access-Control-Allow-Headers: Content-Type, Content-Length\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 0\r\n\r\n";

static const char *HTTP_200 =
    "HTTP/1.1 200 OK\r\n"
    "Access-Control-Allow-Origin: *\r\n"
    "Access-Control-Allow-Methods: OPTIONS, GET, POST\r\n"
    "Access-Control-Allow-Headers: Content-Type, Content-Length\r\n"
    "Content-Type: text/plain\r\n"
    "Transfer-Encoding: chunked\r\n\r\n";

/* TODO: support HEAD method (e.g. to check if a func takes input params) */
/* TODO: support absolute URIs */
/* TODO: support transfer chunked encoding as input */

static char *next(char *buf, const char *sep, int *off)
{
    int len = str_len(sep), idx = str_idx(buf + *off, sep);
    if (idx < 0)
        return NULL;

    buf[*off + idx] = '\0';

    char *res = buf + *off;
    *off += idx + len;

    return str_trim(res);
}

static Http_Args *args_parse(char *buf)
{
    Http_Args *res = mem_alloc(sizeof(Http_Args));
    res->len = 0;

    if (buf == NULL)
        return res;

    int off = 0, len = str_len(buf);
    char *b = mem_alloc(len + 2), *p, *key, *val;
    str_cpy(b, buf);
    b[len++] = '&';
    b[len] = '\0';

    while ((p = next(b, "&", &off)) != NULL) {
        int val_off = 0;
        if ((key = next(p, "=", &val_off)) == NULL)
            goto failure;

        /* TODO: this returns HTTP 400 rather than 404 with error message */
        if (res->len >= MAX_ATTRS)
            goto failure;

        val = str_urldecode(p + val_off);
        if (val == NULL)
            goto failure;

        res->names[res->len] = str_dup(key);
        res->vals[res->len++] = val;
    }

    goto success;

failure:
    for (int i = 0; i < res->len; ++i) {
        mem_free(res->names[i]);
        mem_free(res->vals[i]);
    }
    mem_free(res);
    res = NULL;

success:
    mem_free(b);

    return res;
}

static char *read_header(IO *io, int *size)
{
    int read = 0, alloc = 8192;
    char *res = mem_alloc(alloc), *buf = res;

    *size = 0;
    while ((read = sys_read(io, buf, alloc - 1)) > 0) {
        buf[read] = '\0';
        *size += read;

        if (str_idx(buf, "\r\n\r\n") > -1)
            goto success;

        res = mem_realloc(res, *size + alloc);
        buf = res + *size;
    }

    mem_free(res);
    res = NULL;

success:
    return res;
}

extern Http_Req *http_parse(IO *io)
{
    Http_Req *req = NULL;
    Http_Args *args = NULL;
    char *buf, *head, *line, *p, *q, path[MAX_NAME], method[MAX_NAME];
    int read = 0, body_start = 0, head_off = 0, off = 0;

    buf = read_header(io, &read);
    if (buf == NULL)
        goto exit;

    if ((head = next(buf, "\r\n\r\n", &body_start)) == NULL)
        goto exit;

    if ((line = next(head, "\r\n", &head_off)) == NULL)
        goto exit;

    if ((p = next(line, " ", &off)) == NULL)
        goto exit;

    str_cpy(method, p);

    p = next(line, "?", &off);
    q = next(line, " ", &off);
    if (p != NULL) {
        str_cpy(path, p);

        if (q == NULL)
            goto exit;
    } else {
        if (q == NULL)
            goto exit;
        str_cpy(path, q);
        q = NULL;
    }

    args = args_parse(q);
    if (args == NULL)
        goto exit;

    if (str_cmp(line + off, "HTTP/1.1") != 0)
        goto exit;

    int m, size = 0;
    if (str_cmp(method, "POST") == 0) {
        m = POST;

        if ((p = next(head, "Content-Length:", &head_off)) == NULL)
            goto exit;

        p = next(head, "\r\n", &head_off);
        if (p == NULL) /* content length is the last header */
            p = str_trim(head + head_off);

        int error = 0;
        size = str_int(p, &error);
        if (error || size < 0)
            goto exit;

        int remaining = size - read + body_start;
        if (remaining > 0) {
            buf = mem_realloc(buf, read + remaining);
            if (sys_readn(io, buf + read, remaining) != remaining)
                goto exit;
        }
    } else if (str_cmp(method, "GET") == 0) {
        m = GET;
    } else if (str_cmp(method, "OPTIONS") == 0) {
        m = OPTIONS;
    } else
        goto exit;

    req = mem_alloc(sizeof(Http_Req) + size + 1);
    req->body = (char*) (req + 1);
    req->method = m;
    req->args = args;
    str_cpy(req->path, path);
    if (size > 0)
        mem_cpy(req->body, buf + body_start, size);
    req->body[size] = '\0';

exit:
    mem_free(buf);
    if (req == NULL && args != NULL) {

        for (int i = 0; i < args->len; ++i) {
            mem_free(args->names[i]);
            mem_free(args->vals[i]);
        }

        mem_free(args);
    }

    return req;
}

extern void http_free(Http_Req *req)
{
    for (int i = 0; i < req->args->len; ++i) {
        mem_free(req->args->names[i]);
        mem_free(req->args->vals[i]);
    }

    mem_free(req->args);
    mem_free(req);
}

extern int http_200(IO *io)
{
    return sys_write(io, HTTP_200, str_len(HTTP_200)) < 0 ? -200 : 200;
}

extern int http_400(IO *io)
{
    return sys_write(io, HTTP_400, str_len(HTTP_400)) < 0 ? -400 : 400;
}

extern int http_404(IO *io, const char *response)
{
    const char *res = (response == NULL) ? "": response;
    /* +2 to include trailing \r\n */
    int rsize = str_len(res) + 2;
    int hsize = str_len(HTTP_404);

    char *buf = mem_alloc(hsize + rsize + 10); /* + MAX_INT digits */
    int off = str_print(buf, HTTP_404, rsize, res);

    int ok = sys_write(io, buf, off) == off;

    mem_free(buf);

    return ok ? 404 : -404;
}

extern int http_405(IO *io, const char method)
{
    char *m = "";
    if (method == GET)
        m = "GET";
    if (method == POST)
        m = "POST";

    char *buf = mem_alloc(str_len(HTTP_405) + 4); /* + "POST" length */
    int off = str_print(buf, HTTP_405, m);

    int ok = sys_write(io, buf, off) == off;

    mem_free(buf);

    return ok ? 405 : -405;
}

extern int http_500(IO *io)
{
    return sys_write(io, HTTP_500, str_len(HTTP_500)) < 0 ? -500 : 500;
}

extern int http_opts(IO *io)
{
    return sys_write(io, HTTP_OPTS, str_len(HTTP_OPTS)) < 0 ? -200 : 200;
}

extern int http_chunk(IO *io, const void *buf, int size)
{
    char hex[16];
    int s = str_print(hex, "%X\r\n", size);

    int ok = sys_write(io, hex, s) > 0 &&
             sys_write(io, buf, size) >= 0 &&
             sys_write(io, "\r\n", 2) > 0;

    return ok ? 200 : -200;
}
