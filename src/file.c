/*
  file.c :  implementation of PoCore's file functions

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
#include "pc_cleanup.h"

#include "pocore.h"
#include "pocore_platform.h"

#ifndef PC__IS_WINDOWS
#include <fcntl.h>
#include <unistd.h>
#endif

/* Our basic file structure.

   It's Windows, or it's POSIX. Pretty simple for now.  */
struct pc_file_s {
    /* We remember the pool this file was allocated within. This is needed
       for cleanup registration and for error handling.  */
    pc_pool_t *pool;

#ifdef PC__IS_WINDOWS
    HANDLE handle;
#else
    int fd;
#endif

    pc_bool_t delclose;
    const char *path;  /* only when DELCLOSE.  */
};


/* Set up a singleton for detecting already-closed files.  */
static const char file_is_closed;
#define FILE_CLOSED_MARK (&file_is_closed)
#define FILE_CLOSED(file) ((file)->path == FILE_CLOSED_MARK)
#define MARK_CLOSED(file) ((file)->path = FILE_CLOSED_MARK)

/* ### rearrange functions in this file.  */
static void cleanup_file(void *data);


pc_error_t *pc_file_create(pc_file_t **new_file,
                           const char *path,
                           int flags,
                           pc_pool_t *pool)
{
    *new_file = pc_calloc(pool, sizeof(**new_file));
    (*new_file)->pool = pool;

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
        return pc_error_trace(pc__convert_os_error(pool->memroot->ctx));
    }

    if (mode == PC_FILE_OPEN_APPEND)
    {
        NOT_IMPLEMENTED();
    }
    else if (mode == PC_FILE_OPEN_DELCLOSE)
    {
        (*new_file)->delclose = TRUE;
        (*new_file)->path = pc_strdup(pool, path);
    }
#endif

    /* Register a cleanup to close the file.  */
    pc_cleanup_register(pool, *new_file, cleanup_file);

    return PC_NO_ERROR;
}


static void
close_file(pc_file_t *file)
{
    if (FILE_CLOSED(file))
    {
        /* ### mark an unhandled error to note the double-close  */
        return;
    }
    MARK_CLOSED(file);

#ifdef PC__IS_WINDOWS
    CloseHandle(file->handle);
#else
    close(file->fd);
#endif

    if (file->delclose)
    {
        /* ### is there a better way to deal with any errors during this
           ### removal? strange things can go wrong, and leaving behind
           ### an unhandled error doesn't help much. unhandled errors are
           ### more about programming errors, than runtime problems.
           ### maybe the right answer is to make users handle the removal
           ### themselves. they can then take appropriate action if a
           ### runtime problem arises.  */
        pc_error_handled(pc_path_remove(file->path, TRUE, file->pool));
    }
}


void pc_file_destroy(pc_file_t *file)
{
    pc_cleanup_run(file->pool, file);
}


static void cleanup_file(void *data)
{
    close_file(data);
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
        return pc_error_trace(pc__convert_os_error(file->pool->memroot->ctx));
    }

    *amt_read = actual;
#else
    ssize_t actual;

    actual = read(file->fd, buf, amt);
    if (actual == -1)
    {
        return pc_error_trace(pc__convert_os_error(file->pool->memroot->ctx));
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
        return pc_error_trace(pc__convert_os_error(file->pool->memroot->ctx));
    }

    *amt_written = actual;
#else
    ssize_t actual;

    actual = write(file->fd, buf, amt);
    if (actual == -1)
    {
        return pc_error_trace(pc__convert_os_error(file->pool->memroot->ctx));
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


void pc_file_shift(pc_file_t *file,
                   pc_pool_t *new_pool)
{
    NOT_IMPLEMENTED();
}
