/*
  bsd.c :  implementation of BSD-specific helper functions

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

#include <uuid.h>


void pc__bsd_uuid_create(pc_uuid_t *uuid_out)
{
    uuid_t uuid;
    uint32_t status;

    /* ### do anything with the status?  */
    uuid_create(&uuid, &status);

    /* Copy the bytes, being careful to avoid platform endian-ness.  */
    uuid_out->bytes[0] = (uint8_t)(uuid.time_low >> 24);
    uuid_out->bytes[1] = (uint8_t)(uuid.time_low >> 16);
    uuid_out->bytes[2] = (uint8_t)(uuid.time_low >>  8);
    uuid_out->bytes[3] = (uint8_t)(uuid.time_low      );

    uuid_out->bytes[4] = (uint8_t)(uuid.time_mid >>  8);
    uuid_out->bytes[5] = (uint8_t)(uuid.time_mid      );

    uuid_out->bytes[6] = (uint8_t)(uuid.time_hi_and_version >> 8);
    uuid_out->bytes[7] = (uint8_t)(uuid.time_hi_and_version     );

    uuid_out->bytes[8] = uuid.clock_seq_hi_and_reserved;
    uuid_out->bytes[9] = uuid.clock_seq_low;

    memcpy(&uuid_out->bytes[10], &uuid.node[0], 6);
}
