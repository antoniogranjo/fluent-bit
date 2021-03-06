/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*  Fluent Bit
 *  ==========
 *  Copyright (C) 2015-2017 Treasure Data Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <msgpack.h>
#include <fluent-bit/flb_input.h>
#include <fluent-bit/flb_config.h>
#include <fluent-bit/flb_error.h>
#include <fluent-bit/flb_utils.h>
#include <fluent-bit/flb_stats.h>

#include "syslog.h"
#include "syslog_conf.h"
#include "syslog_unix.h"
#include "syslog_conn.h"

/* cb_collect callback */
static int in_syslog_collect(struct flb_input_instance *i_ins,
                             struct flb_config *config, void *in_context)
{
    int fd;
    struct flb_syslog *ctx = in_context;
    struct syslog_conn *conn;
    (void) i_ins;

    /* Accept the new connection */
    fd = flb_net_accept(ctx->server_fd);
    if (fd == -1) {
        flb_error("[in_syslog] could not accept new connection");
        return -1;
    }

    flb_trace("[in_syslog] new Unix connection arrived FD=%i", fd);
    conn = syslog_conn_add(fd, ctx);
    if (!conn) {
        return -1;
    }

    return 0;
}

/* Initialize plugin */
static int in_syslog_init(struct flb_input_instance *in,
                          struct flb_config *config, void *data)
{
    int ret;
    struct flb_syslog *ctx;

    /* Allocate space for the configuration */
    ctx = syslog_conf_create(in, config);
    if (!ctx) {
        flb_error("[in_syslog] could not initialize plugin");
        return -1;
    }

    if (!ctx->unix_path) {
        flb_error("[in_syslog] Unix path not defined");
        syslog_conf_destroy(ctx);
        return -1;
    }

    /* Create Unix Socket */
    ret = syslog_unix_create(ctx);
    if (ret == -1) {
        syslog_conf_destroy(ctx);
        return -1;
    }

    /* Set context */
    flb_input_set_context(in, ctx);


    /* Collect events for every opened connection to our socket */
    ret = flb_input_set_collector_socket(in,
                                         in_syslog_collect,
                                         ctx->server_fd,
                                         config);
    if (ret == -1) {
        flb_error("[in_syslog] Could not set collector");
        syslog_conf_destroy(ctx);
    }

    return 0;
}

static int in_syslog_exit(void *data, struct flb_config *config)
{
    struct flb_syslog *ctx = data;
    (void) config;

    syslog_conf_destroy(ctx);

    return 0;
}


struct flb_input_plugin in_syslog_plugin = {
    .name         = "syslog",
    .description  = "Syslog",
    .cb_init      = in_syslog_init,
    .cb_pre_run   = NULL,
    .cb_collect   = in_syslog_collect,
    .cb_flush_buf = NULL,
    .cb_exit      = in_syslog_exit
};
