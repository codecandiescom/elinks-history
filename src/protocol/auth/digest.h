/* $Id: digest.h,v 1.1 2004/11/14 14:17:18 witekfl Exp $ */

#ifndef EL__PROTOCOL_AUTH_DIGEST_H
#define EL__PROTOCOL_AUTH_DIGEST_H

struct http_auth_basic;

unsigned char *digest_calc_ha1(struct http_auth_basic *, unsigned char *);
unsigned char *digest_calc_response(struct http_auth_basic *, unsigned char *, unsigned char *);
unsigned char *random_cnonce(void);

#endif
