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

#if defined(__MACH__) && defined(__APPLE__)
#define PC__IS_MACOSX

#elif defined(_WIN32)
#define PC__IS_WINDOWS

/* Go ahead and include this here/now. It has everything.  */
#include <windows.h>
#endif

/* ### we don't have other platform-specific stuff yet...  */
