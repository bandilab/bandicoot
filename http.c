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
#include "http.h"

static const char *HTTP_400 =
    "HTTP/1.1 400 Bad Request\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 0\r\n\r\n";

static const char *HTTP_404 =
    "HTTP/1.1 404 Not Found\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 0\r\n\r\n";

static const char *HTTP_405 =
    "HTTP/1.1 405 Method Not Allowed\r\n"
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

extern Http_Req *http_parse(IO *io)
{
    Http_Req *req = NULL;
    char *buf, *head, *line, *p, path[MAX_NAME], method[MAX_NAME];
    int read = 0, body_start = 0, head_off = 0, off = 0;

    /* FIXME: we should read until '\n' or until the buffer is exhausted */
    buf = mem_alloc(8192);
    read = sys_read(io, buf, 8191);
    if (read < 0)
        goto exit;

    buf[read] = '\0';

    if ((head = next(buf, "\r\n\r\n", &body_start)) == NULL)
        goto exit;

    if ((line = next(head, "\r\n", &head_off)) == NULL)
        goto exit;

    if ((p = next(line, " ", &off)) == NULL)
        goto exit;

    str_cpy(method, p);

    if ((p = next(line, " ", &off)) == NULL)
        goto exit;

    str_cpy(path, p);

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
    str_cpy(req->path, path);
    if (size > 0)
        mem_cpy(req->body, buf + body_start, size);
    req->body[size] = '\0';

exit:
    mem_free(buf);

    return req;
}

extern int http_200(IO *io)
{
    static int size;
    if (size == 0)
        size = str_len(HTTP_200);

    return sys_write(io, HTTP_200, size) < 0 ? -200 : 200;
}

extern int http_400(IO *io)
{
    static int size;
    if (size == 0)
        size = str_len(HTTP_400);

    return sys_write(io, HTTP_400, size) < 0 ? -400 : 400;
}

extern int http_404(IO *io)
{
    static int size;
    if (size == 0)
        size = str_len(HTTP_404);

    return sys_write(io, HTTP_404, size) < 0 ? -404 : 404;
}

extern int http_405(IO *io)
{
    static int size;
    if (size == 0)
        size = str_len(HTTP_405);

    return sys_write(io, HTTP_405, size) < 0 ? -405 : 405;
}

extern int http_500(IO *io)
{
    static int size;
    if (size == 0)
        size = str_len(HTTP_500);

    return sys_write(io, HTTP_500, size) < 0 ? -500 : 500;
}

extern int http_opts(IO *io)
{
    static int size;
    if (size == 0)
        size = str_len(HTTP_OPTS);

    return sys_write(io, HTTP_OPTS, size) < 0 ? -200 : 200;
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
