// -*- coding: utf-8 -*-
// <nginx markdown module - serves markdown files as HTML right from nginx>
// Copyright (C) <2011>  Gabriel Falcão <gabriel@nacaolivre.org>

// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following
// conditions:

// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#include "ngx_http_markdown_module.h"

static ngx_command_t  ngx_http_markdown_commands[] = {
    { ngx_string("markdown"),
      NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_markdown_loc_conf_t, css_path),
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_markdown_module_ctx = {
    NULL,                               /* preconfiguration */
    ngx_http_markdown_init,             /* postconfiguration */
    NULL,                               /* create main configuration */
    NULL,                               /* init main configuration */
    NULL,                               /* create server configuration */
    NULL,                               /* merge server configuration */
    ngx_http_markdown_create_loc_conf,  /* create location configuration */
    ngx_http_markdown_merge_loc_conf    /* merge location configuration */
};


ngx_module_t  ngx_http_markdown_module = {
    NGX_MODULE_V1,
    &ngx_http_markdown_module_ctx, /* module context */
    ngx_http_markdown_commands,    /* module directives */
    NGX_HTTP_MODULE,               /* module type */
    NULL,                          /* init master */
    NULL,                          /* init module */
    NULL,                          /* init process */
    NULL,                          /* init thread */
    NULL,                          /* exit thread */
    NULL,                          /* exit process */
    NULL,                          /* exit master */
    NGX_MODULE_V1_PADDING
};

ngx_buf_t *
create_buffer(u_char *p, ngx_int_t len, ngx_pool_t *pool)
{
    ngx_buf_t   *b;

    b = ngx_pcalloc(pool, sizeof(ngx_buf_t));
    if (b == NULL) {
        return NULL;
    }

    b->start = b->pos = p;
    b->end = b->last = p + len;

    if (len == 0) {
        b->sync = 1;
    }
    else {
        b->memory = 1;
    }

    return b;
}

static ngx_int_t
ngx_http_markdown_handler(ngx_http_request_t *r)
{
    ngx_int_t                    rc;
    ngx_buf_t                   *buf;
    ngx_chain_t                  out;
    ngx_str_t                    content_type = ngx_string("text/html");
    u_char                      *req;
    int                          req_size;
    char                        *content;
    size_t                       content_length;
    ngx_http_markdown_loc_conf_t  *conf;

    int MKD_FLAGS = MKD_TABSTOP | MKD_NORELAXED | MKD_AUTOLINK;
    MMIOT *mkd;

    conf = ngx_http_get_module_loc_conf(r, ngx_http_markdown_module);

    buf  = out.buf;
    req = buf->pos;
    req_size = buf->last - buf->pos;

    mkd = mkd_string((char *)req, req_size, MKD_FLAGS);
    mkd_compile(mkd, MKD_FLAGS);
    content_length = mkd_document(mkd, &content);

    buf->pos = (u_char *)content;
    buf->last = (u_char *) content + content_length; /* last position */

    buf->memory = 1;

    buf->last_buf = 1; /* there will be no more buffers in the request */


    out.next = NULL;
    out.buf = buf;
    if (out.buf == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    mkd_cleanup(mkd);

    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_type = content_type;
    r->headers_out.content_length_n = content_length;

    rc = ngx_http_send_header(r);
    return ngx_http_output_filter(r, &out);
}

static void *
ngx_http_markdown_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_markdown_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_markdown_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }
    conf->handler = NGX_CONF_UNSET_UINT;

    return (void *)conf;
}


static char *
ngx_http_markdown_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_markdown_loc_conf_t  *prev = parent;
    ngx_http_markdown_loc_conf_t  *conf = child;

    ngx_conf_merge_uint_value(conf->handler,
                              prev->handler, 0);

    ngx_conf_merge_str_value(conf->css_path,
                             prev->css_path, "");

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_markdown_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt        *h;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_markdown_handler;

    return NGX_OK;
}
