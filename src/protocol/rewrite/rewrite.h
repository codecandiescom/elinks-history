/* $Id: rewrite.h,v 1.3 2004/01/26 06:20:51 jonas Exp $ */

#ifndef EL__PROTOCOL_REWRITE_REWRITE_H
#define EL__PROTOCOL_REWRITE_REWRITE_H

#ifdef CONFIG_URI_REWRITE

#include "modules/module.h"

extern struct module uri_rewrite_module;

enum uri_rewrite_type {
	URI_REWRITE_DUMB,
	URI_REWRITE_SMART,
};

unsigned char *
get_uri_rewrite_prefix(enum uri_rewrite_type type, unsigned char *url);

unsigned char *
rewrite_uri(unsigned char *url, unsigned char *current_url, unsigned char *arg);

#endif
#endif
