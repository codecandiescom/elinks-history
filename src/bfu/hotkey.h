/* Hotkeys handling. */
/* $Id: hotkey.h,v 1.1 2003/05/21 10:08:43 zas Exp $ */

#ifndef __EL_HOTKEY_H
#define __EL_HOTKEY_H

#include "bfu/menu.h"

//int find_hotkey_pos(unsigned char *text);
void init_hotkeys(struct terminal *term, struct menu_item *items, int ni, int hotkeys);
#ifdef ENABLE_NLS
void clear_hotkeys_cache(struct menu_item *items, int ni, int hotkeys);
#endif
void refresh_hotkeys(struct terminal *term, struct menu *menu);
//int is_hotkey(struct menu_item *item, unsigned char key, struct terminal *term);
int check_hotkeys(struct menu_head *menu, unsigned char hotkey, struct terminal *term);
int check_not_so_hot_keys(struct menu_head *menu, unsigned char key, struct terminal *term);

#endif /* __EL_HOTKEY_H */
