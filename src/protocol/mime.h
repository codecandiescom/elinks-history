/* $Id: mime.h,v 1.1 2002/08/08 18:01:46 pasky Exp $ */

#ifndef EL__PROTOCOL_MIME_H
#define EL__PROTOCOL_MIME_H

#include "config/options.h"
#include "lowlevel/terminal.h"

unsigned char *get_content_type(unsigned char *, unsigned char *);

unsigned char *get_mime_type_name(unsigned char *);
unsigned char *get_mime_handler_name(unsigned char *, int);
struct option *get_mime_type_handler(struct terminal *, unsigned char *);

unsigned char *get_prog(struct terminal *, unsigned char *);

#endif
