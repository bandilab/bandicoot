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

#include "common.h"

typedef enum { TX_ENTER, TX_COMMIT, TX_REVERT, TX_EXIT,
               TX_READ, TX_CHECK, TX_WRITE } Action;

typedef struct {
    Action action;
    int value;
    Mon *write;
    Mon *read;
    char rname[MAX_NAME];
    char wname[MAX_NAME];
} Proc;

static Env *env;
static Proc p1, p2, p3, cp;
static char current_test[30];
static Mon *gmon;
static int seq;
static char *vid;

static void sem_inc(Mon *m)
{
    mon_lock(m);
    m->value++;
    mon_signal(m);
    mon_unlock(m);
}

static void sem_dec(Mon *m)
{
    mon_lock(m);
    while (m->value < 1)
        mon_wait(m, 100);
    m->value--;
    mon_unlock(m);
}

static void sem_wait(Mon *m, int val)
{
    mon_lock(m);
    while (m->value != val)
        mon_wait(m, 100);
    mon_unlock(m);
}

static void send_action(Proc *p, Action a, int value)
{
    p->action = a;
    p->value = value;
    sem_inc(p->read);
    sem_dec(p->write);
}

static void enter_action(Proc *p)
{
    send_action(p, TX_ENTER, seq++);
}

static void action(Proc *p, Action a)
{
    send_action(p, a, -1);
}

static void check(Proc *p, int value)
{
    send_action(p, TX_CHECK, value);
}

static int count(Rel *r)
{
    Tuple *t = NULL;
    int i = 0;
    while ((t = tbuf_next(r->body)) != NULL) {
        tuple_free(t);
        i++;
    }

    return i;
}

static void *exec_thread(void *arg)
{
    Proc *p = arg;
    Vars *r = vars_new(1);
    if (str_len(p->rname) > 0)
        vars_add(r, p->rname, 0, NULL);

    Vars *w = vars_new(1);
    if (str_len(p->wname) > 0)
        vars_add(w, p->wname, 0, NULL);

    Rel *rel = NULL;
    long long sid = 0;

    int action = TX_ENTER;
    int value = -1;
    while (action != TX_EXIT) {
        sem_dec(p->read);
        action = p->action;
        value = p->value;

        if (action == TX_COMMIT)
            commit(sid);
        else if (action == TX_REVERT)
            revert(sid);
        else if (action == TX_READ) {
            rel = rel_load(env_head(env, p->rname), p->rname);
            TBuf *body = vol_read(r->vols[0], r->names[0], r->vers[0]);
            Vars *v = vars_new(1);
            vars_add(v, p->rname, 0, body);
            rel_eval(rel, v, NULL);
            vars_free(v);
        } else if (action == TX_CHECK) {
            int cnt = count(rel);
            if (cnt != value) {
                sys_print("%s: sid=%d, version=%d, expected=%d, got=%d\n",
                          current_test,
                          sid,
                          r->vers[0],
                          value,
                          cnt);
                tx_state();
                fail();
            }
            rel_free(rel);
        } else if (action == TX_WRITE) {
            vol_write(w->vols[0], rel->body, w->names[0], w->vers[0]);
            rel_free(rel);
        }

        sem_inc(p->write);

        if (action == TX_ENTER) {
            /* This wait call is to synchronize all the concurrent TX_ENTER
               based on the global sequence value. Every TX_ENTER action
               has a unique sequence number assigned to it.
               Each thread has to wait until the global lock reaches the value
               which means the thread is suppose to enter the tx. */

            sem_wait(gmon, value);

            sid = enter("", r, w, gmon);
        }
    }

    vars_free(r);
    vars_free(w);

    return NULL;
}

static void init_thread(Proc *p, char *r, char *w)
{
    p->write = mon_new();
    p->read = mon_new();
    p->action = -1;

    str_cpy(p->rname, r);
    str_cpy(p->wname, w);

    sys_thread(exec_thread, p);
}

static void exit_thread(Proc *p)
{
    action(p, TX_EXIT);

    mon_free(p->write);
    mon_free(p->read);
}

static void test(char *name, int cnt)
{
    init_thread(&cp, name, "");
    enter_action(&cp);
    action(&cp, TX_READ);
    check(&cp, cnt);
    action(&cp, TX_COMMIT);
    exit_thread(&cp);
}

static void test_basics()
{
    /* test read */
    Vars *w = vars_new(0), *r = vars_new(1), *v = vars_new(1);
    vars_add(r, "tx_empty", 0, NULL);

    long long sid = tx_enter("", r, w);
    long long ver = r->vers[0];

    if (str_cmp(r->vols[0], vid) != 0)
        fail();

    Rel *rel = rel_load(env_head(env, r->names[0]), r->names[0]);
    TBuf *body = vol_read(r->vols[0], r->names[0], r->vers[0]);
    vars_add(v, r->names[0], 0, body);
    rel_eval(rel, v, NULL);
    if (count(rel) != 0)
        fail();

    rel_free(rel);
    tx_commit(sid);

    vars_free(r);
    vars_free(w);
    vars_free(v);

    /* test write and revert */
    r = vars_new(0);
    w = vars_new(1);
    vars_add(w, "tx_empty", 0, NULL);

    sid = tx_enter("", r, w);

    if (str_cmp(w->vols[0], vid) != 0)
        fail();

    tx_revert(sid);

    vars_free(r);
    vars_free(w);

    /* test read and revert */
    r = vars_new(1);
    w = vars_new(0);
    vars_add(r, "tx_empty", 0, NULL);

    sid = tx_enter("", r, w);

    if (ver != r->vers[0])
        fail();

    tx_revert(sid);

    vars_free(r);
    vars_free(w);
}

static void test_reset()
{
    str_print(current_test, "test_reset");
    init_thread(&p1, "tx_empty", "tx_target1");
    init_thread(&p2, "tx_empty", "tx_target2");
    init_thread(&p3, "tx_empty", "tx_target3");

    enter_action(&p1);
    action(&p1, TX_READ);
    action(&p1, TX_WRITE);
    action(&p1, TX_COMMIT);

    enter_action(&p2);
    action(&p2, TX_READ);
    action(&p2, TX_WRITE);
    action(&p2, TX_COMMIT);

    enter_action(&p3);
    action(&p3, TX_READ);
    action(&p3, TX_WRITE);
    action(&p3, TX_COMMIT);

    test("tx_target1", 0);
    test("tx_target2", 0);
    test("tx_target3", 0);

    exit_thread(&p1);
    exit_thread(&p2);
    exit_thread(&p3);
}

static void test_cleanup(Action a2)
{
    test_reset();

    str_print(current_test, "test_cleanup");
    init_thread(&p1, "tx_target1", "");
    init_thread(&p2, "one_r1", "tx_target1");

    enter_action(&p1);

    enter_action(&p2);

    action(&p2, TX_READ);
    action(&p2, TX_WRITE);
    action(&p2, a2);

    action(&p1, TX_READ);
    check(&p1, 0);
    action(&p1, TX_COMMIT);

    test("tx_target1", (a2 == TX_COMMIT) ? 1 : 0);

    exit_thread(&p1);
    exit_thread(&p2);
}

static void test_reads()
{
    test_reset();

    str_print(current_test, "test_reads");
    init_thread(&p1, "tx_empty", "");
    init_thread(&p2, "tx_empty", "");

    test("tx_empty", 0);

    enter_action(&p1);
    enter_action(&p2);
    action(&p1, TX_READ);
    action(&p2, TX_READ);
    check(&p1, 0);
    check(&p2, 0);
    action(&p1, TX_COMMIT);
    action(&p2, TX_COMMIT);

    exit_thread(&p1);
    exit_thread(&p2);
}

static void test_write_read_con()
{
    test_reset();

    str_print(current_test, "test_write_read_con");
    init_thread(&p1, "one_r1", "tx_target1");
    init_thread(&p2, "tx_target1", "");

    enter_action(&p1);
    enter_action(&p2);
    action(&p1, TX_READ);
    action(&p1, TX_WRITE);
    action(&p2, TX_READ);
    check(&p2, 0);

    action(&p1, TX_COMMIT);
    action(&p2, TX_COMMIT);

    test("tx_target1", 1);

    exit_thread(&p1);
    exit_thread(&p2);
}

static void test_overwrite_seq(Action a1)
{
    test_reset();

    str_print(current_test, "test_overwrite_seq %s",
              (a1 == TX_COMMIT) ? "TX_COMMIT" : "TX_REVERT");
    init_thread(&p1, "one_r1", "tx_target1");
    init_thread(&p2, "one_r2", "tx_target1");

    enter_action(&p1);
    action(&p1, TX_READ);
    action(&p1, TX_WRITE);
    action(&p1, a1);

    test("tx_target1", (a1 == TX_COMMIT) ? 1 : 0);

    enter_action(&p2);
    action(&p2, TX_READ);
    action(&p2, TX_WRITE);
    action(&p2, TX_COMMIT);

    test("tx_target1", 2);

    exit_thread(&p1);
    exit_thread(&p2);
}

static void test_overwrite_con(Action a1)
{
    test_reset();

    str_print(current_test, "test_overwrite_con %s",
              (a1 == TX_COMMIT) ? "TX_COMMIT" : "TX_REVERT");
    init_thread(&p1, "one_r2", "tx_target1");
    init_thread(&p2, "tx_target1", "tx_target1");

    int cnt = (a1 == TX_COMMIT) ? 2 : 0;

    enter_action(&p1);
    enter_action(&p2);

    action(&p1, TX_READ);
    action(&p1, TX_WRITE);
    action(&p1, a1);

    test("tx_target1", cnt);

    action(&p2, TX_READ);
    action(&p2, TX_WRITE);
    action(&p2, TX_COMMIT);

    test("tx_target1", cnt);

    exit_thread(&p1);
    exit_thread(&p2);
}

static void test_chain(Action a1)
{
    test_reset();

    str_print(current_test, "test_chain %s",
              (a1 == TX_COMMIT) ? "TX_COMMIT" : "TX_REVERT");
    init_thread(&p1, "one_r1", "tx_target1");
    init_thread(&p2, "tx_target1", "tx_target2");
    init_thread(&p3, "tx_target2", "tx_target3");

    int cnt = (a1 == TX_COMMIT ? 1 : 0);

    enter_action(&p1);
    enter_action(&p2);
    enter_action(&p3);

    action(&p1, TX_READ);
    action(&p1, TX_WRITE);
    action(&p1, a1);

    action(&p2, TX_READ);
    action(&p2, TX_WRITE);
    test("tx_target2", 0);
    action(&p2, TX_COMMIT);

    action(&p3, TX_READ);
    action(&p3, TX_WRITE);
    test("tx_target2", cnt);
    test("tx_target3", 0);
    action(&p3, TX_COMMIT);

    exit_thread(&p1);
    exit_thread(&p2);
    exit_thread(&p3);

    test("tx_target3", cnt);
}

int main(void)
{
    int tx_port = 0;
    char *source = "test/test_defs.b";

    sys_init(0);
    tx_server(source, "bin/state", &tx_port);
    vid = vol_init(0, "bin/volume");

    char *code = sys_load(source);
    env = env_new(source, code);
    mem_free(code);

    gmon = mon_new();
    gmon->value = 1;
    seq = 1;

    test_basics();
    test_reset();
    test_reads();

    test_cleanup(TX_COMMIT);
    test_cleanup(TX_REVERT);

    test_write_read_con();
    test_overwrite_seq(TX_COMMIT);
    test_overwrite_seq(TX_REVERT);
    test_overwrite_con(TX_COMMIT);
    test_overwrite_con(TX_REVERT);
    test_chain(TX_COMMIT);
    test_chain(TX_REVERT);

    mon_free(gmon);

    env_free(env);
    tx_free();

    return 0;
}
