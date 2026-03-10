/*
 * Copyright (C) 2012 The Android Open Source Project
 * Modified: auth stubbed out for embedded targets (no OpenSSL)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <string.h>

#include "sysdeps.h"
#include "adb.h"
#include "adb_auth.h"

#define TRACE_TAG TRACE_AUTH

/*
 * Stub implementation for embedded targets.
 * Generates random token for protocol compatibility.
 */
int adb_auth_generate_token(void *token, size_t token_size)
{
    FILE *f;
    int ret;

    f = fopen("/dev/urandom", "re");
    if (!f)
        return 0;

    ret = fread(token, token_size, 1, f);
    fclose(f);
    return ret * token_size;
}

/*
 * Always accept - no RSA verification on embedded targets.
 */
int adb_auth_verify(void *token, void *sig, int siglen)
{
    (void)token;
    (void)sig;
    (void)siglen;
    return 1;
}

/*
 * Accept immediately - no framework key confirmation needed.
 */
void adb_auth_confirm_key(unsigned char *key, size_t len, atransport *t)
{
    (void)key;
    (void)len;
    adb_auth_verified(t);
}

/*
 * No-op - no auth listener needed on embedded targets.
 */
void adb_auth_init(void)
{
    D("Auth disabled for embedded target\n");
}
