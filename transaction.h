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

extern void tx_server(int *port);
extern void tx_attach(int port);
extern void tx_deploy(const char *new_src);
extern Vars *tx_volume_sync(long long vol_id, Vars *in);
extern long tx_enter(Vars *rvars, Vars *wvars);
extern void tx_commit(long sid);
extern void tx_revert(long sid);
extern void tx_state();
extern void tx_free();
