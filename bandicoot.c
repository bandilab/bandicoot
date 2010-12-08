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
#include "volume.h"
#include "head.h"
#include "http.h"
#include "value.h"
#include "tuple.h"
#include "expression.h"
#include "summary.h"
#include "transaction.h"
#include "relation.h"
#include "environment.h"
#include "pack.h"

extern const char *VERSION;

static Env *env = NULL;

typedef struct {
    int fd;
    Func *fn;
    Http_Req *req;
    long rvers[MAX_VARS];
    long wvers[MAX_VARS];
} Exec;

static void exec_proc(void *arg)
{
    Exec *e = arg;
    Func *fn = e->fn;
    Http_Req *req = e->req;

    if (fn->p.len == 1 && req->method != POST)
        sys_die("FIXME: should be 404");
    if (req->method == POST && fn->p.len == 0)
        sys_die("FIXME: should be 404");

    int idx = 0;
    Args args = {.len = fn->r.len + fn->p.len};
    if (fn->p.len == 1) {
        args.names[0] = fn->p.names[0];

        Head *head;
        TBuf *tbuf = rel_pack_sep(req->body, &head);

        if (tbuf == NULL)
            sys_die("FIXME: should be 400");
        if (!head_eq(head, fn->p.rels[0]->head))
            sys_die("FIXME: should be 404");

        args.tbufs[0] = tbuf;
        idx++;
    }

    for (int i = 0; i < fn->r.len; ++i, ++idx) {
        args.names[idx] = fn->r.vars[i];
        args.vers[idx] = e->rvers[i];
    }

    for (int i = 0; i < fn->p.len; ++i)
        rel_init(fn->p.rels[i], &args);

    for (int i = 0; i < fn->t.len; ++i)
        rel_init(fn->t.rels[i], &args);

    for (int i = 0; i < fn->w.len; ++i) {
        rel_init(fn->w.rels[i], &args);
        rel_store(fn->w.vars[i], e->wvers[i], fn->w.rels[i]);
    }

    if (fn->ret != NULL) {
        rel_init(fn->ret, &args);

        int size;
        char *res = rel_unpack(fn->ret, &size);
        http_chunk(e->fd, res, size);

        mem_free(res);
    }
}

static void *exec_thread(void *arg)
{
    int ok = 0, fd = (long) arg;
    long time = sys_millis(), sid = 0;

    Http_Req *req = http_parse(fd);
    if (req == NULL) {
        http_400(fd);
        goto exit;
    }

    if (req->method == OPTIONS) {
        http_opts(fd);
        ok = 1;
        goto exit;
    }

    /* FIXME: concurrent calls to env_func */
    Func *fn = env_func(env, req->path + 1);
    if (fn == NULL) {
        http_404(fd);
        goto exit;
    }

    Exec e = {.fd = fd, .fn = fn, .req = req};
    /* FIXME: concurrent access of fn */
    sid = tx_enter(fn->r.vars, e.rvers, fn->r.len,
                   fn->w.vars, e.wvers, fn->w.len);

    http_200(fd);

    ok = !sys_proc(exec_proc, &e);
    if (ok) {
        tx_commit(sid);
        http_chunk(fd, NULL, 0);
    } else {
        tx_revert(sid);
        /* FIXME: as we don't terminate with the 0-chunk it should be
           treated as a failure on the client side, though if we
           have more verbose child exit codes we could have handled
           it more gracefully */
    }

exit:
    sys_print("[%016X] method '%c', path '%s', time %dms - %s\n",
              sid,
              (req == NULL) ? '?' : req->method,
              (req == NULL) ? "malformed" : req->path,
              sys_millis() - time,
              ok ? "ok" : "failed");

    if (req != NULL)
        mem_free(req);
    sys_close(fd);

    return NULL;
}

static void usage(char *p)
{
    sys_print("usage: %s <command> <args>\n\n", p);
    sys_print("commands:\n");
    sys_print("  start  -v <volume> -p <port>\n");
    sys_print("  deploy -v <volume> -s <source.file>\n\n");
    sys_print(VERSION);
    sys_exit(1);
}

int main(int argc, char *argv[])
{
    int e = -1, p;
    char *v = NULL, *s = NULL;

    if (argc < 2)
        usage(argv[0]);

    for (int i = 2; (i + 1) < argc; i += 2)
        if (str_cmp(argv[i], "-p") == 0)
            p = str_int(argv[i + 1], &e);
        else if (str_cmp(argv[i], "-v") == 0)
            v = argv[i + 1];
        else if (str_cmp(argv[i], "-s") == 0)
            s = argv[i + 1];
        else
            usage(argv[0]);

    if (str_cmp(argv[1], "deploy") == 0 && s != NULL && v != NULL && e == -1) {
        vol_deploy(v, s);
        sys_print("deployed: %s -> %s\n", s, v);
    } else if (str_cmp(argv[1], "start") == 0 &&
               s == NULL && v != NULL && e != -1)
    {
        if (e || p < 1 || p > 65535) {
            sys_print("invalid port '%d'\n", p);
            sys_exit(1);
        }

        env = env_new(vol_init(v));
        tx_init(env->vars.names, env->vars.len);

        int sfd = sys_socket(p);

        char tmp[32];
        sys_time(tmp);
        sys_print("started: %s, volume=%s, port=%d\n--\n", tmp, v, p);

        for (;;) {
            long cfd = sys_accept(sfd);
            sys_thread(exec_thread, (void*) cfd);
        }

        tx_free();
        env_free(env);
    } else
        usage(argv[0]);

    return 0;
}
