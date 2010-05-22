/*
  error.c :  PoCore error handling

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

#include <string.h>
#include <stdint.h>

#include "pc_types.h"
#include "pc_memory.h"
#include "pc_error.h"

#include "pocore.h"


static const pc_error_t *
scan_useful(const pc_error_t *error)
{
    while (error != NULL && error->code == PC_ERR_TRACE)
        error = error->original;
    return error;
}


static pc_error_t *
create_error(pc_context_t *ctx,
             int code,
             const char *msg,
             const char *file,
             int lineno,
             pc_error_t *original)
{
    /* ### need to prepare error_pool  */

    pc_error_t *error = pc_alloc(ctx->error_pool, sizeof(*error));

    error->ctx = ctx;
    error->code = code;
    error->msg = msg;
    error->file = file;
    error->lineno = lineno;
    error->original = original;
    error->separate = NULL;

    /* ### hook error into the context  */

    return error;
}


void pc_error_handled(pc_error_t *error)
{
    /* ### mark as handled... */
}


int pc_error_code(const pc_error_t *error)
{
    const pc_error_t *useful = scan_useful(error);

    return useful ? useful->code : PC_SUCCESS;
}


const char *pc_error_message(const pc_error_t *error)
{
    const pc_error_t *useful = scan_useful(error);

    if (useful == NULL)
        return NULL;

    if (useful->msg == NULL)
        return NULL;  /* ### look up default message  */

    return useful->msg;
}


pc_error_t *pc_error_original(const pc_error_t *error)
{
    return (pc_error_t *)scan_useful(error);
}


void pc_error_trace_info(const char **file,
                         int *lineno,
                         const pc_error_t **original,
                         const pc_error_t *error)
{
    *file = error->file;
    *lineno = error->lineno;
    *original = error->original;
}


/* Internal constructors. Use pc_error_create() and friends, instead.  */
pc_error_t *pc__error_create_internal(pc_context_t *ctx,
                                      int code,
                                      const char *msg,
                                      const char *file,
                                      int lineno)
{
    return create_error(ctx, code, msg, file, lineno, NULL);
}


pc_error_t *pc__error_create_internal_via(pc_pool_t *pool,
                                          int code,
                                          const char *msg,
                                          const char *file,
                                          int lineno)
{
    return create_error(pool->ctx, code, msg, file, lineno, NULL);
}


pc_error_t *pc__error_wrap_internal(int code,
                                    const char *msg,
                                    pc_error_t *original,
                                    const char *file,
                                    int lineno)
{
    return create_error(original->ctx, code, msg, file, lineno, original);
}
