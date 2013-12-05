/*
 * Copyright (c) 1999 Niels Provos <provos@citi.umich.edu>
 * Copyright (c) 2013 Andre Oliveira <me@andreldoliveira.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Niels Provos and Andre
 *      Oliveira.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <err.h>

#include "bsd-crypt.h"
#include "bsd-rijndael.h"

static u_int64_t gblock = 0x5fcbd1e0e617668f;
static u_int32_t encrypt_initialized = 1;
/* rediscrypt_key_t *kcur = NULL; */
static rijndael_ctx ctxt;

void 
cryptredis_encrypt(const rediscrypt_key_t *key,
                   const char *src,
                   u_int32_t *dst,
                   size_t count)
{
        u_int64_t block = gblock;
        u_int32_t *dsrc = (u_int32_t *)src;
        u_int32_t *ddst = dst;
        u_int32_t iv[4];
        u_int32_t iv1, iv2, iv3, iv4;

        cryptredis_key_prepare(key, 1);

        count /= sizeof(u_int32_t);

        iv[0] = block >> 32; iv[1] = block; iv[2] = ~iv[0]; iv[3] = ~iv[1];
        rijndael_encrypt(&ctxt, (u_char *)iv, (u_char *)iv); 
        iv1 = iv[0]; iv2 = iv[1]; iv3 = iv[2]; iv4 = iv[3];

        for (; count > 0; count -= 4) {
                ddst[0] = dsrc[0] ^ iv1;
                ddst[1] = dsrc[1] ^ iv2;
                ddst[2] = dsrc[2] ^ iv3;
                ddst[3] = dsrc[3] ^ iv4;
                /*
                 * Do not worry about endianess, it only needs to decrypt
                 * on this machine.
                 */
                rijndael_encrypt(&ctxt, (u_char *)ddst, (u_char *)ddst);
                iv1 = ddst[0];
                iv2 = ddst[1];
                iv3 = ddst[2];
                iv4 = ddst[3];

                dsrc += 4;
                ddst += 4;
        }
}

void cryptredis_decrypt(const rediscrypt_key_t *key,
                        const u_int32_t *src,
                        char *dst,
                        size_t count)
{
        u_int64_t block = gblock;
        u_int32_t *dsrc = (u_int32_t *)src;
        u_int32_t *ddst = (u_int32_t *)dst;
        u_int32_t iv[4];
        u_int32_t iv1, iv2, iv3, iv4, niv1, niv2, niv3, niv4;

        if (!encrypt_initialized)
                errx(1, "rediscrypt_decrypt: key not initialized");

        cryptredis_key_prepare(key, 0);

        count /= sizeof(u_int32_t);

        iv[0] = block >> 32; iv[1] = block; iv[2] = ~iv[0]; iv[3] = ~iv[1];
        rijndael_encrypt(&ctxt, (u_char *)iv, (u_char *)iv); 
        iv1 = iv[0]; iv2 = iv[1]; iv3 = iv[2]; iv4 = iv[3];

        for (; count > 0; count -= 4) {
                ddst[0] = niv1 = dsrc[0];
                ddst[1] = niv2 = dsrc[1];
                ddst[2] = niv3 = dsrc[2];
                ddst[3] = niv4 = dsrc[3];
                rijndael_decrypt(&ctxt, (u_char *)ddst, (u_char *)ddst);
                ddst[0] ^= iv1;
                ddst[1] ^= iv2;
                ddst[2] ^= iv3;
                ddst[3] ^= iv4;

                iv1 = niv1;
                iv2 = niv2;
                iv3 = niv3;
                iv4 = niv4;

                dsrc += 4;
                ddst += 4;
        }
}
void 
cryptredis_dump_ctxt(rijndael_ctx *ctxt)
{
        fprintf(stderr, "=>");
        fprintf(stderr, " c->enc_only: %d", ctxt->enc_only);
        fprintf(stderr, " c->Nr: %d", ctxt->Nr);
        fprintf(stderr, " sizeof(c->ek): %ld", sizeof(ctxt->ek));
        fprintf(stderr, " sizeof(c->dk): %ld", sizeof(ctxt->dk));
        fprintf(stderr, "\n");
}

void
cryptredis_key_prepare(const rediscrypt_key_t *key, int encrypt)
{
        /*
         * Check if we have prepared for this key already,
         * if we only have the encryption schedule, we have
         * to recompute and get the decryption schedule also.
         */
#if 0
        if (kcur == key && (encrypt || !ctxt.enc_only))
                return;
#endif

        if (!encrypt_initialized)
                encrypt_initialized = 1;

        if (encrypt)
                rijndael_set_key_enc_only(&ctxt, (u_char *)key,
                    sizeof(rediscrypt_key_t) * KEY_SIZE * 8);
        else
                rijndael_set_key(&ctxt, (u_char *)key,
                    sizeof(rediscrypt_key_t) * KEY_SIZE * 8);

/*        kcur = key; */
        cryptredis_dump_ctxt(&ctxt);
}

/* vim: set ts=8 sw=8 et: */
