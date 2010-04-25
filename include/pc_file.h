/*
  pc_file.h :  file I/O and management primitives

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

#ifndef PC_FILE_H
#define PC_FILE_H

#include "pc_error.h"
#include "pc_memory.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef struct pc_file_s pc_file_t;

/* Create a new file object, returned in *NEW_FILE. It will be opened on
   FILENAME according to FLAGS. The object will be allocated within POOL.

   The file objecct is NOT registered for tracking.  */
pc_error_t *pc_file_create(pc_file_t **new_file,
                           const char *filename,
                           int flags,
                           pc_pool_t *pool);


/* This destroys the file object FILE. This simply closes the file handle,
   rather than destroying/removing a file on-disk.

   Of course, if the file was opened as DEL_ON_CLOSE, then it will be
   removed at this time.  */
void pc_file_destroy(pc_file_t *file);


/* Register FILE in the tracking registry of CTX.  */
void pc_file_track(pc_file_t *file, pc_context_t *ctx);


/* Register FILE in the tracking registry of the context implied by POOL.  */
void pc_file_track_via(pc_file_t *file, pc_pool_t *pool);


/* Track FILE as an owner of POOL so that it will be closed when POOL
   is cleaned-up/destroyed.  */
void pc_file_owns(pc_file_t *file, pc_pool_t *pool);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* PC_FILE_H */
