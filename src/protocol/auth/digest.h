/* $Id: digest.h,v 1.4 2004/11/19 23:40:05 jonas Exp $ */

#ifndef EL__PROTOCOL_AUTH_DIGEST_H
#define EL__PROTOCOL_AUTH_DIGEST_H

struct auth_entry;
struct uri;

unsigned char *digest_calc_ha1(struct auth_entry *, unsigned char *);
unsigned char *digest_calc_response(struct auth_entry *, struct uri *, unsigned char *, unsigned char *);
unsigned char *random_cnonce(void);

unsigned char *
get_http_auth_digest_challenge(struct auth_entry *entry, struct uri *uri);

#endif
