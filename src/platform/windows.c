/*
  windows.c :  implementation of Windows-specific helper functions

  ====================================================================
    Copyright 2012 Greg Stein

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

#include <windows.h>


void pc__windows_uuid_create(pc_uuid_t *uuid_out)
{
    GUID guid;

    /* ### do we need to test the HRESULT here?  */
    (void) CoCreateGuid(&guid);

    /* Copy the bytes, being careful to avoid platform endian-ness.  */
    uuid_out->bytes[0] = (uint8_t)(guid.Data1 >> 24);
    uuid_out->bytes[1] = (uint8_t)(guid.Data1 >> 16);
    uuid_out->bytes[2] = (uint8_t)(guid.Data1 >>  8);
    uuid_out->bytes[3] = (uint8_t)(guid.Data1      );

    uuid_out->bytes[4] = (uint8_t)(guid.Data2 >>  8);
    uuid_out->bytes[5] = (uint8_t)(guid.Data2      );

    uuid_out->bytes[6] = (uint8_t)(guid.Data3 >>  8);
    uuid_out->bytes[7] = (uint8_t)(guid.Data3      );

    memcpy(&uuid_out->bytes[8], &guid.Data4[0], 8);
}
