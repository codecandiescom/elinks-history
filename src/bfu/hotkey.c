/* Hotkeys handling. */
/* $Id: hotkey.c,v 1.4 2003/06/07 21:28:53 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/align.h"
#include "bfu/menu.h"
#include "config/kbdbind.h"
#include "intl/gettext/libintl.h"
#include "terminal/draw.h"
#include "terminal/kbd.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/conv.h"
#include "util/memory.h"


/* Return position (starting at 1) of the first tilde in text,
 * or 0 if not found. */
static inline int
find_hotkey_pos(unsigned char *text)
{
	if (text && *text) {
		unsigned char *p = strchr(text, '~');

		if (p) return (int)(p - text) + 1;
	}

	return 0;
}

void
init_hotkeys(struct terminal *term, struct menu_item *items, int ni,
	     int hotkeys)
{
	int i;

#ifdef DEBUG
	/* hotkey debugging */
	if (hotkeys) {
		unsigned char used_hotkeys[255];

		memset(used_hotkeys, 0, 255);

		for (i = 0; i < ni; i++) {
			unsigned char *text = items[i].text;

			if (!*text) continue;
			if (!item[i].no_intl) text = _(text, term);
			if (!*text) continue;

			if (items[i].ignore_hotkey != 2 && !items[i].hotkey_pos)
				items[i].hotkey_pos = find_hotkey_pos(text);

			/* Negative value for hotkey_pos means the key is already
			 * used by another entry. We mark it to be able to highlight
			 * this hotkey in menus. --Zas */
			if (items[i].hotkey_pos) {
				unsigned char *used = &used_hotkeys[upcase(text[items[i].hotkey_pos])];

				if (*used) {
					items[i].hotkey_pos = -items[i].hotkey_pos;
					if (items[*used].hotkey_pos > 0)
						items[*used].hotkey_pos = -items[*used].hotkey_pos;
				}

				*used = i;

				items[i].ignore_hotkey = 2; /* cached */
			}
		}
	}
#endif

	for (i = 0; i < ni; i++)
		if (!hotkeys) {
			items[i].hotkey_pos = 0;
			items[i].ignore_hotkey = 1;
		} else if (items[i].ignore_hotkey != 2 && !items[i].hotkey_pos) {
			unsigned char *text = items[i].text;

			if (!*text) continue;
			if (!item[i].no_intl) text = _(text, term);
			if (!*text) continue;

			items[i].hotkey_pos = find_hotkey_pos(text);

			if (items[i].hotkey_pos)
				items[i].ignore_hotkey = 2; /* cached */
		}
}

#ifdef ENABLE_NLS
void
clear_hotkeys_cache(struct menu_item *items, int ni, int hotkeys)
{
	int i;

	for (i = 0; i < ni; i++) {
		items[i].ignore_hotkey = hotkeys ? 0 : 1 ;
		items[i].hotkey_pos = 0;
	}
}
#endif

void
refresh_hotkeys(struct terminal *term, struct menu *menu)
{
#ifdef ENABLE_NLS
 	if (current_language != menu->lang) {
		clear_hotkeys_cache(menu->items, menu->ni, menu->hotkeys);
		init_hotkeys(term, menu->items, menu->ni, menu->hotkeys);
		menu->lang = current_language;
	}
#else
	init_hotkeys(term, menu->items, menu->ni, menu->hotkeys);
#endif
}

/* Returns true if key (upcased) matches one of the hotkeys in menu */
static inline int
is_hotkey(struct menu_item *item, unsigned char key, struct terminal *term)
{
	unsigned char *text = item->text;

	if (!item || !text || !*text) return 0;
	if (!item->no_intl) text = _(text, term);
	if (!*text) return 0;

#ifdef DEBUG
	{
	int key_pos = item->hotkey_pos;

	if (key_pos < 0) key_pos = -key_pos;

	return (key_pos && text
		&& (upcase(text[key_pos]) == key));
	}
#else
	return (item->hotkey_pos && text
		&& (upcase(text[item->hotkey_pos]) == key));
#endif
}

/* Returns true if a hotkey was found in the menu, and set menu->selected. */
int
check_hotkeys(struct menu_head *menu, unsigned char hotkey, struct terminal *term)
{
	unsigned char key = upcase(hotkey);
	int i = menu->selected;
	int start;

	if (menu->ni < 1) return 0;

	i %= menu->ni;
	if (i < 0) i += menu->ni;

	start = i;

	while (1) {
		if (i + 1 == menu->ni) i = 0;
		else i++;

		if (is_hotkey(&menu->items[i], key, term)) {
			menu->selected = i;
			return 1;
		}

		if (i == start) break;

	};

	return 0;
}

/* Search if first letter of an entry in menu matches the key (caseless comp.).
 * It searchs in all entries, from selected entry to bottom and then from top
 * to selected entry.
 * It returns 1 if found and set menu->selected. */
int
check_not_so_hot_keys(struct menu_head *menu, unsigned char key, struct terminal *term)
{
	unsigned char *text;
	unsigned char k = upcase(key);
	int i = menu->selected;
	int start;

	if (menu->ni < 1) return 0;

	i %= menu->ni;
	if (i < 0) i += menu->ni;

	start = i;

	while (1) {
		if (i + 1 == menu->ni) i = 0;
		else i++;

		text = menu->items[i].text;

		if (text && upcase(text[0]) == k) {
			menu->selected = i;
			return 1;
		}

		if (i == start) break;
	};

	return 0;

}
