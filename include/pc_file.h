/*
  pc_file.h :  file I/O and management primitives

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

#ifndef PC_FILE_H
#define PC_FILE_H

#include <stdint.h>

#include "pc_error.h"
#include "pc_memory.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef struct pc_file_s pc_file_t;


/* These flag values determine the mode when opening the file.  */
#define PC_FILE_OPEN_MODEMASK 0x000F
#define PC_FILE_OPEN_READ     0x0001 /* for reading.  */
                                     /* the following modes open the file
                                        for writing:  */
#define PC_FILE_OPEN_WRITE    0x0002 /* must exist.  */
#define PC_FILE_OPEN_TRUNCATE 0x0003 /* truncated to 0 length. must exist.  */
#define PC_FILE_OPEN_APPEND   0x0004 /* seek to EOF. must exist. see notes.  */
#define PC_FILE_OPEN_CREATE   0x0005 /* create if needed.  */
#define PC_FILE_OPEN_EXCL     0x0006 /* error if it exists.  */
#define PC_FILE_OPEN_DELCLOSE 0x0007 /* must not exist. delete when closed.  */

/* Other miscellaneous flags.  */
#define PC_FILE_OPEN_TEXT     0x0010 /* text mode (on Windows).  */

/* Note: APPEND will seek to the end of the file once it has been opened
   for writing. This is NOT the same as the POSIX O_APPEND mode. Future
   seeks or other processes could cause a write mid-file.
   ### is this behavior okay? can we do an append-only mode on Windows?  */


/* Create a new file object, returned in *NEW_FILE. It will be opened on
   PATH according to FLAGS. The object will be allocated within POOL.

   The file object is NOT registered for tracking.  */
pc_error_t *pc_file_create(pc_file_t **new_file,
                           const char *path,
                           int flags,
                           pc_pool_t *pool);


/* This destroys the file object FILE. This simply closes the file handle,
   rather than destroying/removing a file on-disk.

   Of course, if the file was opened with DELCLOSE, then it will be
   removed at this time.  */
void pc_file_destroy(pc_file_t *file);


/* Register FILE in the tracking registry of CTX.  */
void pc_file_track(const pc_file_t *file, pc_context_t *ctx);


/* Register FILE in the tracking registry of the context implied by POOL.  */
void pc_file_track_via(const pc_file_t *file, pc_pool_t *pool);


/* Track FILE as an owner of POOL so that it will be closed when POOL
   is cleaned-up/destroyed. Both arguments will be registered for tracking,
   as required.  */
void pc_file_owns(const pc_file_t *file, pc_pool_t *pool);


pc_error_t *pc_file_read(size_t *amt_read,
                         pc_file_t *file,
                         void *buf,
                         size_t amt,
                         pc_pool_t *pool);
pc_error_t *pc_file_write(size_t *amt_written,
                          pc_file_t *file,
                          const void *buf,
                          size_t amt,
                          pc_pool_t *pool);

/* ### use off_t? or always pretend we have a "large" filesystem?  */
pc_error_t *pc_file_get_position(int64_t *position,
                                 pc_file_t *file,
                                 pc_pool_t *pool);
pc_error_t *pc_file_set_position(pc_file_t *file,
                                 int64_t position,
                                 pc_pool_t *pool);


/* ### locking?  */


/*
  ====================================================================

  The following functions operate on paths, rather than file objects.
*/

pc_error_t *pc_path_move(const char *src_path,
                         const char *dst_path,
                         pc_pool_t *pool);
pc_error_t *pc_path_remove(const char *path,
                           pc_bool_t allow_missing,
                           pc_pool_t *pool);
pc_error_t *pc_path_mkdir(const char *path, pc_pool_t *pool);
pc_error_t *pc_path_rmdir(const char *path, pc_pool_t *pool);

pc_error_t *pc_path_listdir(pc_hash_t **entries,
                            const char *path,
                            pc_pool_t *pool);
typedef struct {
    const char *name;
} pc_dirent_t;


pc_error_t *pc_path_get_readonly(pc_bool_t *readonly,
                                 const char *path,
                                 pc_pool_t *pool);
pc_error_t *pc_path_set_readonly(const char *path,
                                 pc_bool_t readonly,
                                 pc_pool_t *pool);

pc_error_t *pc_path_get_executable(pc_bool_t *executable,
                                   const char *path,
                                   pc_pool_t *pool);
pc_error_t *pc_path_set_executable(const char *path,
                                   pc_bool_t executable,
                                   pc_pool_t *pool);

pc_error_t *pc_path_get_hidden(pc_bool_t *hidden,
                               const char *path,
                               pc_pool_t *pool);
pc_error_t *pc_path_set_hidden(const char *path,
                               pc_bool_t hidden,
                               pc_pool_t *pool);

/* ### need to figure out time type.  */
pc_error_t *pc_path_get_mtime(uint64_t *mtime,
                              const char *path,
                              pc_pool_t *pool);
pc_error_t *pc_path_set_mtime(const char *path,
                              uint64_t mtime,
                              pc_pool_t *pool);


/* Return in ACTUAL how PATH is recorded on disk. This may differ in case,
   and possibly in Unicode normalization form.  */
pc_error_t *pc_path_actual(char **actual,
                           const char *path,
                           pc_pool_t *pool);


typedef enum {
    pc_kind_file,     /* Regular file.  */
    pc_kind_dir,      /* Directory.  */
    pc_kind_symlink,  /* Symbolic link.  */
    pc_kind_pipe,     /* FIFO/pipe.  */
    pc_kind_socket,   /* Unix domain socket.  */
    pc_kind_other     /* Something other than above.  */
} pc_path_kind_t;

typedef struct {
    pc_path_kind_t kind;
    uint64_t size;  /* ### off_t?  */
    uint64_t mtime;  /* ### time type?  */

    /* Note: other (typical) fields are platform-specific. If you want to
       get more complicated, then you're (necessarily) doing something
       specific, and should just go straight to the OS-specific functions.  */

} pc_path_info_t;

pc_error_t *pc_path_stat(pc_path_info_t *info,
                         const char *path,
                         pc_pool_t *pool);


/* ### we should probably have a function to return an array of volumes,
   ### with structures specifying the mount path, flags, and charset.
   ### anyways... below are the considerations to include.  */

/* Return information about how paths are handled on the mounted-volume
   indicated by PATH.

   PoCore does no pathname transliteration, so applications may need to
   understand the underlying path handling used by the filesystem.

   ### need to figure out per-volume handling. there is also a chicken/egg
   ### on figuring out how to specify PATH.  */
int pc_path_handling(pc_context_t *ctx, const char *path);

/* Paths are case-preserving, but matched case-insignificantly.  */
#define PC_PATH_CASE_INSIGNIFICANT 0x0001

/* Paths are stored in Normalization Form D. This can mean storing path
   "FOO" will have a *different* representation when fetched using
   pc_path_listdir(). See pc_path_actual() to get the actual pathname.

   This behavior occurs on HFS(+) volumes, and possibly NFS4.  */
#define PC_PATH_NFD                0x0002 /* normalization form D  */

/* Paths must be presented using a specific character set/encoding. Passing a
   path that is invalid (within that encoding) will raise errors.

   See pc_file_path_encoding().

   This behavior is mostly tied to JFS volumes.  */
#define PC_PATH_CHARSET_RESTRICTED 0x0004


/* If paths must be presented in a specific encoding, this specifies that
   encoding. Otherwise, it returns NULL.

   ### chicken/egg for the PATH parameter...  */
const char *pc_path_encoding(pc_context_t *ctx, const char *path);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* PC_FILE_H */
