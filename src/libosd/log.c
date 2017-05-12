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

#include <osd.h>
#include "osd-private.h"

#include <syslog.h>
#include <stdlib.h>
#include <errno.h>

/**
 * Logging context
 */
struct osd_log_ctx {
    /** logging function */
    osd_log_fn log_fn;
    /** logging priority */
    int log_priority;
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
 * @param ctx      the library context
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
 * @param log_fn logging callback
 *
 * @see osd_log_set_priority()
 * @see osd_log_set_fn()
 */
osd_result osd_log_new(struct osd_log_ctx **ctx, int log_priority,
                       osd_log_fn log_fn)
{
    struct osd_log_ctx *c;

    c = calloc(1, sizeof(struct osd_log_ctx));
    if (!c)
        return -ENOMEM;

    c->log_fn = log_fn;
    c->log_priority = log_priority;

    *ctx = c;
    return OSD_RV_SUCCESS;
}

/**
 * Set logging function
 *
 * The built-in logging writes to STDERR. It can be overridden by a custom
 * function to log messages into the user's logging functionality.
 *
 * In many cases you want the log message to be associated with a context or
 * object of your application, i.e. the object that uses OSD. In this
 * case, set the context or <code>this</code> pointer with osd_set_caller_ctx()
 * and retrieve it inside your @p log_fn.
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
 *   osd_set_caller_ctx(gctx, this);
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
 * @param ctx    the library context
 * @param log_fn the used logging function
 *
 * @ingroup log
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
 * @param ctx the library context
 * @return the log priority
 *
 * @see osd_log_set_priority()
 *
 * @ingroup log
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
 * @param ctx       the library context
 * @param priority  new priority value
 *
 * @see osd_log_get_priority()
 *
 * @ingroup log
 */
API_EXPORT
void osd_log_set_priority(struct osd_log_ctx *ctx, int priority)
{
    ctx->log_priority = priority;
}
