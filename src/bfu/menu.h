/* $Id: menu.h,v 1.27 2003/09/28 13:43:55 jonas Exp $ */

#ifndef EL__BFU_MENU_H
#define EL__BFU_MENU_H

#include "terminal/terminal.h"

typedef void (*menu_func)(struct terminal *, void *, void *);

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

enum hotkey_state {
	HKS_SHOW = 0,
	HKS_IGNORE,
	HKS_CACHED,
};

/* XXX: keep order of fields, there's some hard initializations for it. --Zas
 */
struct menu_item {
	unsigned char *text;
	unsigned char *rtext; /* FIXME: Use real keybindings. */
	menu_func func;
	void *data;
	enum item_free item_free;

	unsigned int submenu:1;

	/* If true, don't try to translate text/rtext inside of the menu
	 * routines. */
	unsigned int no_intl:1;
	enum hotkey_state hotkey_state;
	int hotkey_pos;
};

#define INIT_MENU_ITEM(text, rtext, func, data, item_free, submenu)	\
{									\
	(unsigned char *) (text),					\
	(unsigned char *) (rtext),					\
	(menu_func) (func),						\
	(void *) (data),						\
	(item_free),							\
	(submenu),							\
	0,								\
	HKS_SHOW,							\
	0								\
}

#define NULL_MENU_ITEM							\
	INIT_MENU_ITEM(NULL, NULL, NULL, NULL, FREE_NOTHING, 0)

#define BAR_MENU_ITEM							\
	INIT_MENU_ITEM("", M_BAR, NULL, NULL, FREE_NOTHING, 0)

#define SET_MENU_ITEM(e_, text_, rtext_, func_, data_, item_free_,	\
		      submenu_, no_intl_, hotkey_state_, hotkey_pos_)	\
do {									\
	(e_)->text = (unsigned char *) (text_);				\
	(e_)->rtext = (unsigned char *) (rtext_);			\
	(e_)->func = (menu_func) (func_);				\
	(e_)->data = (void *) (data_);					\
	(e_)->item_free = (item_free_);					\
	(e_)->submenu = (submenu_);					\
	(e_)->no_intl = (no_intl_);					\
	(e_)->hotkey_state = (hotkey_state_);				\
	(e_)->hotkey_pos = (hotkey_pos_);				\
} while (0)


/* These structs are used in hotkey.h. */
/* Must match the start of structs menu and mainmenu */
#define MENU_HEAD			\
	struct window *win;		\
	struct menu_item *items;	\
	void *data;			\
	int selected;			\
	int ni;

struct menu_head {
	MENU_HEAD;
};

struct menu {
	MENU_HEAD;
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
void add_to_menu(struct menu_item **, unsigned char *, unsigned char *, menu_func, void *, int, int);
void do_menu(struct terminal *, struct menu_item *, void *, int);
void do_menu_selected(struct terminal *, struct menu_item *, void *, int, int);
void do_mainmenu(struct terminal *, struct menu_item *, void *, int);

#endif
