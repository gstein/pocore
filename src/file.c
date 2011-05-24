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
    /* We remember the context this file is associated with. This is
       primarily for tracking, but is also needed if we need to create
       an unhandled error.  */
    pc_context_t *ctx;

#ifdef PC__IS_WINDOWS
    HANDLE handle;
#else
    int fd;
#endif

    pc_bool_t delclose;
    const char *path;  /* only when DELCLOSE.  */
    pc_pool_t *pool;  /* only when DELCLOSE.  */
};


/* Set up a singleton for detecting already-closed files.  */
static const char file_is_closed;
#define FILE_CLOSED_MARK (&file_is_closed)
#define FILE_CLOSED(file) ((file)->path == FILE_CLOSED_MARK)
#define MARK_CLOSED(file) ((file)->path = FILE_CLOSED_MARK)


pc_error_t *pc_file_create(pc_file_t **new_file,
                           const char *path,
                           int flags,
                           pc_pool_t *pool)
{
    *new_file = pc_calloc(pool, sizeof(**new_file));
    (*new_file)->ctx = pool->ctx;

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
        return pc_error_trace(pc__convert_os_error((*new_file)->ctx));
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
    /* The file is getting destroyed, so we are no longer an owner of
       anything (such as the pool it is allocated within). Note that
       it is acceptable to call deregister for untracked items.  */
    pc_track_deregister(file->ctx, file);

    close_file(file);
}


static void cleanup_file(void *tracked)
{
    close_file(tracked);
}


void pc_file_track(const pc_file_t *file, pc_context_t *ctx)
{
    /* ### validate ctx == file->ctx ?  */
    pc_track(ctx, file, cleanup_file);
}


void pc_file_track_via(const pc_file_t *file, pc_pool_t *pool)
{
    /* ### validate pool->ctx == file->ctx ?  */
    pc_track_via(pool, file, cleanup_file);
}


void pc_file_owns(const pc_file_t *file, pc_pool_t *pool)
{
    /* ### validate pool->ctx == file->ctx ?  */
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
        return pc_error_trace(pc__convert_os_error(file->ctx));
    }

    *amt_read = actual;
#else
    ssize_t actual;

    actual = read(file->fd, buf, amt);
    if (actual == -1)
    {
        return pc_error_trace(pc__convert_os_error(file->ctx));
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
        return pc_error_trace(pc__convert_os_error(file->ctx));
    }

    *amt_written = actual;
#else
    ssize_t actual;

    actual = write(file->fd, buf, amt);
    if (actual == -1)
    {
        return pc_error_trace(pc__convert_os_error(file->ctx));
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
