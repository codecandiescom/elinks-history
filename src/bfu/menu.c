/* Menu system implementation. */
/* $Id: menu.c,v 1.101 2003/09/28 13:02:36 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/hotkey.h"
#include "bfu/menu.h"
#include "bfu/style.h"
#include "config/kbdbind.h"
#include "intl/gettext/libintl.h"
#include "terminal/draw.h"
#include "terminal/event.h"
#include "terminal/kbd.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/color.h"
#include "util/conv.h"
#include "util/memory.h"


/* Types and structures */

struct mainmenu {
	MENU_HEAD;
	int sp;
};

/* Global variables */
unsigned char m_submenu[] = ">>";
unsigned char m_bar = 0;

/* Prototypes */
static void menu_func(struct window *, struct term_event *, int);
static void mainmenu_func(struct window *, struct term_event *, int);


static inline int
count_items(struct menu_item *items)
{
	register int i = 0;

	if (items)
		for (; items[i].text; i++);

	return i;
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

	if (!it->submenu) {
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
		unsigned char *text = menu->items[my].text;
		unsigned char *rtext = menu->items[my].rtext;


		if (text && *text) {
			if (!menu->items[my].no_intl) text = _(text, term);

			if (text[0])
				s += strlen(text)
				     - (menu->items[my].hotkey_pos ? 1 : 0)
				     + 1;
		}

		if (rtext && *rtext) {
			if (!menu->items[my].no_intl) rtext = _(rtext, term);

			if (rtext[0])
				s += MENU_HOTKEY_SPACE + strlen(rtext);
		}

		if (s > mx) mx = s;
	}

	my += 2;

	int_upper_bound(&mx, sx);
	int_upper_bound(&my, sy);

	menu->xw = mx;
	menu->yw = my;

	menu->x = menu->xp;
	menu->y = menu->yp;

	int_bounds(&menu->x, 0, sx - mx);
	int_bounds(&menu->y, 0, sy - my);
}

static void
scroll_menu(struct menu *menu, int d)
{
	int c = 0;
	int w = menu->yw - 2;
	int scr_i = int_min((w - 1) / 2, SCROLL_ITEMS);

	int_lower_bound(&scr_i, 0);
	int_lower_bound(&w, 0);

	menu->selected += d;

	if (menu->ni) {
		menu->selected %= menu->ni;
		if (menu->selected < 0)
			menu->selected += menu->ni;
	}

	while (1) {
		if (c++ > menu->ni) {
			menu->selected = -1;
			menu->view = 0;
			return;
		}

		int_bounds(&menu->selected, 0, menu->ni - 1);

		if (menu->ni && menu->items[menu->selected].rtext != M_BAR)
			break;

		menu->selected += d;
	}

	int_bounds(&menu->view, menu->selected - w + scr_i + 1, menu->selected - scr_i);
	int_bounds(&menu->view, 0, menu->ni - w);
}

static void
display_menu(struct terminal *term, struct menu *menu)
{
	int p, s;
	struct color_pair *normal_color = get_bfu_color(term, "menu.normal");
	struct color_pair *selected_color = get_bfu_color(term, "menu.selected");
	struct color_pair *frame_color = get_bfu_color(term, "menu.frame");
	struct color_pair *hotkey_color = get_bfu_color(term, "menu.hotkey.normal");
	struct color_pair *selected_hotkey_color = get_bfu_color(term, "menu.hotkey.selected");
	int mx = menu->x + 1;
	int mxw = menu->xw - 2;
	int my = menu->y + 1;
	int myw = menu->yw - 2;

	draw_area(term,	mx, my, mxw, myw, ' ', 0, normal_color);
	draw_border(term, menu->x, menu->y, menu->xw, menu->yw, frame_color, 1);

	for (p = menu->view, s = my;
	     p < menu->ni && p < menu->view + myw;
	     p++, s++) {
		struct color_pair *color = normal_color;
		struct color_pair *hkcolor = hotkey_color;
#ifdef DEBUG
		/* Sanity check. */
		if (!menu->items[p].text)
			internal("[%p] menu->items[%d].text == NULL", menu->items[p],
				 menu->items[p].text);
#endif

		if (p == menu->selected) {
			/* This entry is selected. */
			color = selected_color;
			hkcolor = selected_hotkey_color;

			set_cursor(term, mx, s, 1);
			set_window_ptr(menu->win, menu->x + menu->xw, s);
			draw_area(term, mx, s, mxw, 1, ' ', 0, color);
		}

		if (menu->items[p].rtext == M_BAR &&
		    menu->items[p].text && !*menu->items[p].text) {

			/* Horizontal separator */
			draw_border_char(term, menu->x, s,
					 BORDER_SRTEE, frame_color);

			draw_area(term, mx, s, mxw, 1,
				  BORDER_SHLINE, SCREEN_ATTR_FRAME, frame_color);

			draw_border_char(term, mx + mxw, s,
					 BORDER_SLTEE, frame_color);

		} else {
			unsigned char c;
			int x;

			if (menu->items[p].text && *menu->items[p].text) {
				int l = menu->items[p].hotkey_pos;
				unsigned char *text = menu->items[p].text;

				if (!menu->items[p].no_intl) text = _(text, term);

				if (l) {
					int xbase = mx + 1;
					/* There's an hotkey so handle it. */
					int hk = 0;
#ifdef DEBUG
					/* For redundant hotkeys highlighting. */
					int double_hk = 0;

					if (l < 0) l = -l, double_hk = 1;
#endif
					for (x = 0;
					     x < mxw - 2
					     && (c = text[x]);
					     x++) {
						if (!hk && l
						    && x == l - 1) {
							hk = 1;
							continue;
						}

						if (hk == 1) {
#ifdef DEBUG
							draw_char(term, xbase + x - 1, s, c, 0,
								  (double_hk ? selected_hotkey_color : hkcolor));
#else
							draw_char(term, xbase + x - 1, s, c, 0, hkcolor);
#endif
							hk = 2;
						} else {
							draw_char(term, xbase + x - (hk ? 1 : 0), s, c, 0, color);
						}
					}

				} else {
					int xbase = mx + 1;

					/* No hotkey, just left text. */
					for (x = 0;
				     	     x < mxw - 2
				     	     && (c = text[x]);
				     	     x++)
						draw_char(term, xbase + x, s, c, 0, color);
				}
			}

			if (menu->items[p].rtext && *menu->items[p].rtext) {
				unsigned char *rtext = menu->items[p].rtext;

				if (!menu->items[p].no_intl) rtext = _(rtext, term);

				if (*rtext) {
					/* There's a right text, so print it */
					int l = strlen(rtext);
					int xbase = menu->x + mxw - l;

					for (x = l - 1;
					     (x >= 0) && (mxw - 2 >= l - x)
					     && (c = rtext[x]);
				    	     x--)
						draw_char(term, xbase + x, s, c, 0, color);
				}
			}
		}
	}

	redraw_from_window(menu->win);
}


static void
menu_func(struct window *win, struct term_event *ev, int fwd)
{
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
#ifdef USE_MOUSE
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
					struct window *w1;

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
						    menu->items[sel].submenu)
							select_menu(win->term, menu);
					}
				}
			}
#endif /* USE_MOUSE */
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

					int_upper_bound(&step, menu->ni - menu->selected - 1);
					int_upper_bound(&step, DIST);
					int_upper_bound(&step, menu->ni - 1);

					scroll_menu(menu, step);
				}
					break;
#undef DIST
				default:
				{
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

					if (ev->x > ' ' && ev->x < 255) {
						if (check_hotkeys((struct menu_head *)menu, ev->x, win->term))
							s = 1, scroll_menu(menu, 0);
						else if (check_not_so_hot_keys((struct menu_head *)menu, ev->x, win->term))
							scroll_menu(menu, 0);
						break;
					}

				}
					break;
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
		struct term_event ev =
			INIT_TERM_EVENT(EV_KBD, KBD_ENTER, 0, 0);

		term_send_event(term, &ev);
	}
}

static void
display_mainmenu(struct terminal *term, struct mainmenu *menu)
{
	int i;
	int p = 2;
	struct color_pair *normal_color = get_bfu_color(term, "mainmenu.normal");
	struct color_pair *selected_color = get_bfu_color(term, "mainmenu.selected");
	struct color_pair *hotkey_color = get_bfu_color(term, "mainmenu.hotkey.normal");
	struct color_pair *selected_hotkey_color = get_bfu_color(term, "mainmenu.hotkey.selected");

	draw_area(term, 0, 0, term->x, 1, ' ', 0, normal_color);

	for (i = 0; i < menu->ni; i++) {
		int j;
		struct color_pair *co = normal_color;
		struct color_pair *hkco = hotkey_color;
		int hk = 0;
		int key_pos = menu->items[i].hotkey_pos;
		unsigned char c;
		unsigned char *tmptext = menu->items[i].text;

#ifdef DEBUG
		int double_hk = 0;
		if (key_pos < 0) key_pos = -key_pos, double_hk = 1;
#endif

		if (!menu->items[i].no_intl) tmptext = _(tmptext, term);

		if (i == menu->selected) {
			int tmptextlen = strlen(tmptext)
					 - (key_pos ? 1 : 0);

			co = selected_color;
			hkco = selected_hotkey_color;

			draw_area(term, p, 0, 2, 1, ' ', 0, co);
			draw_area(term, p + tmptextlen + 2, 0, 2, 1, ' ', 0, co);
			menu->sp = p;
			set_cursor(term, p, 0, 1);
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
				draw_char(term, p, 0, c, 0, (double_hk ? selected_hotkey_color : hkco));
#else
				draw_char(term, p, 0, c, 0,  hkco);
#endif
				hk = 2;
			} else {
				draw_char(term, p, 0, c, 0, co);
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

	if (!it->submenu) {
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

#ifdef DEBUG
	if (!it->func) {
	       	internal("No menu function");
	} else {
		it->func(term, it->data, menu->data);
	}
#else
	it->func(term, it->data, menu->data);
#endif
}

static void
mainmenu_func(struct window *win, struct term_event *ev, int fwd)
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
#ifdef USE_MOUSE
			if ((ev->b & BM_BUTT) >= B_WHEEL_UP)
				break;

			if ((ev->b & BM_ACT) == B_DOWN && ev->y) {
				delete_window_ev(win, NULL);

			} else if (!ev->y) {
				int p = 2;
				int i;

				for (i = 0; i < menu->ni; i++) {
					int o = p;
					unsigned char *text = menu->items[i].text;

					if (!menu->items[i].no_intl) text = _(text, win->term);

					if (text && text[0])
						p += strlen(text) + 4
							    - (menu->items[i].hotkey_pos
							    ? 1 : 0);

					if (ev->x < o || ev->x >= p)
						continue;

					menu->selected = i;
					display_mainmenu(win->term, menu);
					if ((ev->b & BM_ACT) == B_UP
					    || menu->items[s].submenu) {
						select_mainmenu(win->term,
								menu);
					}
					break;
				}
			}
#endif /* USE_MOUSE */
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

			if (ev->x > ' ' && ev->x < 256 &&
			    check_hotkeys((struct menu_head *)menu, ev->x, win->term))
				s = 2;

			if (!s) {
				delete_window_ev(win, ev->x != KBD_ESC  ? ev
									: NULL);
			} else {

				display_mainmenu(win->term, menu);
				if (s == 2)
					select_mainmenu(win->term, menu);
			}
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
	    void *data, int submenu, int no_intl)
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
	mii[n].submenu = submenu;
	mii[n].no_intl = no_intl;
	mii[n].hotkey_pos = 0;
	mii[n].hotkey_state = HKS_SHOW;
}
