/* $Id: mime.h,v 1.4 2003/05/04 17:25:55 pasky Exp $ */

#ifndef EL__PROTOCOL_MIME_H
#define EL__PROTOCOL_MIME_H

#include "config/options.h"
#include "terminal/terminal.h"

unsigned char *get_content_type(unsigned char *, unsigned char *);

unsigned char *get_mime_type_name(unsigned char *);
struct option *get_mime_type_handler(struct terminal *, unsigned char *);

#endif
