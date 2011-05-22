/*
  pc_misc.h :  miscellaneous portability support

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

#ifndef PC_MISC_H
#define PC_MISC_H

/* ### is this always the correct header?  */
#include <stdlib.h>  /* for size_t  */

#include "pc_types.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


pc_context_t *pc_context_create(void);


#define PC_DEFAULT_STDSIZE 0
#define PC_DEFAULT_OOM_HANDLER NULL
#define PC_DEFAULT_TRACK TRUE

pc_context_t *pc_context_create_custom(size_t stdsize,
                                       int (*oom_handler)(size_t amt),
                                       pc_bool_t track_unhandled);

void pc_context_destroy(pc_context_t *ctx);


/* Enable/disable error return tracing.  */
void pc_context_tracing(pc_context_t *ctx, pc_bool_t tracing);


/* Return an unhandled error that was saved in CTX. Returns NULL if there
   are no unhandled errors.

   The application should call pc_error_handled() on the returned error
   (after any appropriate reporting), and then call this function to get
   the next unhandled error (if any).

   Note that an application can call this at any time to investigate if
   some errors got left behind. It should almost always be called at
   application exit time.  */
pc_error_t *pc_context_unhandled(pc_context_t *ctx);


/* ### uuid  */


/* Utility macro to produce a string containing the *value* of MACRO, rather
   than a string of MACRO itself. This works because PC_STRINGIFY() and its
   argument is expanded first, followed by the expansion of
   PC_STRINGIFY_INTERNAL() where the stringification occurs.

   Note this only works for preprocessor macros since this all occurs at
   preprocessor time.  */
#define PC_STRINGIFY(macro) PC_STRINGIFY_INTERNAL(macro)
#define PC_STRINGIFY_INTERNAL(macro) #macro


/* Version handling  */

/* ### need to figure out how we want to handle "dev" vs "production"
   ### tagging.  */

#define PC_MAJOR_VERSION 0
#define PC_MINOR_VERSION 1
#define PC_PATCH_VERSION 0

#define PC_VERSION_STRING PC_STRINGIFY(PC_MAJOR_VERSION) "." \
                          PC_STRINGIFY(PC_MINOR_VERSION) "." \
                          PC_STRINGIFY(PC_PATCH_VERSION)

/* Returns TRUE if the version of PoCore (in the compilation) is at least
   that of <major.minor.patch> AND the major version matches (since APIs
   aren't going to be compatible across major version changes anyways).

   Note that a runtime version is always suggested, too.  */
#define PC_VERSION_AT_LEAST(major, minor, patch)                        \
          ((major) == PC_MAJOR_VERSION                                  \
           && (((minor) < PC_MINOR_VERSION)                             \
               || ((minor) == PC_MINOR_VERSION && (patch) <= PC_PATCH_VERSION))

/**
   Returns the version of the library the application has linked/loaded.
   Values are returned in @a major, @a minor, and @a patch.
 
   Applications will want to use this function to verify compatibility
   at runtime, in case of mis-linkage.  */
void pc_lib_version(
    int *major,
    int *minor,
    int *patch);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* PC_MISC_H */
