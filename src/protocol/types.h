/* $Id: types.h,v 1.1 2002/03/17 11:29:12 pasky Exp $ */

#ifndef EL__TYPES_H
#define EL__TYPES_H

#include "links.h" /* list_head, tcount */
#include "terminal.h"

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

struct extension {
	struct extension *next;
	struct extension *prev;
	tcount cnt;
	unsigned char *ext;
	unsigned char *ct;
};

struct protocol_program {
	struct protocol_program *next;
	struct protocol_program *prev;
	unsigned char *prog;
	int system;
};

extern struct list_head assoc;
extern struct list_head extensions;

extern struct list_head mailto_prog;
extern struct list_head telnet_prog;
extern struct list_head tn3270_prog;

unsigned char *get_content_type(unsigned char *, unsigned char *);
struct assoc *get_type_assoc(struct terminal *term, unsigned char *);
void update_assoc(struct assoc *);
void update_ext(struct extension *);
void update_prog(struct list_head *, unsigned char *, int);
unsigned char *get_prog(struct list_head *);
void free_types();

void menu_add_ct(struct terminal *, void *, void *);
void menu_del_ct(struct terminal *, void *, void *);
void menu_list_assoc(struct terminal *, void *, void *);
void menu_add_ext(struct terminal *, void *, void *);
void menu_del_ext(struct terminal *, void *, void *);
void menu_list_ext(struct terminal *, void *, void *);

#endif
