/* RFC1524 (mailcap file) implementation */
/* $Id */

#ifndef EL__PROTOCOL_MAILCAP_H
#define EL__PROTOCOL_MAILCAP_H

#include "config/options.h"

void mailcap_init(void);
void mailcap_exit(void);
struct option *mailcap_lookup(unsigned char *, unsigned char *);

#endif /* EL__PROTOCOL_MAILCAP_H */
