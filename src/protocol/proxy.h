/* $Id: proxy.h,v 1.4 2004/06/08 14:20:55 jonas Exp $ */

#ifndef EL__PROTOCOL_PROXY_H
#define EL__PROTOCOL_PROXY_H

struct uri;

struct uri *get_proxy_uri(struct uri *uri);

#endif
