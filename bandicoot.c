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
#include "array.h"
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

/* error messages for the clients */
static const int ERR_UNKNOWN_FN = 0;
static const int ERR_PARAMS_COUNT = 1;
static const int ERR_PARAM_VALUE = 2;
static const int ERR_PARAM_DUPLICATE = 3;

static const struct {
    int code;
    char *msg;
} ERR_MSGS[4] = {{.code = 1, .msg = "unknown function"},
                 {.code = 2, .msg = "provided pararmeters do not "
                                    "match the function declaration"},
                 {.code = 3, .msg = "parameter value does not match the type"},
                 {.code = 4, .msg = "duplicate parameters are not supported"}};

typedef struct {
    char exe[MAX_FILE_PATH];
    char tx[MAX_ADDR];
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
        IO *cio = queue_get();
        IO *pio = NULL;
        int status = 0, pid = -1, cio_cnt = 0, pio_cnt = 0;

        /* creating processor */
        pid = sys_exec(argv);
        if (!sys_iready(sio, PROC_WAIT_SEC)) {
            status = http_500(cio);
            goto exit;
        }

        pio = sys_accept(sio);

        /* sends HTTP 500 if the processor dies without sending data
           to the client */
        sys_exchange(cio, &cio_cnt, pio, &pio_cnt);
        if (pio_cnt == 0)
            status = http_500(cio);
exit:
        if (status != 0)
            sys_log('E', "failed with status %d\n", status);
        if (pid != -1)
            sys_wait(pid);
        if (pio != NULL)
            sys_close(pio);
        sys_close(cio);
    }

    sys_close(sio);
    mem_free(x);

    return NULL;
}

static char *err(int err)
{
    Rel *res = rel_err(ERR_MSGS[err].code, ERR_MSGS[err].msg);
    int size = 0;
    char *body = rel_unpack(res->head, res->body, &size);
    rel_free(res);

    return body;
}

static void processor(const char *tx_addr, int port)
{
    int status = -1;
    long long sid = 0, time = sys_millis();

    Arg *arg = NULL;
    Vars *r = NULL, *w = NULL;
    char *emsg = NULL;

    /* connect to the control thread */
    char addr[MAX_ADDR];
    sys_address(addr, port);
    IO *io = sys_connect(addr);

    Http_Req *req = http_parse(io);
    if (req == NULL) {
        status = http_400(io);
        goto exit;
    }

    if (req->method == OPTIONS) {
        status = http_opts(io);
        goto exit;
    }

    /* get env from the tx */
    tx_attach(tx_addr);
    char *code = tx_program();
    Env *env = env_new("net", code);
    mem_free(code);

    /* compare the request with the function defintion */
    Func *fn = env_func(env, req->path + 1);
    if (fn == NULL) {
        status = http_404(io, (emsg = err(ERR_UNKNOWN_FN)));
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
            status = http_404(io, (emsg = err(ERR_PARAM_DUPLICATE)));
            goto exit;
        }
    }

    if (fn->pp.len != req->args->len)
    {
        status = http_404(io, (emsg = err(ERR_PARAMS_COUNT)));
        goto exit;
    }

    arg = mem_alloc(sizeof(Arg));
    for (int i = 0; i < fn->pp.len; ++i) {
        char *name = fn->pp.names[i];
        Type t = fn->pp.types[i];

        int idx = array_scan(req->args->names, req->args->len, name);
        if (idx < 0) {
            status = http_404(io, (emsg = err(ERR_PARAMS_COUNT)));
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
            error = str_len(val) > MAX_STRING;
            if (!error)
                str_cpy(arg->vals[i].v_str, val);
        }

        if (error) {
            status = http_404(io, (emsg = err(ERR_PARAM_VALUE)));
            goto exit;
        }
    }

    if (fn->rp.name != NULL) {
        Head *head = NULL;
        arg->body = rel_pack_sep(req->body, &head);

        int eq = 0;
        if (head != NULL) {
            eq = head_eq(head, fn->rp.rel->head);
            mem_free(head);
        }

        if (arg->body == NULL || !eq) {
            status = http_400(io);
            goto exit;
        }
    }

    /* start a transaction */
    r = vars_new(fn->r.len);
    w = vars_new(fn->w.len);
    for (int i = 0; i < fn->r.len; ++i)
        vars_put(r, fn->r.names[i], 0L);
    for (int i = 0; i < fn->w.len; ++i)
        vars_put(w, fn->w.names[i], 0L);

    sid = tx_enter(addr, r, w);

    /* execute the function body */
    if (fn->rp.rel != NULL)
        rel_init(fn->rp.rel, r, arg);

    for (int i = 0; i < fn->t.len; ++i)
        rel_init(fn->t.rels[i], r, arg);

    for (int i = 0; i < fn->w.len; ++i) {
        rel_init(fn->w.rels[i], r, arg);
        rel_store(w->vols[i], w->names[i], w->vers[i], fn->w.rels[i]);
    }

    if (fn->ret != NULL)
        rel_init(fn->ret, r, arg);

    /* TODO: commit should not happen if the client has closed the connection */
    tx_commit(sid);

    /* confirm a success and send the result back */
    status = http_200(io);
    if (fn->ret != NULL) {
        int size;
        char *res = rel_unpack(fn->ret->head, fn->ret->body, &size);
        status = http_chunk(io, res, size);

        mem_free(res);
        tbuf_free(fn->ret->body);
        fn->ret->body = NULL;
    }
    status = http_chunk(io, NULL, 0);

exit:
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
    if (env != NULL)
        env_free(env);
    if (req != NULL)
        http_free(req);
    if (arg != NULL)
        mem_free(arg);
    if (emsg != NULL)
        mem_free(emsg);
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

    for (int i = 0; i < THREADS; ++i) {
        Exec *e = mem_alloc(sizeof(Exec));
        str_cpy(e->exe, exe);
        str_cpy(e->tx, tx_addr);

        sys_thread(exec_thread, e);
    }

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
    } else
        usage(argv[0]);

    return 0;
}
