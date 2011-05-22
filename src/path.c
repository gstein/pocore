/*
  path.c :  implementation of PoCore's path functions

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

#include "pc_types.h"
#include "pc_file.h"

#include "pocore.h"
#include "pocore_platform.h"


pc_error_t *pc_path_move(const char *src_path,
                         const char *dst_path,
                         pc_pool_t *pool)
{
    NOT_IMPLEMENTED();
}


pc_error_t *pc_path_remove(const char *path,
                           pc_bool_t allow_missing,
                           pc_pool_t *pool)
{
    NOT_IMPLEMENTED();
}


pc_error_t *pc_path_mkdir(const char *path, pc_pool_t *pool)
{
    NOT_IMPLEMENTED();
}


pc_error_t *pc_path_rmdir(const char *path, pc_pool_t *pool)
{
    NOT_IMPLEMENTED();
}


pc_error_t *pc_path_listdir(pc_hash_t **entries,
                            const char *path,
                            pc_pool_t *pool)
{
    NOT_IMPLEMENTED();
}


pc_error_t *pc_path_get_readonly(pc_bool_t *readonly,
                                 const char *path,
                                 pc_pool_t *pool)
{
    NOT_IMPLEMENTED();
}


pc_error_t *pc_path_set_readonly(const char *path,
                                 pc_bool_t readonly,
                                 pc_pool_t *pool)
{
    NOT_IMPLEMENTED();
}


pc_error_t *pc_path_get_executable(pc_bool_t *executable,
                                   const char *path,
                                   pc_pool_t *pool)
{
    NOT_IMPLEMENTED();
}


pc_error_t *pc_path_set_executable(const char *path,
                                   pc_bool_t executable,
                                   pc_pool_t *pool)
{
    NOT_IMPLEMENTED();
}


pc_error_t *pc_path_get_hidden(pc_bool_t *hidden,
                               const char *path,
                               pc_pool_t *pool)
{
    NOT_IMPLEMENTED();
}


pc_error_t *pc_path_set_hidden(const char *path,
                               pc_bool_t hidden,
                               pc_pool_t *pool)
{
    NOT_IMPLEMENTED();
}


pc_error_t *pc_path_get_mtime(uint64_t *mtime,
                              const char *path,
                              pc_pool_t *pool)
{
    NOT_IMPLEMENTED();
}


pc_error_t *pc_path_set_mtime(const char *path,
                              uint64_t mtime,
                              pc_pool_t *pool)
{
    NOT_IMPLEMENTED();
}


pc_error_t *pc_path_actual(char **actual,
                           const char *path,
                           pc_pool_t *pool)
{
    NOT_IMPLEMENTED();
}


pc_error_t *pc_path_stat(pc_path_info_t *info,
                         const char *path,
                         pc_pool_t *pool)
{
    NOT_IMPLEMENTED();
}


int pc_path_handling(pc_context_t *ctx, const char *path)
{
    NOT_IMPLEMENTED();
}


const char *pc_path_encoding(pc_context_t *ctx, const char *path)
{
    NOT_IMPLEMENTED();
}
