/*
  pc_misc.h :  miscellaneous portability support

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

#ifndef PC_MISC_H
#define PC_MISC_H

/* ### is this always the correct header?  */
#include <stdlib.h>  /* for size_t  */

#include "pc_types.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define PC_DEFAULT_STDSIZE 0

pc_context_t *pc_context_create(void);

pc_context_t *pc_context_create_custom(size_t stdsize,
                                       int (*oom_handler)(size_t amt));

void pc_context_destroy(pc_context_t *ctx);


/* Return an unhandled error that was saved in CTX. Returns NULL if there
   are no unhandled errors.  */
pc_error_t *pc_context_unhandled(pc_context_t *ctx);


/* ### uuid  */


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* PC_MISC_H */
