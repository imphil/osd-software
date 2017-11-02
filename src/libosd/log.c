/* Copyright (c) 2017 by the author(s)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * ============================================================================
 *
 * Author(s):
 *   Philipp Wagner <philipp.wagner@tum.de>
 */

#include <osd/osd.h>
#include "osd-private.h"

#include <assert.h>
#include <syslog.h>
#include <stdlib.h>
#include <errno.h>

/**
 * Default log priority if not set otherwise
 */
#define LOG_PRIORITY_DEFAULT LOG_ERR

/**
 * Logging context
 */
struct osd_log_ctx {
    /** logging function */
    osd_log_fn log_fn;
    /** logging priority */
    int log_priority;
    /** caller context */
    void* caller_ctx;
    /** log mutex */
    pthread_mutex_t lock;
};

void osd_log(struct osd_log_ctx *ctx,
             int priority, const char *file, int line, const char *fn,
             const char *format, ...)
{
    va_list args;

    if (!ctx->log_fn) {
        return;
    }

    va_start(args, format);
    pthread_mutex_lock(&ctx->lock);
    ctx->log_fn(ctx, priority, file, line, fn, format, args);
    pthread_mutex_unlock(&ctx->lock);
    va_end(args);
}

API_EXPORT
osd_result osd_log_new(struct osd_log_ctx **ctx, const int log_priority,
                       osd_log_fn log_fn)
{
    struct osd_log_ctx *c;

    c = calloc(1, sizeof(struct osd_log_ctx));
    assert(c);

    c->log_fn = log_fn;

    if (!log_priority) {
        c->log_priority = LOG_PRIORITY_DEFAULT;
    } else {
        c->log_priority = log_priority;
    }

    pthread_mutex_init(&c->lock, NULL);

    *ctx = c;
    return OSD_OK;
}

API_EXPORT
void osd_log_free(struct osd_log_ctx **ctx_p)
{
    assert(ctx_p);

    struct osd_log_ctx *ctx;
    ctx = *ctx_p;
    if (!ctx) {
        return;
    }

    pthread_mutex_destroy(&ctx->lock);
    free(ctx);

    *ctx_p = NULL;
}

API_EXPORT
void osd_log_set_fn(struct osd_log_ctx *ctx, osd_log_fn log_fn)
{
    ctx->log_fn = log_fn;
}

API_EXPORT
int osd_log_get_priority(struct osd_log_ctx *ctx)
{
    return ctx->log_priority;
}

API_EXPORT
void osd_log_set_priority(struct osd_log_ctx *ctx, int priority)
{
    ctx->log_priority = priority;
}

API_EXPORT
void osd_log_set_caller_ctx(struct osd_log_ctx *ctx, void *caller_ctx)
{
    ctx->caller_ctx = caller_ctx;
}

API_EXPORT
void* osd_log_get_caller_ctx(struct osd_log_ctx *ctx)
{
    return ctx->caller_ctx;
}
