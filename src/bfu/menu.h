/* $Id: menu.h,v 1.5 2002/04/17 08:24:02 pasky Exp $ */

#ifndef EL__BFU_MENU_H
#define EL__BFU_MENU_H

#include <lowlevel/terminal.h>


#define MENU_FUNC (void (*)(struct terminal *, void *, void *))

extern unsigned char m_bar;
#define M_BAR	(&m_bar)

struct menu_item {
	unsigned char *text;
	unsigned char *rtext;
	unsigned char *hotkey; /* TODO: keybindings ?!?! */
	void (*func)(struct terminal *, void *, void *);
	void *data;
	int in_m;
	int free_i;
};

struct menu {
	int selected;
	int view;
	int xp, yp;
	int x, y, xw, yw;
	int ni;
	void *data;
	struct window *win;
	struct menu_item *items;
};

struct menu_item *new_menu(int);
void add_to_menu(struct menu_item **, unsigned char *, unsigned char *, unsigned char *, void (*)(struct terminal *, void *, void *), void *, int);
void do_menu(struct terminal *, struct menu_item *, void *);
void do_menu_selected(struct terminal *, struct menu_item *, void *, int);
void do_mainmenu(struct terminal *, struct menu_item *, void *, int);

#endif
