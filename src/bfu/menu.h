/* $Id: menu.h,v 1.15 2003/05/04 17:25:51 pasky Exp $ */

#ifndef EL__BFU_MENU_H
#define EL__BFU_MENU_H

#include "terminal/terminal.h"

#define MENU_FUNC_TYPE	void (*)(struct terminal *, void *, void *)
#define MENU_FUNC	(MENU_FUNC_TYPE)

extern unsigned char m_submenu[];
#define M_SUBMENU ((unsigned char *) m_submenu)

extern unsigned char m_bar;
#define M_BAR ((unsigned char *) &m_bar)


/* Which fields to free when zapping a list item - bitwise. */
enum item_free {
	FREE_NOTHING = 0,
	FREE_LIST = 1,
	FREE_TEXT = 2,
	FREE_RTEXT = 4, /* only for menu_item */
	FREE_DATA = 8, /* for menu_item, see menu.c for remarks */
};

/* menu item with no right part :
 * text != NULL and text[0] and rtext != NULL and !rtext[0]
 *
 * menu item with right part:
 * text != NULL and text[0] and rtext != NULL and rtext[0]
 *
 * unselectable menu item:
 * text != NULL and text[0] and rtext == M_BAR
 *
 * horizontal bar
 * text != NULL and !text[0] and rtext == M_BAR
 *
 * end of menu items list
 * text == NULL
 *
 */

struct menu_item {
	unsigned char *text;
	unsigned char *rtext; /* FIXME: Use real keybindings. */
	void (*func)(struct terminal *, void *, void *);
	void *data;
	int in_m;
	enum item_free item_free;
	int hotkey_pos;
	int ignore_hotkey;
};


struct menu_item *new_menu(enum item_free);
void add_to_menu(struct menu_item **, unsigned char *, unsigned char *, MENU_FUNC_TYPE, void *, int);
void do_menu(struct terminal *, struct menu_item *, void *, int);
void do_menu_selected(struct terminal *, struct menu_item *, void *, int, int);
void do_mainmenu(struct terminal *, struct menu_item *, void *, int);

#endif
