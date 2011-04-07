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
#include "memory.h"
#include "string.h"
#include "fs.h"
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
    char func[MAX_NAME];
    Vars *r;
    Vars *w;
} Call;

static void call_free(Call *c)
{
    if (c->r != NULL)
        vars_free(c->r);
    if (c->w != NULL)
        vars_free(c->w);
    mem_free(c);
}

static int call_write(Call *c, IO *io)
{
    if (sys_write(io, c->func, sizeof(c->func)) < 0)
        return -1;

    if (vars_write(c->r, io) < 0)
        return -1;

    if (vars_write(c->w, io) < 0)
        return -1;

    return 1;
}

static Call *call_read(IO *io)
{
    Call *res = NULL, *c = mem_alloc(sizeof(Call));
    c->r = NULL;
    c->w = NULL;

    if (sys_readn(io, c->func, sizeof(c->func)) != sizeof(c->func))
       goto failure;
    else if ((c->r = vars_read(io)) == NULL)
       goto failure;
    else if ((c->w = vars_read(io)) == NULL)
       goto failure;

    res = c;
    c = NULL;

failure:
    if (c != NULL)
        call_free(c);

    return res;
}

typedef struct {
    Env *env;
    char exe[MAX_FILE_PATH];
    char vol[MAX_FILE_PATH];
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
    char *argv[] = {x->exe, "executor", "-v", x->vol, "-p", port, NULL};

    for (;;) {
        int status = -1, pid = -1;
        IO *cio = queue_get();
        IO *pio = NULL;
        long sid = 0, time = sys_millis();
        TBuf *param = NULL, *ret = NULL;
        Call *call = NULL;

        Http_Req *req = http_parse(cio);
        if (req == NULL) {
            status = http_400(cio);
            goto exit;
        }

        if (req->method == OPTIONS) {
            status = http_opts(cio);
            goto exit;
        }

        Func *fn = env_func(x->env, req->path + 1);
        if (fn == NULL) {
            status = http_404(cio);
            goto exit;
        }

        if ((fn->p.len == 1 && req->method != POST) ||
            (fn->p.len == 0 && req->method == POST))
        {
            status = http_405(cio);
            goto exit;
        }

        if (fn->p.len == 1) {
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

        call = mem_alloc(sizeof(Call));
        str_cpy(call->func, req->path + 1);
        call->r = vars_new(fn->r.len);
        call->w = vars_new(fn->w.len);
        for (int i = 0; i < fn->r.len; ++i)
            vars_put(call->r, fn->r.vars[i], 0L);
        for (int i = 0; i < fn->w.len; ++i)
            vars_put(call->w, fn->w.vars[i], 0L);

        pid = sys_exec(argv);
        if (!sys_iready(sio, PROC_WAIT_SEC)) {
            status = http_500(cio);
            goto exit;
        }

        /* FIXME: sys_accept might fail (and cause sys_die). */
        pio = sys_accept(sio);
        sid = tx_enter(call->r, call->w);

        if (call_write(call, pio) < 0) {
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

        if (fn->ret != NULL) {
            ret = tbuf_read(pio);
            if (ret == NULL) {
                status = http_500(cio);
                goto exit;
            }
        }

        int failed = -1;
        if (sys_readn(pio, &failed, sizeof(int)) != sizeof(int) || failed) {
            tx_revert(sid);
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

        tx_commit(sid);
        status = http_chunk(cio, NULL, 0);

exit:
        sys_print("[%016X] method '%c', path '%s', time %dms - %3d\n",
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

        if (call != NULL)
            call_free(call);

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
    sys_print("commands:\n");
    sys_print("  start  -v <volume> -p <port>\n");
    sys_print("  deploy -v <volume> -s <source.file>\n\n");
    sys_die(VERSION);
}

int main(int argc, char *argv[])
{
    int p = 0;
    char *v = NULL, *s = NULL;

    sys_init();
    if (argc < 2)
        usage(argv[0]);

    for (int i = 2; (i + 1) < argc; i += 2)
        if (str_cmp(argv[i], "-p") == 0) {
            int e = -1;
            p = str_int(argv[i + 1], &e);
            if (e || p < 1 || p > 65535)
                sys_die("invalid port '%d'\n", p);
        } else if (str_cmp(argv[i], "-v") == 0)
            v = argv[i + 1];
        else if (str_cmp(argv[i], "-s") == 0)
            s = argv[i + 1];
        else
            usage(argv[0]);

    fs_init(v);
    if (str_cmp(argv[1], "deploy") == 0 &&
        s != NULL && v != NULL && p == 0)
    {
        tx_deploy(s);
        sys_print("deployed: %s -> %s\n", s, v);
    } else if (str_cmp(argv[1], "start") == 0 &&
               s == NULL && v != NULL && p != 0)
    {
        int tx_port = 0;
        tx_server(&tx_port);
        tx_attach(tx_port);
        vol_init();
        queue_init();

        for (int i = 0; i < THREADS; ++i) {
            Exec *e = mem_alloc(sizeof(Exec));
            e->env = env_new(fs_source);
            str_cpy(e->vol, v);
            str_cpy(e->exe, argv[0]);

            sys_thread(exec_thread, e);
        }

        IO *sio = sys_socket(&p);

        char tmp[32];
        sys_time(tmp);
        sys_print("started: %s, volume=%s, port=%d\n--\n", tmp, v, p);

        for (;;) {
            IO *cio = sys_accept(sio);
            queue_put(cio);
        }

        tx_free();
        mon_free(queue.mon);
    } else if (str_cmp(argv[1], "executor") == 0 &&
               s == NULL && v != NULL && p != 0)
    {
        Env *env = env_new(fs_source);

        IO *io = sys_connect(p);

        Call *call = call_read(io);
        if (call == NULL)
            sys_die("executor: failed to retrieve function details\n");

        Func *fn = env_func(env, call->func);
        TBuf *arg;

        if (fn->p.len == 1) {
            arg = tbuf_read(io);
            if (arg == NULL)
                sys_die("%s: failed to retrieve the parameter\n", call->func);
        }

        for (int i = 0; i < fn->p.len; ++i)
            rel_init(fn->p.rels[i], call->r, arg);

        for (int i = 0; i < fn->t.len; ++i)
            rel_init(fn->t.rels[i], call->r, arg);

        for (int i = 0; i < fn->w.len; ++i) {
            rel_init(fn->w.rels[i], call->r, arg);
            rel_store(call->w->vars[i], call->w->vers[i], fn->w.rels[i]);
        }

        if (fn->ret != NULL) {
            rel_init(fn->ret, call->r, arg);
            if (tbuf_write(fn->ret->body, io) < 0)
                sys_die("%s: failed to transmit the result\n", call->func);
        }

        int failed = 0;
        if (sys_write(io, &failed, sizeof(int)) < 0)
            sys_die("%s: failed to transmit the result flag\n", call->func);

        call_free(call);
        sys_close(io);
        env_free(env);
    } else
        usage(argv[0]);

    return 0;
}
