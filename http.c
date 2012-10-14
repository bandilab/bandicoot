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

static const char *HTTP_POST =
    "POST %s%s HTTP/1.1\r\n"
    "User-Agent: bandicoot\r\n"
    "Transfer-Encoding: chunked\r\n\r\n";

static const char *HTTP_GET =
    "GET %s%s HTTP/1.1\r\n"
    "User-Agent: bandicoot\r\n\r\n";

static const char *CONTENT_LENGTH = "Content-Length:";
static const char *CHUNKED_ENCODING = "Transfer-Encoding: chunked";

/* TODO: support HEAD method (e.g. to check if a func takes input params) */
/* TODO: support absolute URIs */

typedef struct {
    char *buf;
    int read;
    int size;
} Buf;

static Buf *alloc()
{
    Buf *res = mem_alloc(sizeof(Buf));
    res->size = MAX_BLOCK;
    res->read = 0;
    res->buf = mem_alloc(MAX_BLOCK);

    return res;
}

static void extend(Buf *b, int size)
{
    b->size += size;
    b->buf = mem_realloc(b->buf, b->size);
}

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

static char *args_conv(Http_Args *args)
{
    char *res = NULL, *tmp = str_dup("??");
    int len = 1;

    for (int i = 0; i < args->len; ++i) {
        char *conv = str_urlencode(args->vals[i]);
        if (conv == NULL)
            goto exit;

        tmp = mem_realloc(tmp, len + MAX_NAME + str_len(conv) + 2);
        len += str_print(tmp + len, "%s=%s&", args->names[i], conv);
        mem_free(conv);
    }

    tmp[len - 1] = '\0';
    res = tmp;
    tmp = NULL;

exit:
    if (tmp != NULL)
        mem_free(tmp);

    return res;
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

static int read_header(IO *io, Buf *b)
{
    for (;;) {
        char *buf = b->buf + b->read;
        int free = b->size - b->read;

        int r = sys_read(io, buf, free - 1);
        if (r <= 0)
            return 0;

        buf[r] = '\0';
        b->read += r;

        if (str_idx(buf, "\r\n\r\n") > -1)
            break;

        extend(b, MAX_BLOCK);
    }

    return 1;
}

static int read_remaining(IO *io, Buf *b, int remaining)
{
    if (remaining > 0) {
        extend(b, remaining);
        if (sys_readn(io, b->buf + b->read, remaining) != remaining)
            return 0;
        b->read += remaining;
        b->buf[b->read] = '\0';
    }

    return 1;
}

static char *read_chunks(IO *io, Buf *b, int chunk_start, int *size)
{
    *size = 0;
    char *line = NULL, *chunks = NULL, *res = NULL;
    int chunk_size = 0, error = 0;
    do {
        /* try to read until the \r\n or an error */
        while (str_idx(b->buf + chunk_start, "\r\n") < 0) {
            extend(b, MAX_BLOCK);
            int r =  sys_read(io, b->buf + b->read, MAX_BLOCK);
            if (r < 0)
                goto exit;

            b->read += r;
        }

        if ((line = next(b->buf, "\r\n", &chunk_start)) == NULL)
            goto exit;

        chunk_size = str_hexdecode(line, &error);
        if (error || chunk_size < 0)
            goto exit;

        if (chunk_size > 0 &&
            !read_remaining(io, b, chunk_size - b->read + chunk_start + 2))
                goto exit;

        if (chunks == NULL)
            chunks = mem_alloc(chunk_size + 1);
        else
            chunks = mem_realloc(chunks, *size + chunk_size + 1);
        mem_cpy(chunks + *size, b->buf + chunk_start, chunk_size);
        *size += chunk_size;

        chunk_start += chunk_size;
        if ((line = next(b->buf, "\r\n", &chunk_start)) == NULL)
            goto exit;
    } while (chunk_size > 0);

    res = chunks;
    res[*size] = '\0';

exit:
    if (res == NULL)
        mem_free(chunks);

    return res;
}

static char *read_data(IO *io, Buf *b, int head_off, int body_start, int *size)
{
    *size = 0;

    char *res = NULL;
    if (next(b->buf, CONTENT_LENGTH, &head_off) != NULL) {
        char *p = next(b->buf, "\r\n", &head_off);
        if (p == NULL) /* content length is the last header */
            p = str_trim(b->buf + head_off);

        int error = 0;
        *size = str_int(p, &error);
        if (error || *size < 0)
            goto exit;

        if (!read_remaining(io, b, *size - b->read + body_start))
            goto exit;

        res = mem_alloc(*size + 1);
        mem_cpy(res, b->buf + body_start, *size);
        res[*size] = '\0';
    } else if (next(b->buf, CHUNKED_ENCODING, &head_off) != NULL) {
        res = read_chunks(io, b, body_start, size);
    }

exit:
    return res;
}

extern Http_Req *http_parse_req(IO *io)
{
    Http_Req *req = NULL;
    Http_Args *args = NULL;
    char *data = NULL, *p, *q;
    char path[MAX_STRING], method[MAX_NAME];
    int body_start = 0, head_off = 0;

    Buf *b = alloc();
    if (!read_header(io, b))
        goto exit;

    if (next(b->buf, "\r\n\r\n", &body_start) == NULL)
        goto exit;

    if ((p = next(b->buf, " ", &head_off)) == NULL)
        goto exit;

    str_cpy(method, p);

    p = next(b->buf, "?", &head_off);
    q = next(b->buf, " ", &head_off);
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

    if ((p = next(b->buf, "\r\n", &head_off)) == NULL)
        goto exit;

    if (str_cmp(p, "HTTP/1.1") != 0)
        goto exit;

    int m, size = 0;
    if (str_cmp(method, "POST") == 0) {
        m = POST;
        data = read_data(io, b, head_off, body_start, &size);
    } else if (str_cmp(method, "GET") == 0) {
        m = GET;
        data = str_dup("");
    } else if (str_cmp(method, "OPTIONS") == 0) {
        m = OPTIONS;
        data = str_dup("");
    } else
        goto exit;

    if (data == NULL)
        goto exit;

    req = mem_alloc(sizeof(Http_Req));
    req->method = m;
    req->args = args;
    req->len = size;
    str_cpy(req->path, path);
    req->body = data;
exit:
    mem_free(b->buf);
    mem_free(b);
    if (req == NULL) {
        if (args != NULL) {
            for (int i = 0; i < args->len; ++i) {
                mem_free(args->names[i]);
                mem_free(args->vals[i]);
            }

            mem_free(args);
        }
        if (data != NULL)
            mem_free(data);
    }

    return req;
}

extern Http_Resp *http_parse_resp(IO *io)
{
    Http_Resp *resp = NULL;
    char *data = NULL, *p, *reason = NULL, *rp;
    int body_start = 0, head_off = 0;

    Buf *b = alloc();
    if (!read_header(io, b))
        goto exit;

    if (next(b->buf, "\r\n\r\n", &body_start) == NULL)
        goto exit;

    if ((p = next(b->buf, " ", &head_off)) == NULL)
        goto exit;

    if (str_cmp(p, "HTTP/1.1") != 0)
        goto exit;

    if ((p = next(b->buf, " ", &head_off)) == NULL)
        goto exit;

    int error = 0;
    int status = str_int(p, &error);
    if (error)
        goto exit;

    if ((rp = next(b->buf, "\r\n", &head_off)) == NULL)
        goto exit;

    if (str_len(rp) == 0)
        goto exit;

    reason = str_dup(rp); /* copy required as b->buf gets reallocated */

    int size = 0;
    if (status == 200 || status == 404)
        data = read_data(io, b, head_off, body_start, &size);
    else
        data = str_dup("");

    if (data == NULL)
        goto exit;

    resp = mem_alloc(sizeof(Http_Resp));
    resp->status = status;
    resp->reason = reason;
    resp->body = data;
    resp->len = size;
exit:
    mem_free(b->buf);
    mem_free(b);
    if (resp == NULL) {
        if (reason != NULL)
            mem_free(reason);
        if (data != NULL)
            mem_free(data);
    }

    return resp;
}

extern void http_free_args(Http_Args *args) {
    for (int i = 0; i < args->len; ++i) {
        mem_free(args->names[i]);
        mem_free(args->vals[i]);
    }
    mem_free(args);
}

static int request(const char *req, IO *io, const char *fn, Http_Args *args)
{
    char *a = args_conv(args);
    if (a == NULL)
        return -200;

    char head[str_len(req) + str_len(fn) + str_len(a) + 1];
    int head_size = str_print(head, req, fn, a);
    mem_free(a);

    return sys_write(io, head, head_size) == head_size ? 200 : -200;
}

extern int http_post(IO *io, const char *fn, Http_Args *args)
{
    return request(HTTP_POST, io, fn, args);
}

extern int http_get(IO *io, const char *fn, Http_Args *args)
{
    return request(HTTP_GET, io, fn, args);
}

extern void http_free_req(Http_Req *req)
{
    http_free_args(req->args);
    mem_free(req->body);
    mem_free(req);
}

extern void http_free_resp(Http_Resp *resp)
{
    mem_free(resp->reason);
    mem_free(resp->body);
    mem_free(resp);
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
    int rsize = str_len(res);
    int hsize = str_len(HTTP_404);

    /* +2 to include trailing \r\n  + MAX_INT digits */
    char *buf = mem_alloc(hsize + rsize + 2 + 10);
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
    int sz = s + size + 2;

    void *m = mem_alloc(sz);
    mem_cpy(m, hex, s);
    mem_cpy(m + s, buf, size);
    mem_cpy(m + s + size, "\r\n", 2);

    int ok = sys_write(io, m, sz) == sz;

    mem_free(m);
    return ok ? 200 : -200;
}
