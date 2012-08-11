/*
Copyright 2008-2012 Ostap Cherkashin
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
#include "error.h"
#include "system.h"
#include "memory.h"
#include "string.h"
#include "array.h"
#include "list.h"
#include "head.h"
#include "http.h"
#include "value.h"
#include "tuple.h"
#include "volume.h"
#include "expression.h"
#include "summary.h"
#include "variable.h"
#include "relation.h"
#include "transaction.h"
#include "environment.h"
#include "pack.h"

extern void conv_parse();
extern const char *VERSION;

/* number of concurrently processing requests */
#define THREADS 8

/* how long to wait for a processor to connect */
#define PROC_WAIT_SEC 5

/* how long to keep-alive client connection since it was last used */
#define KEEP_ALIVE_MS 5000

typedef struct {
    List *head;
    List *tail;
    Mon *mon;
} Queue;

typedef struct {
    char exe[MAX_FILE_PATH];
    char tx[MAX_ADDR];
    Queue *runq;
    Queue *waitq;
} Exec;

typedef struct {
    IO *io;
    long long time;
} Conn;

static Conn *conn_new(IO *io)
{
    Conn *res = mem_alloc(sizeof(Conn));
    res->io = io;
    res->time = sys_millis();

    return res;
}

static void conn_free(Conn *c)
{
    sys_close(c->io);
    mem_free(c);
}

static Queue *queue_new()
{
    Queue *q = mem_alloc(sizeof(Queue));
    q->head = NULL;
    q->tail = NULL;
    q->mon = mon_new();

    return q;
}

static void queue_free(Queue *q)
{
    for (List *it = q->head; it != NULL; it = list_next(it))
        conn_free(it->elem);
    mon_free(q->mon);
    mem_free(q);
}

static void queue_put(Queue *q, Conn *conn)
{
    mon_lock(q->mon);

    q->head = list_prepend(q->head, conn);
    if (q->tail == NULL)
        q->tail = q->head;

    mon_signal(q->mon);
    mon_unlock(q->mon);
}

static Conn *queue_get(Queue *q)
{
    mon_lock(q->mon);
    while (q->tail == NULL)
        mon_wait(q->mon, -1);

    Conn *conn = q->tail->elem;
    q->tail = list_prev(q->tail);
    if (q->tail == NULL)
        q->head = NULL;

    mon_unlock(q->mon);

    return conn;
}

static void queue_wait(Queue *q, int ms)
{
    mon_lock(q->mon);
    mon_wait(q->mon, ms);
    mon_unlock(q->mon);
}

static void *waitq_thread(void *arg)
{
    Conn *start = NULL;
    Exec *e = arg;
    for (;;) {
        Conn *c = queue_get(e->waitq);
        if (c->io->stop) {
            start = NULL;
            conn_free(c);
        } else if (sys_iready(c->io, 0)) {
            start = NULL;
            queue_put(e->runq, c);
        } else if (sys_millis() - c->time > KEEP_ALIVE_MS) {
            start = NULL;
            conn_free(c);
        } else {
            if (start == c)                 /* we are in the wait-loop */
                queue_wait(e->waitq, 10);   /* wait for new connections */

            if (start == NULL)
                start = c;

            queue_put(e->waitq, c);
        }
    }

    return NULL;
}

static void *exec_thread(void *arg)
{
    Exec *e = arg;
    for (;;) {
        int p = 0, pid = -1;

        /* creating processor */
        IO *sio = sys_socket(&p);
        char port[8];
        str_print(port, "%d", p);
        char *argv[] = {e->exe, "processor", "-p", port, "-t", e->tx, NULL};

        pid = sys_exec(argv);
        if (!sys_iready(sio, PROC_WAIT_SEC)) {
            sys_log('E', "failed to fork a processor\n");
        } else {
            IO *pio = sys_accept(sio, IO_CHUNK);
            int ok = 1;
            while (ok) {
                pio->stop = 0;
                Conn *c = queue_get(e->runq);

                int pcnt = 0, ccnt = 0;
                sys_proxy(c->io, &ccnt, pio, &pcnt);

                ok = pio->stop == IO_TERM || (ccnt == 0 && !pio->stop);
                if (!ok && pcnt == 0) {
                    int status = http_500(c->io);
                    sys_log('E', "failed with status %d\n", status);
                }

                c->time = sys_millis();
                queue_put(e->waitq, c);
            }
            sys_close(pio);
        }
        sys_close(sio);

        if (pid != -1) {
            sys_kill(pid);
            sys_wait(pid);
        }
    }

    mem_free(e);

    return NULL;
}

static void processor(const char *tx_addr, int port)
{
    sys_init(1);
    sys_log('E', "started port=%d, tx=%s\n", port, tx_addr);

    /* connect to the control thread */
    char addr[MAX_ADDR];
    sys_address(addr, port);
    IO *io = sys_connect(addr, IO_CHUNK);

    tx_attach(tx_addr);

    /* get env code from the tx */
    char *code = tx_program();
    char *res = mem_alloc(MAX_BLOCK);

    while (!io->stop) {
        sys_iready(io, -1);

        int status = -1;
        long long sid = 0LL, time = sys_millis();

        Env *env = NULL;
        Arg *arg = NULL;
        Vars *v = vars_new(0), *r = NULL, *w = NULL;

        Http_Req *req = http_parse_req(io);
        if (io->stop)
            goto exit;

        if (req == NULL) {
            status = http_400(io);
            goto exit;
        }

        if (req->method == OPTIONS) {
            status = http_opts(io);
            goto exit;
        }

        env = env_new("net", code);

        if (str_idx(req->path, "/fn") == 0) {
            int idx = (req->path[3] == '/') ? 4 : 3;
            int i = 0, len = 1, cnt = 0;
            Func **fns = env_funcs(env, req->path + idx, &cnt);

            status = http_200(io);
            while (status == 200 && len) {
                len = pack_fn2csv(fns, cnt, res, MAX_BLOCK, &i);
                status = http_chunk(io, res, len);
            }

            mem_free(fns);
            goto exit;
        }

        /* compare the request with the function defintion */
        Func *fn = env_func(env, req->path + 1);
        if (fn == NULL) {
            Error *err = error_new("unknown function '%s'", req->path + 1);
            status = http_404(io, err->msg);
            mem_free(err);
            goto exit;
        }

        if (fn->rp.name != NULL && req->method != POST) {
            status = http_405(io, POST);
            goto exit;
        }

        if (fn->rp.name == NULL && req->method == POST) {
            status = http_405(io, GET);
            goto exit;
        }

        /* TODO: think what to do with duplicate parameter values */
        for (int i = 0; i < req->args->len; ++i) {
            char *name = req->args->names[i];
            if (array_freq(req->args->names, req->args->len, name) > 1) {
                Error *err = error_new("duplicate parameter '%s' "
                                       "(not supported)",
                                       name);
                status = http_404(io, err->msg);
                mem_free(err);
                goto exit;
            }
        }

        if (fn->pp.len != req->args->len) {
            Error *err = error_new("expected %d primitive parameters, got %d",
                                   fn->pp.len, req->args->len);
            status = http_404(io, err->msg);
            mem_free(err);
            goto exit;
        }

        arg = arg_new();
        for (int i = 0; i < fn->pp.len; ++i) {
            char *name = fn->pp.names[i];
            Type t = fn->pp.types[i];

            int idx = array_scan(req->args->names, req->args->len, name);
            if (idx < 0) {
                Error *err = error_new("unknown parameter '%s'", name);
                status = http_404(io, err->msg);
                mem_free(err);
                goto exit;
            }

            char *val = req->args->vals[idx];
            int error = 0;
            if (t == Int) {
                arg->vals[i].v_int = str_int(val, &error);
            } else if (t == Real)
                arg->vals[i].v_real = str_real(val, &error);
            else if (t == Long)
                arg->vals[i].v_long = str_long(val, &error);
            else if (t == String) {
                arg->vals[i].v_str = str_dup(val);
            }

            if (error) {
                Error *err = error_new("value '%s' (parameter '%s') "
                                       "is not of type '%s'",
                                       val, name, type_to_str(t));
                status = http_404(io, err->msg);
                mem_free(err);
                goto exit;
            }
        }

        if (fn->rp.name != NULL) {
            TBuf *body = NULL;
            if (req->len > 0) {
                Error *err = pack_csv2rel(req->body, fn->rp.head, &body);
                if (err != NULL) {
                    status = http_404(io, err->msg);
                    mem_free(err);
                    goto exit;
                }
            } else {
                body = tbuf_new();
            }

            vars_add(v, fn->rp.name, 0, body);

            /* project the parameter */
            Rel *param = rel_project(rel_load(fn->rp.head, fn->rp.name),
                                     fn->rp.head->names,
                                     fn->rp.head->len);

            rel_eval(param, v, arg);

            /* clean the previous version */
            tbuf_clean(body);
            tbuf_free(body);

            /* replace with the new body */
            int vpos = array_scan(v->names, v->len, fn->rp.name);
            v->vals[vpos] = param->body;

            param->body = NULL;
            rel_free(param);
        }

        /* start a transaction */
        r = vars_new(fn->r.len);
        w = vars_new(fn->w.len);
        for (int i = 0; i < fn->r.len; ++i)
            vars_add(r, fn->r.names[i], 0, NULL);
        for (int i = 0; i < fn->w.len; ++i)
            vars_add(w, fn->w.names[i], 0, NULL);

        sid = tx_enter(addr, r, w);

        /* prepare variables */
        for (int i = 0; i < r->len; ++i) {
            TBuf *body = vol_read(r->vols[i], r->names[i], r->vers[i]);
            vars_add(v, r->names[i], 0, body);
        }
        for (int i = 0; i < w->len; ++i) {
            int pos = array_scan(v->names, v->len, w->names[i]);
            if (pos < 0)
                vars_add(v, w->names[i], 0, NULL);
        }
        for (int i = 0; i < fn->t.len; ++i)
            vars_add(v, fn->t.names[i], 0, NULL);

        /* evaluate the function body */
        for (int i = 0; i < fn->slen; ++i)
            rel_eval(fn->stmts[i], v, arg);

        /* prepare the return value. note, the resulting relation
           is just a container for the body, so it is not freed */
        Rel *ret = NULL;
        if (fn->ret != NULL)
            ret = fn->stmts[fn->slen - 1];

        /* persist the global variables */
        for (int i = 0; i < w->len; ++i) {
            int idx = array_scan(v->names, v->len, w->names[i]);
            if (idx < 0) {
                status = http_500(io);
                goto exit;
            }

            vol_write(w->vols[i], v->vals[idx], w->names[i], w->vers[i]);
            tbuf_free(v->vals[idx]);
            v->vals[idx] = NULL;
        }

        /* confirm a success and send the result back */
        status = http_200(io);
        if (status != 200)
            goto exit;

        tx_commit(sid);

        /* N.B. there is no explicit revert as the transaction manager handles
           nested tx_enter and a connectivity failure as a rollback */

        int len = 1;
        PBuf tmp = { .data = NULL, .len = 0, .iteration = 0 };
        while (status == 200 && len) {
            len = pack_rel2csv(ret, res, MAX_BLOCK, &tmp);
            status = http_chunk(io, res, len);
        }

        /* FIXME: mem leaks on tmp and fn->ret if http_chunk fails in the
         * middle of a relation */
exit:
        if (status != -1)
            sys_log('E', "%016llX method %c, path %s, time %lldms - %3d\n",
                         sid,
                         (req == NULL) ? '?' : req->method,
                         (req == NULL) ? "malformed" : req->path,
                         sys_millis() - time,
                         status);


        if (r != NULL)
            vars_free(r);
        if (w != NULL)
            vars_free(w);
        if (arg != NULL)
            arg_free(arg);
        if (req != NULL)
            http_free_req(req);
        if (env != NULL)
            env_free(env);
        for (int i = 0; i < v->len; ++i)
            if (v->vals[i] != NULL) {
                tbuf_clean(v->vals[i]);
                tbuf_free(v->vals[i]);
            }
        vars_free(v);

        sys_term(io);
    }

    mem_free(code);
    mem_free(res);
    tx_detach();
    sys_close(io);
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
    sys_print("program converter (v5 syntax):\n");
    sys_print(
"  convert - transforms v4 programs to the v5 syntax. the source program is\n");
    sys_print(
"  read from stdin and the result is printed to stdout. all identifiers\n");
    sys_print(
"  which clash with the new keywords will be prefixed with '___'. please\n");
    sys_print(
"  ensure that you review the resulting program.\n\n");

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
    Queue *runq = queue_new();
    Queue *waitq = queue_new();

    for (int i = 0; i < THREADS; ++i) {
        Exec *e = mem_alloc(sizeof(Exec));
        str_cpy(e->exe, exe);
        str_cpy(e->tx, tx_addr);
        e->runq = runq;
        e->waitq = waitq;

        sys_thread(exec_thread, e);

        if (i == 0) /* only one waitq thread (reuses the same operand) */
            sys_thread(waitq_thread, e);
    }

    IO *sio = sys_socket(&port);

    sys_log('E', "started port=%d, tx=%s\n", port, tx_addr);

    for (;;) {
        IO *io = sys_accept(sio, IO_STREAM);
        queue_put(waitq, conn_new(io));
    }

    queue_free(waitq);
    queue_free(runq);
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
        processor(tx_addr, port);
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
    } else if (str_cmp(argv[1], "convert") == 0 && source == NULL &&
               data == NULL && state == NULL && port == 0 && tx_addr == NULL)
    {
        conv_parse();
    } else
        usage(argv[0]);

    return 0;
}
