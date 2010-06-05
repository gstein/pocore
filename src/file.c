/*
  file.c :  implementation of PoCore's file/path functions

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

#include "pc_types.h"
#include "pc_file.h"
#include "pc_track.h"

#include "pocore.h"
#include "pocore_platform.h"

#ifndef PC__IS_WINDOWS
#include <fcntl.h>
#include <unistd.h>
#endif

/* Our basic file structure.

   It's Windows, or it's POSIX. Pretty simple for now.  */
struct pc_file_s {
#ifdef PC__IS_WINDOWS
    HANDLE handle;
#else
    int fd;
#endif

    pc_bool_t delclose;
    const char *path;  /* only when DELCLOSE.  */
    pc_pool_t *pool;  /* only when DELCLOSE.  */
};


pc_error_t *pc_file_create(pc_file_t **new_file,
                           const char *path,
                           int flags,
                           pc_pool_t *pool)
{
    *new_file = pc_calloc(pool, sizeof(**new_file));

#ifdef PC__IS_WINDOWS
    NOT_IMPLEMENTED();
#else
    int mode = flags & PC_FILE_OPEN_MODEMASK;
    int openflags;

    switch (mode)
    {
    case PC_FILE_OPEN_READ:
        openflags = O_RDONLY;
        break;

    case PC_FILE_OPEN_WRITE:
    case PC_FILE_OPEN_APPEND:
        openflags = O_RDWR;
        break;

    case PC_FILE_OPEN_TRUNCATE:
        openflags = O_RDWR | O_TRUNC;
        break;

    case PC_FILE_OPEN_CREATE:
        openflags = O_RDWR | O_CREAT;
        break;

    case PC_FILE_OPEN_EXCL:
    case PC_FILE_OPEN_DELCLOSE:
        openflags = O_RDWR | O_CREAT | O_EXCL;
        break;

    default:
        NOT_IMPLEMENTED();
    }

    (*new_file)->fd = open(path, openflags, 0777);
    if ((*new_file)->fd == -1)
    {
        NOT_IMPLEMENTED();
    }

    if (mode == PC_FILE_OPEN_APPEND)
    {
        NOT_IMPLEMENTED();
    }
    else if (mode == PC_FILE_OPEN_DELCLOSE)
    {
        (*new_file)->delclose = TRUE;
        (*new_file)->path = pc_strdup(pool, path);
        (*new_file)->pool = pool;
    }
#endif

    return NULL;
}


void pc_file_destroy(pc_file_t *file)
{
    /* ### do anything to avoid double-close?  */

#ifdef PC__IS_WINDOWS
    CloseHandle(file->handle);
#else
    close(file->fd);
#endif

    if (file->delclose)
    {
        /* ### handle the error return  */
        (void) pc_path_remove(file->path, TRUE, file->pool);
    }
}


static void cleanup_file(void *tracked)
{
    pc_file_destroy(tracked);
}


void pc_file_track(const pc_file_t *file, pc_context_t *ctx)
{
    pc_track(ctx, file, cleanup_file);
}


void pc_file_track_via(const pc_file_t *file, pc_pool_t *pool)
{
    pc_track_via(pool, file, cleanup_file);
}


void pc_file_owns(const pc_file_t *file, pc_pool_t *pool)
{
    pc_track_via(pool, file, cleanup_file);
    pc_track_owns_pool(file, pool);
}


pc_error_t *pc_file_read(size_t *amt_read,
                         pc_file_t *file,
                         void *buf,
                         size_t amt,
                         pc_pool_t *pool)
{
#ifdef PC__IS_WINDOWS
    DWORD actual;

    if (!ReadFile(file->handle, buf, amt, &actual, NULL))
    {
        /* ### return error.  */
        NOT_IMPLEMENTED();
    }

    *amt_read = actual;
#else
    ssize_t actual;

    actual = read(file->fd, buf, amt);
    if (actual == -1)
    {
        /* ### return error.  */
        NOT_IMPLEMENTED();
    }

    *amt_read = actual;
#endif

    return NULL;
}


pc_error_t *pc_file_write(size_t *amt_written,
                          pc_file_t *file,
                          const void *buf,
                          size_t amt,
                          pc_pool_t *pool)
{
#ifdef PC__IS_WINDOWS
    DWORD actual;

    if (!WriteFile(file->handle, buf, amt, &actual, NULL))
    {
        /* ### return error.  */
        NOT_IMPLEMENTED();
    }

    *amt_written = actual;
#else
    ssize_t actual;

    actual = write(file->fd, buf, amt);
    if (actual == -1)
    {
        /* ### return error.  */
        NOT_IMPLEMENTED();
    }

    *amt_written = actual;
#endif

    return NULL;
}


pc_error_t *pc_file_get_position(int64_t *position,
                                 pc_file_t *file,
                                 pc_pool_t *pool)
{
    NOT_IMPLEMENTED();
}


pc_error_t *pc_file_set_position(pc_file_t *file,
                                 int64_t position,
                                 pc_pool_t *pool)
{
    NOT_IMPLEMENTED();
}
