/*
  pc_error.h :  error handling primitives

  ====================================================================
    Copyright 2010 Greg Stein

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

#ifndef PC_ERROR_H
#define PC_ERROR_H

#include "pc_types.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Symbolic constant for "no error occurred".  */
#define PC_NO_ERROR NULL

/* ### design for error codes?
   ###
   ### the APR system seems a little broken because two higher-level
   ### libraries may choose conflicting error spaces. an application
   ### certainly knows about the pocore error space and each of the
   ### libraries that it uses. but the two libraries are independent,
   ### and may conflict.  */

#define PC_SUCCESS 0
#define PC_ERR_TRACE  100  /* ### figure out numbering scheme  */
#define PC_ERR_IMPROPER_UNHANDLED_CALL 101
#define PC_ERR_IMPROPER_WRAP 102
#define PC_ERR_IMPROPER_DEREGISTER 103
#define PC_ERR_IMPROPER_CLEANUP 104
#define PC_ERR_NOT_REGISTERED 105
#define PC_ERR_UNSPECIFIED_OS 106
#define PC_ERR_IMPROPER_REENTRY 107


/* Create an error object, associated with CTX.  */
#define pc_error_create(ctx, code, msg) \
    pc__error_create_internal(ctx, code, msg, __FILE__, __LINE__)


/* Create an error object, associated with the context implied by POOL.  */
#define pc_error_create_via(pool, code, msg) \
    pc__error_create_internal_via(pool, code, msg, __FILE__, __LINE__)


/* Create an error object, associated with CTX.  */
#define pc_error_createf(ctx, code, format, ...)  \
    pc__error_createf_internal(ctx, code, format, \
                               __FILE__, __LINE__, __VA_ARGS__)


/* Create an error object, associated with the context implied by POOL.  */
#define pc_error_createf_via(pool, code, format, ...)  \
    pc__error_createf_internal_via(pool, code, format, \
                                   __FILE__, __LINE__, __VA_ARGS__)


/* Wrap an error with further information. @a original must not be NULL.  */
#define pc_error_wrap(code, msg, original) \
    pc__error_wrap_internal(code, msg, original, __FILE__, __LINE__)


/* Join SEPARATE onto ERROR. This is used when SEPARATE occurs while
   processing ERROR, and both errors need to be returned to the caller.
   SEPARATE will be stored in a path of errors, distinct from the
   "original" chain.

   ERROR will be returned, wrapped in a PC_ERR_TRACE error to indicate
   where the two errors were joined.

   If ERROR is NULL, then SEPARATE will be returned (with the trace wrapper).
   If SEPARATE is NULL, then ERROR will be returned (with the trace wrapper).
   If ERROR and SEPARATE are NULL, then NULL is returned.
*/
#define pc_error_join(error, separate) \
    pc__error_join_internal(error, separate, __FILE__, __LINE__)


/* Add a stacktrace wrapper, if the context has tracing enabled. If
   ORIGINAL is NULL, then NULL will be returned.  */
#define pc_error_trace(original) \
    pc__error_trace_internal(original, __FILE__, __LINE__)


/* Mark ERROR as handled, along with all of its wrapped and joined
   errors. ERROR will be unusable after this call. ERROR may be NULL.  */
void pc_error_handled(pc_error_t *error);


/* Return ERROR's useful code value. Any tracing errors will be skipped until
   a non-tracing error is located in the ORIGINAL chain. If ERROR is NULL,
   or there are no errors (which should not happen), then PC_SUCCESS will
   be returned.  */
int pc_error_code(const pc_error_t *error);


/* Return the useful message associated with ERROR. This will live as long
   as ERROR lives. If no message was associated with ERROR at creation time,
   this may be a default message, if available. The message may be NULL
   if nothing was associated and no default is available.

   Note that tracing errors will be skipped -- this message will come from
   the first useful (non-tracing) error found on the ORIGINAL chain. If ERROR
   is NULL or no original error is found, then NULL will be returned.  */
const char *pc_error_message(const pc_error_t *error);


/* Get the ORIGINAL error that ERROR is wrapping. If ERROR is NULL, then
   NULL is returned. If there is no ORIGINAL error (which shouldn't happen,
   but is possible if the very first error raised is PC_ERR_TRACE).

   Tracing errors will be skipped.  */
pc_error_t *pc_error_original(const pc_error_t *error);


/* Get the SEPARATE error that is associated with ERROR. NULL is returned
   if ERROR is NULL, there is no original error, or if there is no
   SEPARATE error.

   Tracing errors will be skipped on ERROR, and on SEPARATE before it is
   returned.  */
pc_error_t *pc_error_separate(const pc_error_t *error);


/* For producing tracebacks, this will return the FILE and LINENO for an
   error, including traceback information. *ORIGINAL will provide the next
   step in the chain, and will be set to NULL when no further frames are
   present. *SEPARATE will provide the associated error from this frame,
   without skipping trace errors (like pc_error_separate does).  */
void pc_error_trace_info(const char **file,
                         int *lineno,
                         const pc_error_t **original,
                         const pc_error_t **separate,
                         const pc_error_t *error);


/* Internal constructors. Use pc_error_create() and friends, instead.  */
pc_error_t *pc__error_create_internal(pc_context_t *ctx,
                                      int code,
                                      const char *msg,
                                      const char *file,
                                      int lineno);
pc_error_t *pc__error_create_internal_via(pc_pool_t *pool,
                                          int code,
                                          const char *msg,
                                          const char *file,
                                          int lineno);
pc_error_t *pc__error_createf_internal(pc_context_t *ctx,
                                       int code,
                                       const char *format,
                                       const char *file,
                                       int lineno,
                                       ...)
        __attribute__((format(printf, 3, 6)));
pc_error_t *pc__error_createf_internal_via(pc_pool_t *pool,
                                           int code,
                                           const char *format,
                                           const char *file,
                                           int lineno,
                                           ...)
        __attribute__((format(printf, 3, 6)));
pc_error_t *pc__error_wrap_internal(int code,
                                    const char *msg,
                                    pc_error_t *original,
                                    const char *file,
                                    int lineno);
pc_error_t *pc__error_join_internal(pc_error_t *error,
                                    pc_error_t *separate,
                                    const char *file,
                                    int lineno);
pc_error_t *pc__error_trace_internal(pc_error_t *error,
                                     const char *file,
                                     int lineno);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* PC_ERROR_H */
