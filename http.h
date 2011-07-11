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

static const char GET = 'G';
static const char POST = 'P';
static const char OPTIONS = 'O';

typedef struct {
    int len;
    char *names[MAX_ATTRS];
    char *vals[MAX_ATTRS];
} Http_Args;

typedef struct {
    char method;
    char path[MAX_NAME];
    Http_Args *args;
    char *body;
} Http_Req;

extern Http_Req *http_parse(IO *io);
extern void http_free(Http_Req *req);

extern int http_500(IO *io);
extern int http_405(IO *io, const char method);
extern int http_404(IO *io, const char *response);
extern int http_400(IO *io);
extern int http_200(IO *io);
extern int http_opts(IO *io);
extern int http_chunk(IO *io, const void *buf, int size);
