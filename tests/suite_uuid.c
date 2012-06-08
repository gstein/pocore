/*
  suite_hash.c :  test suite for the hash table implementation

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

#include "pc_misc.h"

#include "ctest/ctest.h"


/* Some UUIDs for use in testing.  */
static const pc_uuid_t uuid1 = { "0123456789abcdef" };
static const char *human1 = "30313233-3435-3637-3839-616263646566";

static const pc_uuid_t uuid2 = { " *A|x\x00#4e}Z\x1f$a7M" };
static const char *human2a = "202A417C-7800-2334-657D-5A1F2461374D";
static const char *human2b = "202a417c-7800-2334-657d-5a1f2461374d";

#define ASSERT_UUID_EQUAL(u1, u2) \
    ASSERT_TRUE(memcmp((u1).bytes, (u2).bytes, 16) == 0)


CTEST(uuid, formatting)
{
    char buf[37];

    pc_uuid_format(buf, &uuid1);
    ASSERT_STR(human1, buf);

    pc_uuid_format(buf, &uuid2);
    ASSERT_STR(human2a, buf);
}


CTEST(uuid, parsing)
{
    pc_uuid_t uuid;

    pc_uuid_parse(&uuid, human1);
    ASSERT_UUID_EQUAL(uuid, uuid1);

    pc_uuid_parse(&uuid, human2a);
    ASSERT_UUID_EQUAL(uuid, uuid2);

    pc_uuid_parse(&uuid, human2b);
    ASSERT_UUID_EQUAL(uuid, uuid2);
}
