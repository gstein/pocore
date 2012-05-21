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
   ### and may conflict.
   ###
   ### see draft of error mappers below.  */

#define PC_SUCCESS 0

#define PC_ERR_TRACE  PC__ERR_VALUE(0)
#define PC_ERR_IMPROPER_UNHANDLED_CALL  PC__ERR_VALUE(1)
#define PC_ERR_IMPROPER_WRAP  PC__ERR_VALUE(2)
#define PC_ERR_IMPROPER_DEREGISTER  PC__ERR_VALUE(3)
#define PC_ERR_IMPROPER_CLEANUP  PC__ERR_VALUE(4)
#define PC_ERR_NOT_REGISTERED  PC__ERR_VALUE(5)
#define PC_ERR_UNSPECIFIED_OS  PC__ERR_VALUE(6)
#define PC_ERR_IMPROPER_REENTRY  PC__ERR_VALUE(7)
#define PC_ERR_ADDRESS_LOOKUP  PC__ERR_VALUE(8)
#define PC_ERR_BAD_PARAM  PC__ERR_VALUE(9)

/* Private macros for computing the builtin error values.  */
#define PC__ERR_BASE  10000
#define PC__ERR_VALUE(code)  (PC__ERR_BASE + (code))

/* Error number mapping.  */
typedef struct pc_errmap_s pc_errmap_t;

#define PC_ERR_MAPPING (-1)

#define PC_ERR_DEFAULT_NS ("pc")


typedef const char *(*pc_errmap_message_cb)(int code,
                                            void *baton);

/* Construct an error code mapper for this namespace. This will have the
   same lifetime as CTX. If a mapper for NAMESPACE was created previously,
   then it will be returned.

   If MESSAGE_CB is not NULL, then it will be called to provide a default
   message for a specified (local) error codel  */
const pc_errmap_t *pc_errmap_register(pc_context_t *ctx,
                                      const char *namespace,
                                      pc_errmap_message_cb message_cb,
                                      void *message_baton);

/* Map a global error value to a local error code. If the value is not
   within the namespace defined by this error map, then PC_ERR_MAPPING
   is returned.  */
int pc_errmap_code(const pc_errmap_t *errmap,
                   int errval);

/* Map a local error code to a global error value. If CODE is PC_SUCCESS,
   then PC_SUCCESS will be returned. If CODE is less than zero, then
   PC_ERR_MAPPING will be returned.  */
int pc_errmap_errval(const pc_errmap_t *errmap,
                     int code);

/* Return the namespace associated with this global error value. If no
   namespace exists for this value, then NULL is returned. The standard
   errors will return PC_ERR_DEFAULT_NS.  */
const char *pc_errmap_namespace(const pc_context_t *ctx,
                                int errval);

/* Map a global error value to a local error code.  */
int pc_errmap_code_any(const pc_context_t *ctx,
                       int errval);

/* Return the pocore context associated with the given error map.  */
pc_context_t *pc_errmap_context(const pc_errmap_t *errmap);


/* ### docco for all the functions below  */

#define pc_error_create_e(emap, code) \
    pc__error_create_internal_e(emap, code, __FILE__, __LINE__)
#define pc_error_create_xn(ctx, ns, code) \
    pc__error_create_internal_xn(ctx, ns, code, __FILE__, __LINE__)
#define pc_error_create_pn(pool, ns, code) \
    pc__error_create_internal_pn(pool, ns, code, __FILE__, __LINE__)

#define pc_error_create_em(emap, code, msg) \
    pc_error_createf_e(emap, code, "%s", msg)
#define pc_error_create_xnm(ctx, ns, code, msg) \
    pc_error_createf_xn(ctx, ns, code, "%s", msg)
#define pc_error_create_pnm(pool, ns, code, msg) \
    pc_error_createf_pn(pool, ns, code, "%s", msg)

#define pc_error_createf_e(emap, code, format, ...) \
    pc__error_createf_internal_e(emap, code, format, \
                                 __FILE__, __LINE__, __VA_ARGS__)
#define pc_error_createf_xn(ctx, ns, code, format, ...) \
    pc__error_createf_internal_xn(ctx, ns, code, format, \
                                  __FILE__, __LINE__, __VA_ARGS__)
#define pc_error_createf_pn(pool, ns, code, format, ...) \
    pc__error_createf_internal_pn(pool, ns, code, format, \
                                  __FILE__, __LINE__, __VA_ARGS__)


/* This group takes global error values, which is typically not as
   convenient for applications.  */
#define pc_error_create_x(ctx, errval) \
    pc__error_create_internal_xn(ctx, NULL, errval, __FILE__, __LINE__)
#define pc_error_create_p(pool, errval) \
    pc__error_create_internal_pn(ctx, NULL, errval, __FILE__, __LINE__)
#define pc_error_create_xm(ctx, errval, msg) \
    pc_error_createf_x(ctx, errval, "%s", msg)
#define pc_error_create_pm(pool, errval, msg) \
    pc_error_createf_p(pool, errval, "%s", msg)
#define pc_error_createf_x(ctx, errval, format, ...) \
    pc__error_createf_internal_xn(ctx, NULL, errval, format, \
                                  __FILE__, __LINE__, __VA_ARGS__)
#define pc_error_createf_p(pool, errval, format, ...) \
    pc__error_createf_internal_pn(pool, NULL, errval, format, \
                                  __FILE__, __LINE__, __VA_ARGS__)


/* Annotate an error with further information by inserting a PC_ERR_TRACE
   with a custom message. If ERROR is NULL, then NULL is returned.  */
#define pc_error_annotate(msg, error) \
    pc__error_annotate_internal(msg, error, __FILE__, __LINE__)


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
    pc__error_trace_internal((original), __FILE__, __LINE__)


/* Mark ERROR as handled, along with all of its wrapped and joined
   errors. ERROR will be unusable after this call. ERROR may be NULL.  */
void pc_error_handled(pc_error_t *error);


/* Return ERROR's useful local error code. Any tracing errors will be
   skipped until a non-tracing error is located in the ORIGINAL chain.
   If ERROR is NULL, or there are no errors (which should not happen),
   then PC_SUCCESS will be returned.  */
int pc_error_code(const pc_error_t *error);


/* Return ERROR's useful global error value. Any tracing errors will be
   skipped until a non-tracing error is located in the ORIGINAL chain.
   If ERROR is NULL, orthere are no errors (which should not happen),
   then PC_SUCCESS will be returned.  */
int pc_error_errval(const pc_error_t *error);


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
   but is possible if the very first error raised is PC_ERR_TRACE), then
   NULL will be returned.

   Tracing errors will be skipped.  */
pc_error_t *pc_error_original(const pc_error_t *error);


/* Get the SEPARATE error that is associated with ERROR. NULL is returned
   if ERROR is NULL, there is no original error, or if there is no
   SEPARATE error.

   Tracing errors will be skipped on ERROR, and on SEPARATE before it is
   returned.  */
pc_error_t *pc_error_separate(const pc_error_t *error);


/* Return the context this error is associated with.  */
pc_context_t *pc_error_context(const pc_error_t *error);


/* For producing tracebacks, this will return the FILE and LINENO for an
   error, its key information (without skipping trace records), and the
   continuing linked errors. *ORIGINAL will provide the nextstep in the
   chain, and will be set to NULL when no further frames are present.
   *SEPARATE will provide the associated error from this frame, without
   skipping trace errors (like pc_error_separate does).  */
void pc_error_trace_info(const char **file,
                         int *lineno,
                         int *errval,
                         const char **msg,
                         const pc_error_t **original,
                         const pc_error_t **separate,
                         const pc_error_t *error);


/* Internal constructors. Use pc_error_create_*() and friends, instead.  */
pc_error_t *pc__error_create_internal_e(pc_errmap_t *emap,
                                        int code,
                                        const char *file,
                                        int lineno);
pc_error_t *pc__error_create_internal_xn(pc_context_t *ctx,
                                         const char *ns,
                                         int code,
                                         const char *file,
                                         int lineno);
pc_error_t *pc__error_create_internal_pn(pc_pool_t *pool,
                                         const char *ns,
                                         int code,
                                         const char *file,
                                         int lineno);

pc_error_t *pc__error_createf_internal_e(pc_errmap_t *emap,
                                         int code,
                                         const char *format,
                                         const char *file,
                                         int lineno,
                                         ...)
        __attribute__((format(printf, 3, 6)));
pc_error_t *pc__error_createf_internal_xn(pc_context_t *ctx,
                                          const char *ns,
                                          int code,
                                          const char *format,
                                          const char *file,
                                          int lineno,
                                          ...)
        __attribute__((format(printf, 4, 7)));
pc_error_t *pc__error_createf_internal_pn(pc_pool_t *pool,
                                          const char *ns,
                                          int code,
                                          const char *format,
                                          const char *file,
                                          int lineno,
                                          ...)
        __attribute__((format(printf, 4, 7)));

pc_error_t *pc__error_annotate_internal(const char *msg,
                                        pc_error_t *error,
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
