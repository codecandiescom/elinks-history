/* Digest MD5 */
/* $Id: digest.c,v 1.5 2004/11/14 15:49:05 jonas Exp $ */

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
#include "util/memory.h"


/* taken from RFC 2617 */
static unsigned char *
convert_hex(unsigned char *bin, int len)
{
	int i;
	unsigned char *hex = mem_calloc(1, len * 2 + 1);

	if (!hex) return NULL;

	for (i = 0; i < len; i++) {
		unsigned char j = bin[i] >> 4 & 0xf;

		if (j <= 9)
			hex[i * 2] = j + '0';
		else
			hex[i * 2] = j + 'a' - 10;
		j = bin[i] & 0xf;
		if (j <= 9)
			hex[i * 2 + 1] = j + '0';
		else
			hex[i * 2 + 1] = j + 'a' - 10;
	}
	hex[len * 2] = '\0';
	return hex;
}

unsigned char *
random_cnonce(void)
{
	int r;
	srand(time(0));

	r = rand();
	return convert_hex((unsigned char *)&r, sizeof(r));
}

unsigned char *
digest_calc_ha1(struct auth_entry *entry, unsigned char *cnounce)
{
	MD5_CTX MD5Ctx;
	unsigned char skey[17];

	MD5_Init(&MD5Ctx);
	MD5_Update(&MD5Ctx, entry->user, strlen(entry->user));
	MD5_Update(&MD5Ctx, ":", 1);
	MD5_Update(&MD5Ctx, entry->realm, strlen(entry->realm));
	MD5_Update(&MD5Ctx, ":", 1);
	MD5_Update(&MD5Ctx, entry->password, strlen(entry->password));
	MD5_Final(skey, &MD5Ctx);
	return convert_hex(skey, 16);
}

unsigned char *
digest_calc_response(struct auth_entry *entry, unsigned char *ha1,
	unsigned char *cnonce)
{
	MD5_CTX MD5Ctx;
	unsigned char Ha2[17];
	unsigned char *Ha2_hex;

	MD5_Init(&MD5Ctx);
	MD5_Update(&MD5Ctx, "GET", 3);
	MD5_Update(&MD5Ctx, ":/", 2);
	MD5_Update(&MD5Ctx, entry->uri->data, strlen(entry->uri->data));
	MD5_Final(Ha2, &MD5Ctx);
	Ha2_hex = convert_hex(Ha2, 16);

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
	return convert_hex(Ha2, 16); 
}
