/* $Id: menu.h,v 1.7 2002/08/11 18:25:48 pasky Exp $ */

#ifndef EL__BFU_MENU_H
#define EL__BFU_MENU_H

#include "lowlevel/terminal.h"

#define MENU_FUNC_TYPE	void (*)(struct terminal *, void *, void *)
#define MENU_FUNC	(MENU_FUNC_TYPE)

extern unsigned char m_bar;
#define M_BAR	(&m_bar)


/* Which fields to free when zapping a list item - bitwise. */
enum item_free {
	FREE_NOTHING = 0,
	FREE_LIST = 1,
	FREE_TEXT = 2,
	FREE_RTEXT = 4, /* only for menu_item */
	FREE_DATA = 8, /* for menu_item, see menu.c for remarks */
};

struct menu_item {
	unsigned char *text;
	unsigned char *rtext;
	unsigned char *hotkey; /* TODO: keybindings ?!?! */
	void (*func)(struct terminal *, void *, void *);
	void *data;
	int in_m;
	enum item_free item_free;
};


struct menu_item *new_menu(enum item_free);
void add_to_menu(struct menu_item **, unsigned char *, unsigned char *, unsigned char *, MENU_FUNC_TYPE, void *, int);
void do_menu(struct terminal *, struct menu_item *, void *);
void do_menu_selected(struct terminal *, struct menu_item *, void *, int);
void do_mainmenu(struct terminal *, struct menu_item *, void *, int);

#endif
