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

#include "common.h"

/* FIXME: proper tx test is required */
int main(void)
{
    Env *env = env_new(vol_init("bin/volume"));
    tx_init(env->vars.names, env->vars.len);

    char *rnames[] = {"empty_r1", "io"}, *wnames[] = {"io"};
    long rvers[2], wvers[1];

    tx_revert(tx_enter(rnames, rvers, 2, wnames, wvers, 1));

    long sid = tx_enter(rnames, rvers, 2, wnames, wvers, 1);
    Rel *r = gen_rel(0, 10);
    rel_init(r, NULL);
    rel_store(wnames[0], wvers[0], r);
    rel_free(r);
    tx_commit(sid);

    tx_commit(tx_enter(rnames, rvers, 2, 0, 0, 0));

    env_free(env);
    tx_free();

    return 0;
}
