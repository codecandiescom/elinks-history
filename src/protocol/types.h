/* $Id: types.h,v 1.8 2002/06/20 11:05:33 pasky Exp $ */

#ifndef EL__PROTOCOL_TYPES_H
#define EL__PROTOCOL_TYPES_H

#include "config/options.h"
#include "lowlevel/terminal.h"
#include "util/lists.h"

unsigned char *get_content_type(unsigned char *, unsigned char *);
struct option *get_type_assoc(struct terminal *term, unsigned char *);
unsigned char *get_prog(struct terminal *term, unsigned char *);

void menu_add_ct(struct terminal *, void *, void *);
void menu_del_ct(struct terminal *, void *, void *);
void menu_list_assoc(struct terminal *, void *, void *);
void menu_add_ext(struct terminal *, void *, void *);
void menu_del_ext(struct terminal *, void *, void *);
void menu_list_ext(struct terminal *, void *, void *);

#endif
