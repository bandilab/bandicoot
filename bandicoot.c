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

#include "config.h"
#include "system.h"
#include "memory.h"
#include "string.h"
#include "head.h"
#include "http.h"
#include "value.h"
#include "tuple.h"
#include "volume.h"
#include "expression.h"
#include "summary.h"
#include "relation.h"
#include "transaction.h"
#include "environment.h"
#include "pack.h"

extern const char *VERSION;

#define QUEUE_LEN 128
#define THREADS 8
#define PROC_WAIT_SEC 5

typedef struct {
    char exe[MAX_FILE_PATH];
    char tx[MAX_ADDR];
    Env *env;
} Exec;

struct {
    int pos;
    int len;
    IO *ios[QUEUE_LEN];
    Mon *mon;
} queue;

static void queue_init()
{
    queue.pos = -1;
    queue.len = QUEUE_LEN;
    queue.mon = mon_new();
}

static void queue_put(IO *io)
{
    mon_lock(queue.mon);
    while (queue.pos >= queue.len)
        mon_wait(queue.mon);

    queue.pos++;
    queue.ios[queue.pos] = io;

    mon_signal(queue.mon);
    mon_unlock(queue.mon);
}

static IO *queue_get()
{
    mon_lock(queue.mon);
    while (queue.pos < 0)
        mon_wait(queue.mon);

    IO *io = queue.ios[queue.pos];
    queue.pos--;

    mon_signal(queue.mon);
    mon_unlock(queue.mon);

    return io;
}

static void *exec_thread(void *arg)
{
    Exec *x = arg;
    int p = 0;
    IO *sio = sys_socket(&p);
    char port[8];
    str_print(port, "%d", p);
    char *argv[] = {x->exe, "processor", "-p", port, "-t", x->tx, NULL};

    for (;;) {
        int status = -1, pid = -1;
        IO *cio = queue_get();
        IO *pio = NULL;
        long sid = 0, time = sys_millis();
        TBuf *param = NULL, *ret = NULL;

        Http_Req *req = http_parse(cio);
        if (req == NULL) {
            status = http_400(cio);
            goto exit;
        }

        if (req->method == OPTIONS) {
            status = http_opts(cio);
            goto exit;
        }

        /* TODO: all the checks below could move into the processor */

        char func[MAX_NAME];
        str_cpy(func, req->path + 1);

        Func *fn = env_func(x->env, func);
        if (fn == NULL) {
            status = http_404(cio);
            goto exit;
        }

        if ((fn->p.len == 1 && req->method != POST) ||
            (fn->p.len == 0 && req->method == POST))
        {
            status = http_405(cio);
            goto exit;
        } if (fn->p.len == 1) {
            Head *head = NULL;
            param = rel_pack_sep(req->body, &head);

            int eq = 0;
            if (head != NULL) {
                eq = head_eq(head, fn->p.rels[0]->head);
                mem_free(head);
            }

            if (param == NULL || !eq) {
                status = http_400(cio);
                goto exit;
            }
        }

        pid = sys_exec(argv);
        if (!sys_iready(sio, PROC_WAIT_SEC)) {
            status = http_500(cio);
            goto exit;
        }

        /* FIXME: sys_accept might fail (and cause sys_die). */
        pio = sys_accept(sio);

        if (sys_write(pio, func, MAX_NAME) < 0) {
            status = http_500(cio);
            goto exit;
        }

        if (fn->p.len == 1) {
            if (tbuf_write(param, pio) < 0) {
                status = http_500(cio);
                goto exit;
            }
            tbuf_free(param);
            param = NULL;
        }

        if (sys_readn(pio, &sid, sizeof(long)) != sizeof(long))
            goto exit;

        if (fn->ret != NULL) {
            ret = tbuf_read(pio);
            if (ret == NULL) {
                status = http_500(cio);
                goto exit;
            }
        }

        int failed = -1;
        if (sys_readn(pio, &failed, sizeof(int)) != sizeof(int) || failed) {
            status = http_500(cio);
            goto exit;
        }

        status = http_200(cio);
        if (ret != NULL) {
            int size;
            char *res = rel_unpack(fn->ret->head, ret, &size);
            status = http_chunk(cio, res, size);

            mem_free(res);
            tbuf_free(ret);
            ret = NULL;
        }

        status = http_chunk(cio, NULL, 0);

exit:
        sys_log('E', "%016X method %c, path %s, time %dms - %3d\n",
                     sid,
                     (req == NULL) ? '?' : req->method,
                     (req == NULL) ? "malformed" : req->path,
                     sys_millis() - time,
                     status);

        if (ret != NULL) {
            tbuf_clean(ret);
            tbuf_free(ret);
        }
        if (param != NULL) {
            tbuf_clean(param);
            tbuf_free(param);
        }
        if (req != NULL)
            mem_free(req);
        if (pid != -1)
            sys_wait(pid);
        if (pio != NULL)
            sys_close(pio);
        sys_close(cio);
    }

    sys_close(sio);
    env_free(x->env);
    mem_free(x);
    mem_free(arg);

    return NULL;
}

static void usage(char *p)
{
    sys_print("usage: %s <command> <args>\n\n", p);
    sys_print("standalone commands:\n");
    sys_print("  start -p <port> -d <data.dir> -c <source.file>"
              " -s <state.file>\n\n");
    sys_print("distributed commands:\n");
    sys_print("  tx    -p <port> -c <source.file> -s <state.file>\n");
    sys_print("  vol   -p <port> -d <data.dir> -t <tx.host:port>\n");
    sys_print("  exec  -p <port> -t <tx.host:port>\n\n");

    sys_die(VERSION);
}

static int parse_port(char *p)
{
    int port = 0, e = -1;
    port = str_int(p, &e);
    if (e || port < 1 || port > 65535)
        sys_die("invalid port '%s'\n", p);

    return port;
}

static void multiplex(const char *exe, const char *tx_addr, int port)
{
    queue_init();

    char *code = tx_program();
    for (int i = 0; i < THREADS; ++i) {
        Exec *e = mem_alloc(sizeof(Exec));
        e->env = env_new("net", code);
        str_cpy(e->exe, exe);
        str_cpy(e->tx, tx_addr);

        sys_thread(exec_thread, e);
    }
    mem_free(code);

    IO *sio = sys_socket(&port);

    sys_log('E', "started port=%d, tx=%s\n", port, tx_addr);

    for (;;) {
        IO *cio = sys_accept(sio);
        queue_put(cio);
    }

    mon_free(queue.mon);
}

int main(int argc, char *argv[])
{
    int port = 0;
    char *data = NULL;
    char *state = NULL;
    char *source = NULL;
    char *tx_addr = NULL;

    sys_init(1);
    if (argc < 2)
        usage(argv[0]);

    for (int i = 2; (i + 1) < argc; i += 2)
        if (str_cmp(argv[i], "-d") == 0)
            data = argv[i + 1];
        else if (str_cmp(argv[i], "-c") == 0)
            source = argv[i + 1];
        else if (str_cmp(argv[i], "-s") == 0)
            state = argv[i + 1];
        else if (str_cmp(argv[i], "-p") == 0)
            port = parse_port(argv[i + 1]);
        else if (str_cmp(argv[i], "-t") == 0) {
            tx_addr = argv[i + 1];
            if (str_len(tx_addr) >= MAX_ADDR)
                sys_die("tx address exceeds the maximum length\n");
        } else
            usage(argv[0]);

    if (str_cmp(argv[1], "start") == 0 && source != NULL && data != NULL &&
        state != NULL && port != 0 && tx_addr == NULL)
    {
        int tx_port = 0;
        tx_server(source, state, &tx_port);
        vol_init(0, data);

        char addr[MAX_ADDR];
        str_print(addr, "127.0.0.1:%d", tx_port);
        multiplex(argv[0], addr, port);

        tx_free();
    } else if (str_cmp(argv[1], "processor") == 0 && source == NULL &&
               data == NULL && state == NULL && port != 0 && tx_addr != NULL)
    {
        tx_attach(tx_addr);
        char *code = tx_program();
        Env *env = env_new("net", code);
        mem_free(code);

        char addr[MAX_ADDR];
        str_print(addr, "127.0.0.1:%d", port);
        IO *io = sys_connect(addr);

        Func *fn = NULL;
        char func[MAX_NAME];
        if (sys_readn(io, func, MAX_NAME) != MAX_NAME ||
            (fn = env_func(env, func)) == NULL)
            sys_die("processor: failed to retrieve a function name\n");

        TBuf *arg = NULL;
        if (fn->p.len == 1)
            if ((arg = tbuf_read(io)) == NULL)
                sys_die("processor:%s: failed to retrieve parameters\n", func);

        Vars *r = vars_new(fn->r.len);
        Vars *w = vars_new(fn->w.len);
        for (int i = 0; i < fn->r.len; ++i)
            vars_put(r, fn->r.vars[i], 0L);
        for (int i = 0; i < fn->w.len; ++i)
            vars_put(w, fn->w.vars[i], 0L);

        long sid = tx_enter(r, w);

        if (sys_write(io, &sid, sizeof(long)) < 0)
            sys_die("%s: failed to transmit\n", func);

        for (int i = 0; i < fn->p.len; ++i)
            rel_init(fn->p.rels[i], r, arg);

        for (int i = 0; i < fn->t.len; ++i)
            rel_init(fn->t.rels[i], r, arg);

        for (int i = 0; i < fn->w.len; ++i) {
            rel_init(fn->w.rels[i], r, arg);
            rel_store(w->vols[i], w->vars[i], w->vers[i], fn->w.rels[i]);
        }

        if (fn->ret != NULL) {
            rel_init(fn->ret, r, arg);
            if (tbuf_write(fn->ret->body, io) < 0)
                sys_die("%s: failed to transmit the result\n", func);
        }

        tx_commit(sid);

        int failed = 0;
        if (sys_write(io, &failed, sizeof(int)) < 0)
            sys_die("%s: failed to transmit the result flag\n", func);

        /* FIXME: confirm that the commit was successful
        tx_commit(sid);
        int succeeded = 1;
        if (sys_write(io, &succeeded, sizeof(int)) < 0)
            sys_die("%s: failed to transmit the result flag\n", func);
        */

        vars_free(r);
        vars_free(w);
        sys_close(io);
        env_free(env);
    } else if (str_cmp(argv[1], "tx") == 0 && source != NULL &&
               data == NULL && state != NULL && port != 0 && tx_addr == NULL)
    {
        tx_server(source, state, &port);
    } else if (str_cmp(argv[1], "vol") == 0 && source == NULL &&
               data != NULL && state == NULL && port != 0 && tx_addr != NULL)
    {
        tx_attach(tx_addr);
        vol_init(port, data);
    } else if (str_cmp(argv[1], "exec") == 0 && source == NULL &&
               data == NULL && state == NULL && port != 0 && tx_addr != NULL)
    {
        tx_attach(tx_addr);
        multiplex(argv[0], tx_addr, port);
    } else
        usage(argv[0]);

    return 0;
}
