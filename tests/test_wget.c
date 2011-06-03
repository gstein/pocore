/*
  test_wget.c :  basic test of an HTTP GET

  ====================================================================
    Copyright 2011 Greg Stein

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
  ====================================================================
*/

#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "pc_types.h"
#include "pc_misc.h"
#include "pc_memory.h"
#include "pc_channel.h"
#include "pc_error.h"


struct channel_baton_s {
    pc_bool_t finished;

    pc_bool_t request_written;

    const char *host;
    int port;
    const char *path;

    struct iovec iov[7];
    char portbuf[6];
};


/* Rudimentary URL parsing.  */
static void
parse_url(const char **scheme,
          const char **host,
          int *port,
          const char **path,
          const char *url,
          pc_pool_t *pool)
{
    const char *scan = strchr(url, ':');
    const char *scan2;
    const char *scan3;

    if (scan == NULL || scan == url)
        goto error;
    *scheme = pc_strmemdup(pool, url, scan - url);

    if (*++scan != '/')
        goto error;
    if (*++scan != '/')
        goto error;

    scan2 = strchr(++scan, ':');
    scan3 = strchr(scan, '/');
    if (scan2 == NULL && scan3 == NULL)
    {
        /* No port, and no path. Just a hostname.  */
        *host = pc_strdup(pool, scan);
        *port = 80;
        *path = "/";
        return;
    }

    if (scan3 < scan2)
        scan2 = NULL;  /* ### probably an error to find ':' after '/'  */

    if (scan2 != NULL)
    {
        size_t len = scan3 - scan2 - 1;
        char buf[6] = { 0 };

        if (len > 5)
            goto error;  /* A port > 99999 is definitely wrong  */

        *host = pc_strmemdup(pool, scan, scan2 - scan);

        memcpy(buf, scan2 + 1, len);
        *port = atoi(buf);
    }
    else
    {
        *host = pc_strmemdup(pool, scan, scan3 - scan);
        *port = 80;
    }

    *path = pc_strdup(pool, scan3);
    return;

  error:
    *scheme = *host = *path = NULL;
    *port = 0;
}


static pc_error_t *
read_channel(ssize_t *consumed,
             const void *buf,
             size_t len,
             pc_channel_t *channel,
             void *baton,
             pc_pool_t *pool)
{
    if (buf == NULL)
    {
        *consumed = PC_CONSUMED_CONTINUE;
        return PC_NO_ERROR;
    }

    write(1 /* stdout */, buf, len);
    *consumed = len;

    return PC_NO_ERROR;
}


static pc_error_t *
write_channel(struct iovec **iov,
              int *iovcnt,
              pc_channel_t *channel,
              void *baton,
              pc_pool_t *pool)
{
    struct channel_baton_s *cb = baton;
    int i;

    if (cb->request_written)
    {
        /* Nothing more to write.  */
        *iov = NULL;
        return PC_NO_ERROR;
    }

    cb->iov[0].iov_base = "GET ";
    cb->iov[1].iov_base = (char *)cb->path;
    cb->iov[2].iov_base = (" HTTP/1.1\n"
                           "Host: ");
    cb->iov[3].iov_base = (char *)cb->host;

    if (cb->port != 80)
    {
        snprintf(cb->portbuf, sizeof(cb->portbuf), "%d", cb->port);

        cb->iov[4].iov_base = ":";
        cb->iov[5].iov_base = cb->portbuf;
        cb->iov[6].iov_base = "\n\n";
        i = 7;
    }
    else
    {
        cb->iov[4].iov_base = "\n\n";
        i = 5;
    }

    *iov = &cb->iov[0];
    *iovcnt = i;

    while (i--)
        cb->iov[i].iov_len = strlen(cb->iov[i].iov_base);

    cb->request_written = TRUE;
    return PC_NO_ERROR;
}


static void
dump_error(const pc_error_t *error)
{
    while (error != NULL)
    {
        const char *file;
        int lineno;
        int code;
        const char *msg;
        const pc_error_t *separate;

        pc_error_trace_info(&file, &lineno, &code, &msg, &error, &separate,
                            error);
        fprintf(stderr, "%s:%d: [%d] %s\n", file, lineno, code, msg);
        if (separate != NULL)
        {
            /* ### there is a whole chain of SEPARATE issues, and we could
               ### also print out FILE and LINENO. just go simple.  */
            fprintf(stderr, "-- also: [%d] %s\n",
                    pc_error_code(separate), pc_error_message(separate));
        }
    }
}


static void usage()
{
    printf("Usage: test_wget url\n");
}

int main(int argc, const char **argv)
{
    pc_context_t *ctx = pc_context_create();
    pc_pool_t *pool = pc_pool_root(ctx);
    const char *scheme;
    const char *host;
    int port;
    const char *path;
    pc_error_t *err;
    pc_hash_t *addresses;
    pc_hiter_t *hi;
    pc_address_t *addr;
    pc_channel_t *channel;
    struct channel_baton_s cb = { 0 };

    if (argc < 2)
    {
        usage();
        return EXIT_FAILURE;
    }
    pc_context_tracing(ctx, TRUE);

    parse_url(&scheme, &host, &port, &path, argv[1], pool);
    if (scheme == NULL || strcmp(scheme, "http") != 0)
    {
        fprintf(stderr, "error: could not parse URL\n");
        return EXIT_FAILURE;
    }

    err = pc_error_trace(pc_address_lookup(&addresses, host, port, 0, pool));
    if (err)
        goto error;

    /* Just grab the first address.  */
    hi = pc_hiter_begin(addresses, pool);
    if (hi == NULL)
    {
        fprintf(stderr, "error: lookup returned no results for '%s'\n", host);
        return EXIT_FAILURE;
    }

    addr = pc_hiter_value(hi);

    err = pc_error_trace(pc_channel_create_tcp(&channel, ctx, addr, NULL,
                                               PC_CHANNEL_DEFAULT_FLAGS));
    if (err)
        goto error;

    cb.host = host;
    cb.port = port;
    cb.path = path;

    pc_channel_desire_read(channel, read_channel, &cb);
    pc_channel_desire_write(channel, write_channel, &cb);

    do
    {
        err = pc_error_trace(pc_channel_run_events(ctx, 10));
        if (err)
            goto error;

    } while (!cb.finished);

    pc_channel_destroy(channel);

    return EXIT_SUCCESS;

  error:
    dump_error(err);
    return EXIT_FAILURE;
}
