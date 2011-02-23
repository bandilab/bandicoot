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

#define QUEUE_LEN 128
#define THREADS 8

typedef struct {
    char func[MAX_NAME];
    struct {
        char vars[MAX_VARS][MAX_NAME];
        long vers[MAX_VARS];
        int len;
    } r, w;
} Call;

typedef struct {
    Env *env;
    char exe[MAX_FILE_PATH];
    char vol[MAX_FILE_PATH];
} Exec;

struct {
    int pos;
    int fds[QUEUE_LEN];
    int len;
    Mon *mon;
} queue;

static void queue_init()
{
    queue.pos = -1;
    queue.len = QUEUE_LEN;
    queue.mon = mon_new();
}

static void queue_put(int fd)
{
    mon_lock(queue.mon);
    while (queue.pos >= queue.len)
        mon_wait(queue.mon);

    queue.pos++;
    queue.fds[queue.pos] = fd;

    mon_signal(queue.mon);
    mon_unlock(queue.mon);
}

static int queue_get()
{
    mon_lock(queue.mon);
    while (queue.pos < 0)
        mon_wait(queue.mon);

    int fd = queue.fds[queue.pos];
    queue.pos--;

    mon_signal(queue.mon);
    mon_unlock(queue.mon);

    return fd;
}

static void *exec_thread(void *arg)
{
    Exec *x = arg;
    int p = 0, sfd = sys_socket(&p);
    char port[16];
    str_print(port, "%d", p);
    char *argv[] = {x->exe, "executor", "-v", x->vol, "-p", port, NULL};

    for (;;) {
        int status = -1, pid = -1;
        int cfd = queue_get(), pfd = -1;
        long sid = 0, time = sys_millis();
        TBuf *param = NULL, *ret = NULL;

        Http_Req *req = http_parse(cfd);
        if (req == NULL) {
            status = http_400(cfd);
            goto exit;
        }

        if (req->method == OPTIONS) {
            status = http_opts(cfd);
            goto exit;
        }

        Func *fn = env_func(x->env, req->path + 1);
        if (fn == NULL) {
            status = http_404(cfd);
            goto exit;
        }

        if ((fn->p.len == 1 && req->method != POST) ||
            (fn->p.len == 0 && req->method == POST))
        {
            status = http_405(cfd);
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
                status = http_400(cfd);
                goto exit;
            }
        }

        Call call = {.r.len = fn->r.len, .w.len = fn->w.len};
        str_cpy(call.func, req->path + 1);
        for (int i = 0; i < call.r.len; ++i)
            str_cpy(call.r.vars[i], fn->r.vars[i]);
        for (int i = 0; i < call.w.len; ++i)
            str_cpy(call.w.vars[i], fn->w.vars[i]);

        /* FIXME: what if an executor never connects? */

        pid = sys_exec(argv);
        pfd = sys_accept(sfd);
        sid = tx_enter(fn->r.vars, call.r.vers, call.r.len,
                       fn->w.vars, call.w.vers, call.w.len);

        if (sys_send(pfd, &call, sizeof(Call)) < 0) {
            status = http_500(cfd);
            goto exit;
        }

        if (fn->p.len == 1) {
            if (tbuf_write(param, pfd) < 0) {
                status = http_500(cfd);
                goto exit;
            }
            tbuf_free(param);
            param = NULL;
        }

        if (fn->ret != NULL) {
            ret = tbuf_read(pfd);
            if (ret == NULL) {
                status = http_500(cfd);
                goto exit;
            }
        }

        int failed = -1;
        if (sys_recvn(pfd, &failed, sizeof(int)) != sizeof(int) || failed) {
            tx_revert(sid);
            status = http_500(cfd);
            goto exit;
        }

        status = http_200(cfd);
        if (ret != NULL) {
            int size;
            char *res = rel_unpack(fn->ret->head, ret, &size);
            status = http_chunk(cfd, res, size);

            mem_free(res);
            tbuf_free(ret);
            ret = NULL;
        }

        tx_commit(sid);
        status = http_chunk(cfd, NULL, 0);

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
        if (pfd != -1)
            sys_close(pfd);

        sys_close(cfd);
    }

    sys_close(sfd);
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

    if (str_cmp(argv[1], "deploy") == 0 &&
        s != NULL && v != NULL && p == 0)
    {
        vol_deploy(v, s);
        sys_print("deployed: %s -> %s\n", s, v);
    } else if (str_cmp(argv[1], "start") == 0 &&
               s == NULL && v != NULL && p != 0)
    {
        char *src = vol_init(v, 1);
        queue_init();
        sys_signals();

        Env *env = env_new(src);
        tx_init(env->vars.names, env->vars.len);
        env_free(env);

        for (int i = 0; i < THREADS; ++i) {
            Exec *e = mem_alloc(sizeof(Exec));
            e->env = env_new(src);
            str_cpy(e->vol, v);
            str_cpy(e->exe, argv[0]);

            sys_thread(exec_thread, e);
        }

        int sfd = sys_socket(&p);

        char tmp[32];
        sys_time(tmp);
        sys_print("started: %s, volume=%s, port=%d\n--\n", tmp, v, p);

        for (;;) {
            long cfd = sys_accept(sfd);
            queue_put(cfd);
        }

        tx_free();
        mon_free(queue.mon);
    } else if (str_cmp(argv[1], "executor") == 0 &&
               s == NULL && v != NULL && p != 0)
    {
        char *src = vol_init(v, 0);
        Env *env = env_new(src);

        int fd = sys_connect(p);

        Call call;
        sys_readn(fd, &call, sizeof(Call));

        int idx = 0;
        Func *fn = env_func(env, call.func);
        Args args = {.len = fn->r.len + fn->p.len};

        if (fn->p.len == 1) {
            args.names[0] = fn->p.names[0];
            args.tbufs[0] = tbuf_read(fd);
            if (args.tbufs[0] == NULL)
                sys_die("%s: failed to retrieve the parameter\n", call.func);

            idx++;
        }

        for (int i = 0; i < call.r.len; ++i, ++idx) {
            args.names[idx] = call.r.vars[i];
            args.vers[idx] = call.r.vers[i];
        }

        for (int i = 0; i < fn->p.len; ++i)
            rel_init(fn->p.rels[i], &args);

        for (int i = 0; i < fn->t.len; ++i)
            rel_init(fn->t.rels[i], &args);

        for (int i = 0; i < fn->w.len; ++i) {
            rel_init(fn->w.rels[i], &args);
            /* FIXME: potential mismatch in call/fn vars/rels */
            rel_store(call.w.vars[i], call.w.vers[i], fn->w.rels[i]);
        }

        if (fn->ret != NULL) {
            rel_init(fn->ret, &args);
            if (tbuf_write(fn->ret->body, fd) < 0)
                sys_die("%s: failed to transmit the result\n", call.func);
        }

        int failed = 0;
        sys_write(fd, &failed, sizeof(int));

        sys_close(fd);
        env_free(env);
    } else
        usage(argv[0]);

    return 0;
}
