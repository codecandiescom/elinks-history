/* Digest MD5 */
/* $Id: digest.c,v 1.14 2004/11/17 21:09:04 zas Exp $ */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef CONFIG_OPENSSL
#include <openssl/md5.h>
#else
#include <gnutls/openssl.h>
#endif

#include "elinks.h"

#include "protocol/auth/auth.h"
#include "protocol/auth/digest.h"
#include "util/conv.h"
#include "util/memory.h"

/* GNU TLS doesn't define this */
#ifndef MD5_DIGEST_LENGTH
#define MD5_DIGEST_LENGTH 16
#endif


/* taken from RFC 2617 */
static unsigned char *
convert_hex(unsigned char bin[MD5_DIGEST_LENGTH + 1])
{
	int i;
	unsigned char *hex = mem_alloc(MD5_DIGEST_LENGTH * 2 + 1);

	if (!hex) return NULL;

	for (i = 0; i < MD5_DIGEST_LENGTH; i++) {
		int j = i * 2;

		hex[j]   = hx(bin[i] >> 4 & 0xf);
		hex[++j] = hx(bin[i] & 0xf);
	}

	hex[MD5_DIGEST_LENGTH * 2] = '\0';
	return hex;
}

unsigned char *
random_cnonce(void)
{
	unsigned char md5[MD5_DIGEST_LENGTH + 1];
	int random;

	srand(time(0));

	random = rand();
	MD5((const unsigned char *) &random, sizeof(int), md5);

	return convert_hex(md5);
}

unsigned char *
digest_calc_ha1(struct auth_entry *entry, unsigned char *cnounce)
{
	MD5_CTX MD5Ctx;
	unsigned char skey[MD5_DIGEST_LENGTH + 1];

	MD5_Init(&MD5Ctx);
	MD5_Update(&MD5Ctx, entry->user, strlen(entry->user));
	MD5_Update(&MD5Ctx, ":", 1);
	MD5_Update(&MD5Ctx, entry->realm, strlen(entry->realm));
	MD5_Update(&MD5Ctx, ":", 1);
	MD5_Update(&MD5Ctx, entry->password, strlen(entry->password));
	MD5_Final(skey, &MD5Ctx);
	return convert_hex(skey);
}

unsigned char *
digest_calc_response(struct auth_entry *entry, struct uri *uri,
		     unsigned char *ha1, unsigned char *cnonce)
{
	MD5_CTX MD5Ctx;
	unsigned char Ha2[MD5_DIGEST_LENGTH + 1];
	unsigned char *Ha2_hex;

	MD5_Init(&MD5Ctx);
	MD5_Update(&MD5Ctx, "GET", 3);
	MD5_Update(&MD5Ctx, ":/", 2);
	MD5_Update(&MD5Ctx, uri->data, uri->datalen);
	MD5_Final(Ha2, &MD5Ctx);
	Ha2_hex = convert_hex(Ha2);

	MD5_Init(&MD5Ctx);
	MD5_Update(&MD5Ctx, ha1, 32);
	MD5_Update(&MD5Ctx, ":", 1);
	MD5_Update(&MD5Ctx, entry->nonce, strlen(entry->nonce));
	MD5_Update(&MD5Ctx, ":", 1);
	MD5_Update(&MD5Ctx, "00000001", 8);
	MD5_Update(&MD5Ctx, ":", 1);
	MD5_Update(&MD5Ctx, cnonce, strlen(cnonce));
	MD5_Update(&MD5Ctx, ":", 1);
	MD5_Update(&MD5Ctx, "auth", 4);
	MD5_Update(&MD5Ctx, ":", 1);
	MD5_Update(&MD5Ctx, Ha2_hex, 32);
	MD5_Final(Ha2, &MD5Ctx);
	mem_free_if(Ha2_hex);
	return convert_hex(Ha2);
}