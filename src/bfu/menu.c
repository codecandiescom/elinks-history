/* Menu system implementation. */
/* $Id: menu.c,v 1.60 2003/05/04 17:21:09 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/align.h"
#include "bfu/menu.h"
#include "config/kbdbind.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/kbd.h"
#include "lowlevel/terminal.h"
#include "util/conv.h"
#include "util/memory.h"


/* Types and structures */

struct menu {
	int selected;
	int view;
	int xp, yp;
	int x, y, xw, yw;
	int ni;
	int hotkeys;
#ifdef ENABLE_NLS
	int lang;
#endif
	void *data;
	struct window *win;
	struct menu_item *items;
};

struct mainmenu {
	int selected;
	int sp;
	int ni;
	void *data;
	struct window *win;
	struct menu_item *items;
};

/* Global variables */
unsigned char m_submenu[] = ">";
unsigned char m_bar = 0;

/* Prototypes */
static void menu_func(struct window *, struct event *, int);
static void mainmenu_func(struct window *, struct event *, int);


static inline int
count_items(struct menu_item *items)
{
	int i = 0;

	if (items)
		for (; items[i].text; i++);

	return i;
}

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

static void
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
			unsigned char *text = _(items[i].text, term);

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
			items[i].hotkey_pos = find_hotkey_pos(_(items[i].text, term));
			if (items[i].hotkey_pos) items[i].ignore_hotkey = 2; /* cached */
		}
}

#ifdef ENABLE_NLS
static void
clear_hotkeys_cache(struct menu_item *items, int ni, int hotkeys)
{
	int i;

	for (i = 0; i < ni; i++) {
		items[i].ignore_hotkey = hotkeys ? 0 : 1 ;
		items[i].hotkey_pos = 0;
	}
}
#endif

static void
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


static inline int
is_hotkey(struct menu_item *item, unsigned char hotkey, struct terminal *term)
{
	unsigned char *text = _(item->text, term);

#ifdef DEBUG
	int key_pos = item->hotkey_pos;

	if (key_pos < 0) key_pos = -key_pos;
	return (key_pos && text
		&& (upcase(text[key_pos]) == upcase(hotkey)));
#else
	return (item->hotkey_pos && text
		&& (upcase(text[item->hotkey_pos]) == upcase(hotkey)));
#endif
}

static void
free_menu_items(struct menu_item *items)
{
	int i;

	if (!items) return;

	/* Note that item_free & FREE_DATA applies only when menu is aborted;
	 * it is zeroed when some menu field is selected. */

	for (i = 0; items[i].text; i++) {
		if (items[i].item_free & FREE_TEXT && items[i].text)
			mem_free(items[i].text);
		if (items[i].item_free & FREE_RTEXT && items[i].rtext)
			mem_free(items[i].rtext);
		if (items[i].item_free & FREE_DATA && items[i].data)
			mem_free(items[i].data);
	}

	mem_free(items);
}

void
do_menu_selected(struct terminal *term, struct menu_item *items,
		 void *data, int selected, int hotkeys)
{
	struct menu *menu = mem_alloc(sizeof(struct menu));

	if (menu) {
		menu->selected = selected;
		menu->view = 0;
		menu->items = items;
		menu->data = data;
		menu->ni = count_items(items);
		menu->hotkeys = hotkeys;
#ifdef ENABLE_NLS
		menu->lang = -1;
#endif
		refresh_hotkeys(term, menu);
		add_window(term, menu_func, menu);
	} else if (items->item_free & ~(1<<8)) {
		free_menu_items(items);
	}
}

void
do_menu(struct terminal *term, struct menu_item *items, void *data, int hotkeys)
{
	do_menu_selected(term, items, data, 0, hotkeys);
}

static void
select_menu(struct terminal *term, struct menu *menu)
{
	struct menu_item *it = &menu->items[menu->selected];
	void (*func)(struct terminal *, void *, void *);
	void *data1 = it->data;
	void *data2 = menu->data;

	func = it->func;

	if (menu->selected < 0 ||
	    menu->selected >= menu->ni ||
	    it->rtext == M_BAR)
		return;

	if (!it->in_m) {
		struct window *win, *win1;

		/* Don't free data! */
		it->item_free &= ~FREE_DATA;

		win = term->windows.next;

		while ((void *) win != &term->windows &&
			(win->handler == menu_func ||
			 win->handler == mainmenu_func)) {

			win1 = win->next;
			delete_window(win);
			win = win1;
		}
	}

	func(term, data1, data2);
}

static void
count_menu_size(struct terminal *term, struct menu *menu)
{
	int sx = term->x;
	int sy = term->y;
	int mx = 4;
	int my;

	for (my = 0; my < menu->ni; my++) {
		int s = 4;

		if (_(menu->items[my].text, term)
		    && _(menu->items[my].text, term)[0]) {
			s += strlen(_(menu->items[my].text, term))
			     - (menu->items[my].hotkey_pos ? 1 : 0)
			     + 1;
		}

		if (_(menu->items[my].rtext, term)
		    && _(menu->items[my].rtext, term)[0]) {
			s += MENU_HOTKEY_SPACE
			     + strlen(_(menu->items[my].rtext, term));
		}

		if (s > mx) mx = s;
	}

	my += 2;

	if (mx > sx) mx = sx;
	if (my > sy) my = sy;
	menu->xw = mx;
	menu->yw = my;

	if ((menu->x = menu->xp) < 0) menu->x = 0;
	if ((menu->y = menu->yp) < 0) menu->y = 0;
	if (menu->x + mx > sx) menu->x = sx - mx;
	if (menu->y + my > sy) menu->y = sy - my;
}

static void
scroll_menu(struct menu *menu, int d)
{
	int c = 0;
	int w = menu->yw - 2;
	int scr_i = (SCROLL_ITEMS > (w - 1) / 2) ? (w - 1) / 2 : SCROLL_ITEMS;

	if (scr_i < 0) scr_i = 0;
	if (w < 0) w = 0;

	menu->selected += (d > 0) ? 1 : -1;

	if (menu->ni) {
		menu->selected %= menu->ni;
		if (menu->selected < 0) menu->selected += menu->ni;
	}

	while (1) {
		if (c++ > menu->ni) {
			menu->selected = -1;
			menu->view = 0;
			return;
		}

		if (menu->selected < 0)
			menu->selected = 0;
		if (menu->selected >= menu->ni)
			menu->selected = menu->ni - 1;

#if 0
		if (menu->selected < 0) menu->selected = menu->ni - 1;
		if (menu->selected >= menu->ni) menu->selected = 0;
#endif
		if (menu->ni && menu->items[menu->selected].rtext != M_BAR)
			break;

		menu->selected += (d > 0) ? 1 : -1;
	}

	if (menu->selected < menu->view + scr_i)
		menu->view = menu->selected - scr_i;
	if (menu->selected >= menu->view + w - scr_i - 1)
		menu->view = menu->selected - w + scr_i + 1;
	if (menu->view > menu->ni - w)
		menu->view = menu->ni - w;
	if (menu->view < 0)
		menu->view = 0;
}

static void
display_menu(struct terminal *term, struct menu *menu)
{
	int p, s;
	int menu_normal_color = get_bfu_color(term, "menu.normal");
	int menu_frame_color = get_bfu_color(term, "menu.frame");
	int menu_selected_color = get_bfu_color(term, "menu.selected");
	int menu_hotkey_color = get_bfu_color(term, "menu.hotkey.normal");
	int menu_selected_hotkey_color = get_bfu_color(term, "menu.hotkey.selected");

	fill_area(term,	menu->x + 1, menu->y + 1, menu->xw - 2, menu->yw - 2,
		  menu_normal_color);

	draw_frame(term, menu->x, menu->y, menu->xw, menu->yw,
		   menu_frame_color, 1);

	for (p = menu->view, s = menu->y + 1;
	     p < menu->ni && p < menu->view + menu->yw - 2;
	     p++, s++) {
		int co = menu_normal_color;
		int hkco = menu_hotkey_color;

		if (p == menu->selected) {
			co = menu_selected_color;
			hkco = menu_selected_hotkey_color;

			set_cursor(term, menu->x + 1, s, term->x - 1, term->y - 1);
			set_window_ptr(menu->win, menu->x + menu->xw, s);
			fill_area(term, menu->x + 1, s, menu->xw - 2, 1, co);
		}

		if (menu->items[p].rtext != M_BAR ||
		    (_(menu->items[p].text, term) && _(menu->items[p].text, term)[0])) {
			unsigned char c;
			int l = menu->items[p].hotkey_pos;
			int x;

			if (l) {
				int hk = 0;
#ifdef DEBUG
				int double_hk = 0;

				if (l < 0) l = -l, double_hk = 1;
#endif
				for (x = 0;
				     x < menu->xw - 4
				     && (c = _(menu->items[p].text, term)[x]);
				     x++) {
					if (!hk && l
					    && x == l - 1) {
						hk = 1;
						continue;
					}
					if (hk == 1) {
#ifdef DEBUG
						set_char(term, menu->x + x - 1 + 2,
							 s, (double_hk ?  menu_selected_hotkey_color : hkco) | c);
#else
						set_char(term, menu->x + x - 1 + 2, s, hkco | c);
#endif
						hk = 2;
					} else {
						set_char(term, menu->x + x - (hk ? 1 : 0) + 2,
							 s, c | co);
					}
				}

			} else {
				for (x = 0;
				     x < menu->xw - 4
				     && (c = _(menu->items[p].text, term)[x]);
				     x++)
					set_char(term, menu->x + x + 2, s, c | co);
			}

			if (_(menu->items[p].rtext, term)
			    && _(menu->items[p].rtext, term)[0]) {
				l = strlen(_(menu->items[p].rtext, term));

				for (x = l - 1;
				     (x >= 0) && (menu->xw - 4 >= l - x)
				     && (c = _(menu->items[p].rtext, term)[x]);
				     x--)
					set_char(term, menu->x + menu->xw - 2 - l + x, s, c | co);
			}

		} else {
			set_char(term, menu->x, s,
				 menu_frame_color | FRAMES_RTEE);

			fill_area(term, menu->x + 1, s, menu->xw - 2, 1,
				  menu_frame_color | FRAMES_HLINE);

			set_char(term, menu->x + menu->xw - 1, s,
				 menu_frame_color | FRAMES_LTEE);
		}
	}

	redraw_from_window(menu->win);
}

static void
menu_func(struct window *win, struct event *ev, int fwd)
{
	struct window *w1;
	struct menu *menu = win->data;
	int s = 0;

	menu->win = win;

	switch (ev->ev) {
		case EV_INIT:
		case EV_RESIZE:
		case EV_REDRAW:
			get_parent_ptr(win, &menu->xp, &menu->yp);
			count_menu_size(win->term, menu);
			menu->selected--;
			scroll_menu(menu, 1);
			display_menu(win->term, menu);
			break;

		case EV_MOUSE:
			switch (ev->b & BM_BUTT) {
				/* XXX: We return here directly because we
				 * would just break this switch instead of the
				 * large one. If you will add some generic
				 * action after the former switch, replace the
				 * return with goto here. --pasky */
				case B_WHEEL_UP:
					if ((ev->b & BM_ACT) == B_DOWN) {
						scroll_menu(menu, -1);
						display_menu(win->term, menu);
					}
					return;
				case B_WHEEL_DOWN:
					if ((ev->b & BM_ACT) == B_DOWN) {
						scroll_menu(menu, 1);
						display_menu(win->term, menu);
					}
					return;
			}

			if ((ev->x < menu->x) || (ev->x >= menu->x + menu->xw) ||
			    (ev->y < menu->y) || (ev->y >= menu->y + menu->yw)) {
				if ((ev->b & BM_ACT) == B_DOWN)
					delete_window_ev(win, ev);

				else {
					for (w1 = win;
					     (void *)w1 != &win->term->windows;
					     w1 = w1->next) {
						struct menu *m1;

						if (w1->handler == mainmenu_func) {
							if (!ev->y)
								delete_window_ev(win, ev);
							break;
						}

						if (w1->handler != menu_func) break;

						m1 = w1->data;

						if (ev->x > m1->x &&
						    ev->x < m1->x + m1->xw - 1 &&
						    ev->y > m1->y &&
						    ev->y < m1->y + m1->yw - 1)
							delete_window_ev(win, ev);
					}
				}

			} else {
				if (!(ev->x <  menu->x ||
				      ev->x >= menu->x + menu->xw ||
				      ev->y <  menu->y + 1 ||
				      ev->y >= menu->y + menu->yw-1)) {
					int sel = ev->y - menu->y - 1 + menu->view;

					if (sel >= 0 && sel < menu->ni
					    && menu->items[sel].rtext != M_BAR) {
						menu->selected = sel;
						scroll_menu(menu, 0);
						display_menu(win->term, menu);

						if ((ev->b & BM_ACT) == B_UP ||
						    menu->items[sel].in_m)
							select_menu(win->term, menu);
					}
				}
			}

			break;

		case EV_KBD:
			switch (kbd_action(KM_MENU, ev, NULL)) {
				case ACT_LEFT:
				case ACT_RIGHT:
					if ((void *) win->next != &win->term->windows &&
					    win->next->handler == mainmenu_func) {
						delete_window_ev(win, ev);
						goto break2;
					}

					if (kbd_action(KM_MENU, ev, NULL) == ACT_RIGHT)
						goto enter;

					delete_window(win);

					goto break2;

				case ACT_UP:
					scroll_menu(menu, -1);
					break;

				case ACT_DOWN:
					scroll_menu(menu, 1);
					break;

				case ACT_HOME:
					menu->selected = -1;
					scroll_menu(menu, 1);
					break;

				case ACT_END:
					menu->selected = menu->ni;
					scroll_menu(menu, -1);
					break;
#define DIST 5
				case ACT_PAGE_UP:
				{
					int i = menu->selected - 2;
					int found = 0;
					int step = -1;

					for (; i >= 0; i--) {
						if (menu->items[i].rtext == M_BAR) {
							found = 1;
							break;
						}
					}

					if (found) {
						step = i + 1 - menu->selected;
					} else {
						step = -DIST;
					}

					if (menu->selected + step < 0)
						step = -menu->selected;
					if (step < -DIST) step = DIST;
					if (step > 0)
						step = menu->selected - menu->ni - 1;

					scroll_menu(menu, step);
				}
					break;

				case ACT_PAGE_DOWN:
				{
					int i = menu->selected;
					int found = 0;
					int step = 1;

					for (; i < menu->ni; i++) {
						if (menu->items[i].rtext == M_BAR) {
							found = 1;
							break;
						}
					}

					if (found) {
						step = i + 1 - menu->selected;
					} else {
						step = DIST;
					}

					if (menu->selected + step >= menu->ni)
						step = menu->ni - menu->selected - 1;
					if (step > DIST) step = DIST;
					if (step >= menu->ni)
						step = menu->ni - 1;

					scroll_menu(menu, step);
				}
					break;
#undef DIST
				default:
				{
					int i;

					if ((ev->x >= KBD_F1 && ev->x <= KBD_F12) ||
					    ev->y == KBD_ALT) {
						delete_window_ev(win, ev);
						goto break2;
					}

					if (ev->x == KBD_ESC) {
						if ((void *) win->next != &win->term->windows &&
						    win->next->handler == mainmenu_func)
							delete_window_ev(win, ev);
						else
							delete_window_ev(win, NULL);

						goto break2;
					}

					if (ev->x <= ' ' || ev->x >= 256)
						break;

					for (i = 0; i < menu->ni; i++) {
						if (!is_hotkey(&menu->items[i], ev->x, win->term))
							continue;
						menu->selected = i;
						scroll_menu(menu, 0);
						s = 1;
					}

					if (s != 0)
						break;

					for (i = 0; i < menu->ni; i++) {
						if (!menu->items[i].text ||
						    (upcase(menu->items[i].text[0])
						     != upcase(ev->x)))
							continue;
						if (upcase(menu->items[menu->selected].text[0])
						    	== upcase(ev->x)
						    && menu->selected >= i)
							continue;
						menu->selected = i;
						scroll_menu(menu, 0);
						break;
					}

					break;
				}
			}

			display_menu(win->term, menu);
			if (s || ev->x == KBD_ENTER || ev->x == ' ') {
enter:
				select_menu(win->term, menu);
			}

break2:
			break;

		case EV_ABORT:
			if (menu->items->item_free & ~(1<<8))
				free_menu_items(menu->items);

			break;
	}
}

void
do_mainmenu(struct terminal *term, struct menu_item *items,
	    void *data, int sel)
{
	struct mainmenu *menu = mem_calloc(1, sizeof(struct mainmenu));

	if (!menu) return;

	menu->selected = (sel == -1 ? 0 : sel);
	menu->items = items;
	menu->data = data;
	menu->ni = count_items(items);
#ifdef ENABLE_NLS
	clear_hotkeys_cache(items, menu->ni, 1);
#endif
	init_hotkeys(term, items, menu->ni, 1);
	add_window(term, mainmenu_func, menu);

	if (sel != -1) {
		struct event ev = {EV_KBD, KBD_ENTER, 0, 0};

		term_send_event(term, &ev);
	}
}

static void
display_mainmenu(struct terminal *term, struct mainmenu *menu)
{
	int i;
	int p = 2;
	int mainmenu_normal_color = get_bfu_color(term, "mainmenu.normal");
	int mainmenu_selected_color = get_bfu_color(term, "mainmenu.selected");
	int mainmenu_hotkey_color = get_bfu_color(term, "mainmenu.hotkey.normal");
	int mainmenu_selected_hotkey_color = get_bfu_color(term, "mainmenu.hotkey.selected");

	fill_area(term, 0, 0, term->x, 1, mainmenu_normal_color | ' ');

	for (i = 0; i < menu->ni; i++) {
		int j;
		int co = mainmenu_normal_color;
		int hkco = mainmenu_hotkey_color;
		int hk = 0;
		unsigned char c;
		unsigned char *tmptext = _(menu->items[i].text, term);
		int key_pos = menu->items[i].hotkey_pos;

#ifdef DEBUG
		int double_hk = 0;
		if (key_pos < 0) key_pos = -key_pos, double_hk = 1;
#endif
		if (i == menu->selected) {
			int tmptextlen = strlen(tmptext)
					 - (key_pos ? 1 : 0);

			co = mainmenu_selected_color;
			hkco = mainmenu_selected_hotkey_color;

			fill_area(term, p, 0, 2, 1, co);
			fill_area(term, p + tmptextlen + 2, 0, 2, 1, co);
			menu->sp = p;
			set_cursor(term, p, 0, term->x - 1, term->y - 1);
			set_window_ptr(menu->win, p, 1);
		}

		p += 2;

		for (j = 0; (c = tmptext[j]); j++, p++) {
			if (!hk && key_pos
			    && j == key_pos - 1) {
				hk = 1;
				p--;
				continue;
			}

			if (hk == 1) {
#ifdef DEBUG
				set_char(term, p, 0, (double_hk ? mainmenu_selected_hotkey_color : hkco) | c);
#else
				set_char(term, p, 0, hkco | c);
#endif
				hk = 2;
			} else {
				set_char(term, p, 0, co | c);
			}
		}

		p += 2;
	}

	redraw_from_window(menu->win);
}

static void
select_mainmenu(struct terminal *term, struct mainmenu *menu)
{
	struct menu_item *it = &menu->items[menu->selected];

	if (menu->selected < 0 || menu->selected >= menu->ni
	    || it->rtext == M_BAR)
		return;

	if (!it->in_m) {
		struct window *win;

		for (win = term->windows.next;
		     (void *) win != &term->windows
			&& (win->handler == menu_func ||
			    win->handler == mainmenu_func);
		     delete_window(win)) ;
	}

	it->func(term, it->data, menu->data);
}

static void
mainmenu_func(struct window *win, struct event *ev, int fwd)
{
	int s = 0;
	struct mainmenu *menu = win->data;

	menu->win = win;
	switch (ev->ev) {
		case EV_INIT:
		case EV_RESIZE:
		case EV_REDRAW:
			display_mainmenu(win->term, menu);
			break;

		case EV_MOUSE:
			if ((ev->b & BM_BUTT) >= B_WHEEL_UP)
				break;

			if ((ev->b & BM_ACT) == B_DOWN && ev->y) {
				delete_window_ev(win, ev);

			} else if (!ev->y) {
				int p = 2;
				int i;

				for (i = 0; i < menu->ni; i++) {
					int o = p;

					if (_(menu->items[i].text, win->term)
					    && _(menu->items[i].text, win->term)[0])
						p += strlen(_(menu->items[i].text,
							    win->term)) + 4
							    - (menu->items[i].hotkey_pos
							    ? 1 : 0);

					if (ev->x < o || ev->x >= p)
						continue;

					menu->selected = i;
					display_mainmenu(win->term, menu);
					if ((ev->b & BM_ACT) == B_UP
					    || menu->items[s].in_m) {
						select_mainmenu(win->term,
								menu);
					}
					break;
				}
			}
			break;

		case EV_KBD:
			if (ev->x == ' ' ||
			    ev->x == KBD_ENTER ||
			    ev->x == KBD_DOWN ||
			    ev->x == KBD_UP ||
			    ev->x == KBD_PAGE_DOWN ||
			    ev->x == KBD_PAGE_UP) {
				select_mainmenu(win->term, menu);
				break;
			}

			if (ev->x == KBD_LEFT) {
				if (!menu->selected--)
					menu->selected = menu->ni - 1;
				s = 1;

			} else if (ev->x == KBD_RIGHT) {
				if (++menu->selected >= menu->ni)
					menu->selected = 0;
				s = 1;

			}

			if ((ev->x == KBD_LEFT || ev->x == KBD_RIGHT) && fwd) {
				display_mainmenu(win->term, menu);
				select_mainmenu(win->term, menu);
				break;
			}

			if (ev->x > ' ' && ev->x < 256) {
				int i;

				s = 1;
				for (i = 0; i < menu->ni; i++)
					if (!is_hotkey(&menu->items[i], ev->x, win->term))
						continue;
					menu->selected = i;
					s = 2;

			} else if (!s) {
				delete_window_ev(win, ev->x != KBD_ESC  ? ev
									: NULL);
				break;
			}

			display_mainmenu(win->term, menu);
			if (s == 2)
				select_mainmenu(win->term, menu);
			break;

		case EV_ABORT:
			break;
	}
}

struct menu_item *
new_menu(enum item_free item_free)
{
	struct menu_item *mi = mem_calloc(1, sizeof(struct menu_item));

	if (mi) mi->item_free = item_free;

	return mi;
}

void
add_to_menu(struct menu_item **mi, unsigned char *text,
	    unsigned char *rtext,
	    void (*func)(struct terminal *, void *, void *),
	    void *data, int in_m)
{
	struct menu_item *mii;
	int n = count_items(*mi);

	mii = mem_realloc(*mi, (n + 2) * sizeof(struct menu_item));
	if (!mii) return;

	*mi = mii;
	memcpy(mii + n + 1, mii + n, sizeof(struct menu_item));
	mii[n].text = text;
	mii[n].rtext = rtext;
	mii[n].func = func;
	mii[n].data = data;
	mii[n].in_m = in_m;
	mii[n].hotkey_pos = 0;
	mii[n].ignore_hotkey = 1;
}
