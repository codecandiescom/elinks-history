/* $Id: menu.h,v 1.22 2003/06/07 20:51:57 pasky Exp $ */

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

/* XXX: keep order of fields, there's some hard initializations for it. --Zas
 */
struct menu_item {
	unsigned char *text;
	unsigned char *rtext; /* FIXME: Use real keybindings. */
	void (*func)(struct terminal *, void *, void *);
	void *data;
	enum item_free item_free;
	unsigned int submenu:1;
	unsigned int ignore_hotkey:2;
	int hotkey_pos;
};


/* These structs are used in hotkey.h. */
/* Must match the start of structs menu and mainmenu */
struct menu_head {
	struct window *win;
	struct menu_item *items;
	void *data;
	int selected;
	int ni;
};

struct menu {
	/* menu_head */
	struct window *win;
	struct menu_item *items;
	void *data;
	int selected;
	int ni;
	/* end of menu_head */

	int view;
	int x, y;
	int xp, yp;
        int xw, yw;

	int hotkeys;
#ifdef ENABLE_NLS
	int lang;
#endif
};


struct menu_item *new_menu(enum item_free);
void add_to_menu(struct menu_item **, unsigned char *, unsigned char *, MENU_FUNC_TYPE, void *, int);
void do_menu(struct terminal *, struct menu_item *, void *, int);
void do_menu_selected(struct terminal *, struct menu_item *, void *, int, int);
void do_mainmenu(struct terminal *, struct menu_item *, void *, int);

#endif
