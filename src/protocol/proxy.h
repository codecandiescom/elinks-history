/* $Id: proxy.h,v 1.2 2004/04/01 06:22:38 jonas Exp $ */

#ifndef EL__PROTOCOL_PROXY_H
#define EL__PROTOCOL_PROXY_H

struct uri;

unsigned char *get_proxy(struct uri *uri);

#endif
