/* $Id: types.h,v 1.6 2002/06/17 15:16:54 pasky Exp $ */

#ifndef EL__PROTOCOL_TYPES_H
#define EL__PROTOCOL_TYPES_H

#include "links.h" /* tcount */
#include "lowlevel/terminal.h"
#include "util/lists.h"

struct assoc {
	struct assoc *next;
	struct assoc *prev;
	tcount cnt;
	unsigned char *label;
	unsigned char *ct;
	unsigned char *prog;
	int cons;
	int xwin;
	int block;
	int ask;
	int system;
};

extern struct list_head assoc;

unsigned char *get_content_type(unsigned char *, unsigned char *);
struct assoc *get_type_assoc(struct terminal *term, unsigned char *);
void update_assoc(struct assoc *);
unsigned char *get_prog(unsigned char *);
void free_types();

void menu_add_ct(struct terminal *, void *, void *);
void menu_del_ct(struct terminal *, void *, void *);
void menu_list_assoc(struct terminal *, void *, void *);
void menu_add_ext(struct terminal *, void *, void *);
void menu_del_ext(struct terminal *, void *, void *);
void menu_list_ext(struct terminal *, void *, void *);

#endif
