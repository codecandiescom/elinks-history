/* $Id: proxy.h,v 1.3 2004/04/01 18:34:13 jonas Exp $ */

#ifndef EL__PROTOCOL_PROXY_H
#define EL__PROTOCOL_PROXY_H

struct uri;

struct uri *get_proxy(struct uri *uri);

#endif
