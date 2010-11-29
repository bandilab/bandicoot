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
    char method;
    char path[MAX_NAME];
    char *body;
} Http_Req;

extern Http_Req *http_parse(int fd);

extern void http_404(int fd);
extern void http_400(int fd);
extern void http_200(int fd);
extern void http_opts(int fd);
extern void http_chunk(int fd, const void *buf, int size);
