/*
  pocore_platform.h :  generated, platform-specific definitions

  ### we need to generate this! for now, just do it manually

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

#ifndef POCORE_PLATFORM_H
#define POCORE_PLATFORM_H

#if defined(__MACH__) && defined(__APPLE__)
#define PC__IS_MACOSX

#elif defined(_WIN32)
#define PC__IS_WINDOWS

/* Go ahead and include this here/now. It has everything.  */
#include <windows.h>

#elif defined(__linux__)
#define PC__IS_LINUX

#include <sys/socket.h>

#elif defined(__FreeBSD__) || defined(__NetBSD__)
#define PC__IS_BSD

#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#ifdef PC__IS_LINUX
#define PC__SOCKADDR_LEN(ss) pc__linux_sockaddr_len(ss)
size_t pc__linux_sockaddr_len(const struct sockaddr_storage *ss);
#else
#define PC__SOCKADDR_LEN(ss) ((ss)->ss_len)
#endif


#ifdef PC__IS_WINDOWS
#define PC__FREE(ctx, p) HeapFree((ctx)->heap, 0, p)
#else
#define PC__FREE(ctx, p) free(p)
#endif


#ifdef PC__IS_WINDOWS
#define PC__INLINE __inline
#elif defined(__GNUC__) && __GNUC__ >= 3
#define PC__INLINE inline
#else
#define PC__INLINE
#endif


/* Various stuff for UUID generation.  */
#if defined(PC__IS_WINDOWS)
void pc__windows_uuid_create(pc_uuid_t *uuid_out);
#elif defined(PC__IS_BSD)
void pc__bsd_uuid_create(pc_uuid_t *uuid_out);
#endif

/* ### this should be an scons feature test. hack for now.  */
#if defined(PC__IS_MACOSX) || defined(PC__IS_LINUX)
#define PC__USE_UUID_GENERATE
#endif


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* POCORE_PLATFORM_H */
