/* $Id: types.h,v 1.9 2002/06/22 16:45:19 pasky Exp $ */

#ifndef EL__PROTOCOL_TYPES_H
#define EL__PROTOCOL_TYPES_H

#include "config/options.h"
#include "lowlevel/terminal.h"
#include "util/lists.h"

unsigned char *get_content_type(unsigned char *, unsigned char *);

unsigned char *get_mime_type_name(unsigned char *);
unsigned char *get_mime_handler_name(unsigned char *, int);
struct option *get_mime_type_handler(struct terminal *, unsigned char *);

unsigned char *get_prog(struct terminal *, unsigned char *);

void menu_add_ct(struct terminal *, void *, void *);
void menu_del_ct(struct terminal *, void *, void *);
void menu_list_assoc(struct terminal *, void *, void *);
void menu_add_ext(struct terminal *, void *, void *);
void menu_del_ext(struct terminal *, void *, void *);
void menu_list_ext(struct terminal *, void *, void *);

#endif
