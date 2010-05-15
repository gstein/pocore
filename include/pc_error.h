/*
  pc_error.h :  error handling primitives

  ====================================================================
     Licensed to the Apache Software Foundation (ASF) under one
     or more contributor license agreements.  See the NOTICE file
     distributed with this work for additional information
     regarding copyright ownership.  The ASF licenses this file
     to you under the Apache License, Version 2.0 (the
     "License"); you may not use this file except in compliance
     with the License.  You may obtain a copy of the License at
    
       http://www.apache.org/licenses/LICENSE-2.0
    
     Unless required by applicable law or agreed to in writing,
     software distributed under the License is distributed on an
     "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
     KIND, either express or implied.  See the License for the
     specific language governing permissions and limitations
     under the License.
  ====================================================================
*/

#ifndef PC_ERROR_H
#define PC_ERROR_H

#include "pc_types.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* ### design for error codes?
   ###
   ### the APR system seems a little broken because two higher-level
   ### libraries may choose conflicting error spaces. an application
   ### certainly knows about the pocore error space and each of the
   ### libraries that it uses. but the two libraries are independent,
   ### and may conflict.  */

/* ### what to do with error->separate?  */

#define PC_SUCCESS 0
#define PC_ERR_TRACE  100  /* ### figure out numbering scheme  */


/* Create an error object, associated with CTX.  */
#define pc_error_create(ctx, code, msg) \
    pc__error_create_internal(ctx, code, msg, __FILE__, __LINE__)


/* Create an error object, associated with the context implied by POOL.  */
#define pc_error_create_via(pool, code, msg) \
    pc__error_create_internal_via(pool, code, msg, __FILE__, __LINE__)


/* Wrap an error with further information.  */
#define pc_error_wrap(code, msg, original) \
    pc__error_wrap_internal(code, msg, original, __FILE__, __LINE__)


/* Add a stacktrace wrapper.  */
#define pc_error_trace(original) \
    pc__error_wrap_internal(PC_ERR_TRACE, NULL, original, __FILE__, __LINE__)


/* Mark ERROR as handled, along with all of its associated and wrapped
   errors. ERROR will be unusable after this call.  */
void pc_error_handled(pc_error_t *error);


/* Return ERROR's useful code value. Any tracing errors will be skipped until
   a non-tracing error is located in the ORIGINAL chain. If there are no
   errors (which should not happen), then PC_SUCCESS will be returned.  */
int pc_error_code(const pc_error_t *error);


/* Return the useful message associated with ERROR. This will live as long
   as ERROR lives. If no message was associated with ERROR at creation time,
   this may be a default message, if available. The message may be NULL
   if nothing was associated and no default is available.

   Note that tracing errors will be skipped -- this message will come from
   the first useful (non-tracing) error found on the ORIGINAL chain.  */
const char *pc_error_message(const pc_error_t *error);


/* Get the ORIGINAL error that ERROR is wrapping. NULL is returned if there
   is no ORIGINAL error. Tracing errors will be skipped.  */
pc_error_t *pc_error_original(const pc_error_t *error);


/* For producing tracebacks, this will return the FILE and LINENO for an
   error, including traceback information. *ORIGINAL will provide the next
   step in the chain, and will be set to NULL when no further frames are
   present.  */
void pc_error_trace_info(const char **file,
                         int *lineno,
                         const pc_error_t **original,
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
pc_error_t *pc__error_wrap_internal(int code,
                                    const char *msg,
                                    pc_error_t *original,
                                    const char *file,
                                    int lineno);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* PC_ERROR_H */
