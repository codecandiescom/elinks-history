/* $Id: digest.h,v 1.2 2004/11/14 15:49:05 jonas Exp $ */

#ifndef EL__PROTOCOL_AUTH_DIGEST_H
#define EL__PROTOCOL_AUTH_DIGEST_H

struct auth_entry;

unsigned char *digest_calc_ha1(struct auth_entry *, unsigned char *);
unsigned char *digest_calc_response(struct auth_entry *, unsigned char *, unsigned char *);
unsigned char *random_cnonce(void);

#endif
