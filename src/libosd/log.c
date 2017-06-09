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
};

/**
 * Log a message
 *
 * This calls the registered logging function to output (or possibly discard)
 * the log message.
 *
 * Don't use this function directly, use the dbg(), info() and err() macros
 * instead, which fill in all details for you (e.g. file name, line number,
 * etc.).
 *
 * @param ctx      the log context
 * @param priority the priority of the log message
 * @param file     the file the log message originates from (use __FILE__)
 * @param line     the line number the message originates from
 * @param fn       the C function the message originates from
 * @param format   the format string of the message (as used in printf() and
 *                 friends)
 *
 * @see dbg()
 * @see info()
 * @see err()
 */
void osd_log(struct osd_log_ctx *ctx,
             int priority, const char *file, int line, const char *fn,
             const char *format, ...)
{
    va_list args;

    if (!ctx->log_fn) {
        return;
    }

    va_start(args, format);
    ctx->log_fn(ctx, priority, file, line, fn, format, args);
    va_end(args);
}

/**
 * Create a new instance of osd_log_ctx
 *
 * @param ctx the logging context
 * @param log_priority filter: only log messages greater or equal the given
 *                     priority. Use on the LOG_* constants in stdlog.h
 * @param log_fn logging callback. Set to NULL to disable logging output.
 *
 * @see osd_log_set_priority()
 * @see osd_log_set_fn()
 */
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

    *ctx = c;
    return OSD_OK;
}

/**
 * Free an osd_log_ctx object
 *
 * Don't use the @p ctx pointer any more after calling this function.
 *
 * @param ctx    the log context
 */
API_EXPORT
void osd_log_free(struct osd_log_ctx *ctx)
{
    free(ctx);
}

/**
 * Set logging function
 *
 * The built-in logging writes to STDERR. It can be overridden by a custom
 * function to log messages into the user's logging functionality.
 *
 * In many cases you want the log message to be associated with a context or
 * object of your application, i.e. the object that uses OSD. In this
 * case, set the context or <code>this</code> pointer with
 * osd_log_set_caller_ctx() and retrieve it inside your @p log_fn.
 *
 * An example in C++ could look like this:
 * @code{.cpp}
 * static void MyClass::osdLogCallback(struct osd_log_ctx *gctx,
 *                                     int priority, const char *file,
 *                                     int line, const char *fn,
 *                                     const char *format, va_list args)
 * {
 *   MyClass *myclassptr = static_cast<MyClass*>(osd_get_caller_ctx(gctx));
 *   myclassptr->doLogging(format, args);
 * }
 *
 * MyClass::MyClass()
 * {
 *   // ...
 *   osd_log_set_caller_ctx(gctx, this);
 *   osd_log_set_fn(&MyClass::osdLogCallback);
 *   // ...
 * }
 *
 * MyClass::doLogging(const char* format, va_list args)
 * {
 *    printf("this = %p", this);
 *    vprintf(format, args);
 * }
 * @endcode
 *
 *
 * @param ctx    the log context
 * @param log_fn the used logging function
 */
API_EXPORT
void osd_log_set_fn(struct osd_log_ctx *ctx, osd_log_fn log_fn)
{
    ctx->log_fn = log_fn;
}

/**
 * Get the logging priority
 *
 * The logging priority is the lowest message type that is reported.
 *
 * @param ctx the log context
 * @return the log priority
 *
 * @see osd_log_set_priority()
 */
API_EXPORT
int osd_log_get_priority(struct osd_log_ctx *ctx)
{
    return ctx->log_priority;
}

/**
 * Set the logging priority
 *
 * The logging priority is the lowest message type that is reported.
 *
 * Allowed values for @p priority are <code>LOG_DEBUG</code>,
 * <code>LOG_INFO</code> and <code>LOG_ERR</code> as defined in
 * <code>syslog.h</code>.
 *
 * For example setting @p priority will to <code>LOG_INFO</code> will result in
 * all error and info messages to be shown, and all debug messages to be
 * discarded.
 *
 * @param ctx       the log context
 * @param priority  new priority value
 *
 * @see osd_log_get_priority()
 */
API_EXPORT
void osd_log_set_priority(struct osd_log_ctx *ctx, int priority)
{
    ctx->log_priority = priority;
}

/**
 * Set a caller context pointer
 *
 * This library does not use this pointer in any way, you're free to set it to
 * whatever your application needs.
 *
 * @param ctx        the log context
 * @param caller_ctx the caller context pointer
 *
 * @see osd_log_get_caller_ctx()
 * @see osd_log_set_fn() for a code example using this functionality
 */
API_EXPORT
void osd_log_set_caller_ctx(struct osd_log_ctx *ctx, void *caller_ctx)
{
    ctx->caller_ctx = caller_ctx;
}

/**
 * Get the caller context pointer
 *
 * @param ctx the log context
 * @return the caller context pointer
 *
 * @see osd_log_set_caller_ctx()
 */
API_EXPORT
void* osd_log_get_caller_ctx(struct osd_log_ctx *ctx)
{
    return ctx->caller_ctx;
}
