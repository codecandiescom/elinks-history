/* Hotkeys handling. */
/* $Id: hotkey.h,v 1.7 2004/08/03 21:02:03 zas Exp $ */

#ifndef EL__BFU_HOTKEY_H
#define EL__BFU_HOTKEY_H

struct menu;
struct menu_item;
struct terminal;

/* int find_hotkey_pos(unsigned char *text); */
void init_hotkeys(struct terminal *term, struct menu_item *items, int hotkeys);
#ifdef ENABLE_NLS
void clear_hotkeys_cache(struct menu_item *items, int hotkeys);
#endif
void refresh_hotkeys(struct terminal *term, struct menu *menu);
/* int is_hotkey(struct menu_item *item, unsigned char key, struct terminal *term); */
int check_hotkeys(struct menu *menu, unsigned char hotkey, struct terminal *term);
int check_not_so_hot_keys(struct menu *menu, unsigned char key, struct terminal *term);

#endif
