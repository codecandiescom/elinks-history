/* $Id: mime.h,v 1.3 2002/12/21 19:00:41 zas Exp $ */

#ifndef EL__PROTOCOL_MIME_H
#define EL__PROTOCOL_MIME_H

#include "config/options.h"
#include "lowlevel/terminal.h"

unsigned char *get_content_type(unsigned char *, unsigned char *);

unsigned char *get_mime_type_name(unsigned char *);
struct option *get_mime_type_handler(struct terminal *, unsigned char *);

#endif
