/* $Id: types.h,v 1.7 2002/06/20 10:11:17 pasky Exp $ */

#ifndef EL__PROTOCOL_TYPES_H
#define EL__PROTOCOL_TYPES_H

#include "config/options.h"
#include "lowlevel/terminal.h"
#include "util/lists.h"

unsigned char *get_content_type(unsigned char *, unsigned char *);
struct option *get_type_assoc(struct terminal *term, unsigned char *);
unsigned char *get_prog(unsigned char *);

void menu_add_ct(struct terminal *, void *, void *);
void menu_del_ct(struct terminal *, void *, void *);
void menu_list_assoc(struct terminal *, void *, void *);
void menu_add_ext(struct terminal *, void *, void *);
void menu_del_ext(struct terminal *, void *, void *);
void menu_list_ext(struct terminal *, void *, void *);

#endif
