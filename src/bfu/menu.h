/* $Id: menu.h,v 1.45 2004/01/07 14:47:17 jonas Exp $ */

#ifndef EL__BFU_MENU_H
#define EL__BFU_MENU_H

#include "config/kbdbind.h"

struct terminal;
struct window;

typedef void (*menu_func)(struct terminal *, void *, void *);

/* Which fields to free when zapping a list item - bitwise. */
enum menu_item_flags {
	NO_FLAG = 0,

	FREE_LIST = 1,
	FREE_TEXT = 2,
	FREE_RTEXT = 4, /* only for menu_item */
	FREE_DATA = 8, /* for menu_item, see menu.c for remarks */

	MENU_FULLNAME = 16,
	SUBMENU = 32,
	NO_INTL = 64,
	NO_SELECT = 128,	/* Mark unselectable item */
	RIGHT_INTL = 256,	/* Force translation of right text. */
};

/*
 * Unselectable menu item
 */
#define mi_is_unselectable(mi) ((mi).flags & NO_SELECT)
#define mi_is_selectable(mi) (!mi_is_unselectable(mi))

/*
 * Menu item has left text.
 */
#define mi_has_left_text(mi) ((mi).text && *(mi).text)

/*
 * Menu item has right text.
 */
#define mi_has_right_text(mi) ((mi).rtext && *(mi).rtext)

/*
 * Horizontal bar
 */
#define mi_is_horizontal_bar(mi) (mi_is_unselectable(mi) && (mi).text && !(mi).text[0])

/*
 * Submenu item
 */
#define mi_is_submenu(mi) ((mi).flags & SUBMENU)

/*
 * Texts should be translated or not.
 */
#define mi_text_translate(mi) (!((mi).flags & NO_INTL))
#define mi_rtext_translate(mi) ((mi).flags & RIGHT_INTL)

/*
 * End of menu items list
 */
#define mi_is_end_of_menu(mi) (!(mi).text)


enum hotkey_state {
	HKS_SHOW = 0,
	HKS_IGNORE,
	HKS_CACHED,
};

/* XXX: keep order of fields, there's some hard initializations for it. --Zas
 */
struct menu_item {
	unsigned char *text;
	unsigned char *rtext;
	enum keyact action;
	menu_func func;
	void *data;
	enum menu_item_flags flags;

	/* If true, don't try to translate text/rtext inside of the menu
	 * routines. */
	enum hotkey_state hotkey_state;
	int hotkey_pos;
};

#define INIT_MENU_ITEM(text, rtext, action, func, data, flags)		\
{									\
	(unsigned char *) (text),					\
	(unsigned char *) (rtext),					\
	(action),							\
	(menu_func) (func),						\
	(void *) (data),						\
	(flags),							\
	HKS_SHOW,							\
	0								\
}

#define NULL_MENU_ITEM							\
	INIT_MENU_ITEM(NULL, NULL, ACT_NONE, NULL, NULL, 0)

#define BAR_MENU_ITEM							\
	INIT_MENU_ITEM("", NULL, ACT_NONE, NULL, NULL, NO_SELECT)

#define SET_MENU_ITEM(e_, text_, rtext_, action_, func_, data_, flags_,	\
		      hotkey_state_, hotkey_pos_)			\
do {									\
	(e_)->text = (unsigned char *) (text_);				\
	(e_)->rtext = (unsigned char *) (rtext_);			\
	(e_)->action = (action_);					\
	(e_)->func = (menu_func) (func_);				\
	(e_)->data = (void *) (data_);					\
	(e_)->flags = (flags_);						\
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
	int ni

struct menu_head {
	MENU_HEAD;
};

struct menu {
	MENU_HEAD;
	int view;
	int x, y;
	int width, height;
	int parent_x, parent_y;
	int hotkeys;
#ifdef ENABLE_NLS
	int lang;
#endif
};


struct menu_item *new_menu(enum menu_item_flags);

void
add_to_menu(struct menu_item **mi, unsigned char *text, unsigned char *rtext,
	    enum keyact action, menu_func func, void *data,
	    enum menu_item_flags flags);

#define add_separator_to_menu(menu) \
	add_to_menu(menu, "", NULL, ACT_NONE, NULL, NULL, NO_SELECT)

/* Implies that the action will be handled by do_action() */
#define add_menu_action(menu, text, action) \
	add_to_menu(menu, text, NULL, action, NULL, NULL, NO_FLAG)

void do_menu(struct terminal *, struct menu_item *, void *, int);
void do_menu_selected(struct terminal *, struct menu_item *, void *, int, int);
void do_mainmenu(struct terminal *, struct menu_item *, void *, int);

#endif
