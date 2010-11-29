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

/* input/output */
static const int CREATE = 0x01;
static const int READ = 0x02;
static const int WRITE = 0x04;

extern int sys_open(const char *path, int mode);
extern int sys_exists(const char *path);
extern int sys_read(int fd, void *buf, int size);
extern int sys_readn(int fd, void *buf, int size);
extern void sys_write(int fd, const void *buf, int size);
extern void sys_move(const char *dest, const char *src);
extern void sys_cpy(const char *dest, const char *src);
extern void sys_remove(const char *path);
extern void sys_close(int fd);
extern char *sys_load(const char *path);
extern char **sys_lsdir(const char *path, int *len);
extern int sys_empty(const char *dir);

/* [multi]proc */
extern int sys_proc(void (*fn)(void *arg), void *arg);
extern void sys_thread(void *(*fn)(void *arg), void *arg);
extern void sys_exit(int status);
extern void sys_die(const char *msg, ...);

/* networking */
extern int sys_socket(int port);
extern int sys_accept(int socket);

/* misc */
extern void sys_print(const char *msg, ...);
extern long sys_millis(); /* suitable for quick time measures only (long might
                             not be enough on 32bit systems) */
