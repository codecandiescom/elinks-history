/* Menu system implementation. */
/* $Id: menu.c,v 1.6 2002/04/28 11:49:57 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <links.h>

#include <bfu/menu.h>
#include <config/kbdbind.h>
#include <intl/language.h>
#include <lowlevel/kbd.h>
#include <lowlevel/terminal.h>


/* Types and structures */

struct mainmenu {
	int selected;
	int sp;
	int ni;
	void *data;
	struct window *win;
	struct menu_item *items;
};

/* Global variables */
unsigned char m_bar = 0;

/* Prototypes */
void menu_func(struct window *, struct event *, int);
void mainmenu_func(struct window *, struct event *, int);


/* do_menu_selected() */
void do_menu_selected(struct terminal *term, struct menu_item *items,
		      void *data, int selected)
{
	struct menu *menu;

	menu = mem_alloc(sizeof(struct menu));
	if (menu) {
		menu->selected = selected;
		menu->view = 0;
		menu->items = items;
		menu->data = data;
		add_window(term, menu_func, menu);
	} else if (items->free_i) {
		int i;

		for (i = 0; items[i].text; i++) {
			if (items[i].free_i & 2) mem_free(items[i].text);
			if (items[i].free_i & 4) mem_free(items[i].rtext);
		}

		mem_free(items);
	}
}

/* do_menu() */
void do_menu(struct terminal *term, struct menu_item *items, void *data)
{
	do_menu_selected(term, items, data, 0);
}

/* select_menu() */
void select_menu(struct terminal *term,	struct menu *menu)
{
	struct menu_item *it = &menu->items[menu->selected];
	void (*func)(struct terminal *, void *, void *);
	void *data1 = it->data;
	void *data2 = menu->data;

	func = it->func;

	if (menu->selected < 0 ||
	    menu->selected >= menu->ni ||
	    it->hotkey == M_BAR)
		return;

	if (!it->in_m) {
		struct window *win, *win1;

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

/* count_menu_size() */
void count_menu_size(struct terminal *term, struct menu *menu)
{
	int sx = term->x;
	int sy = term->y;
	int mx = 4;
	int my;

	for (my = 0; menu->items[my].text; my++) {
		int s;

		s = strlen(_(menu->items[my].text, term)) +
		    strlen(_(menu->items[my].rtext, term)) + 4;

		if (_(menu->items[my].rtext, term)[0] != 0)
			s += MENU_HOTKEY_SPACE;

		if (s > mx) mx = s;
	}

	menu->ni = my;
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

/* scroll_menu() */
void scroll_menu(struct menu *menu, int d)
{
	int c = 0;
	int w = menu->yw - 2;
	int scr_i = (SCROLL_ITEMS > (w - 1) / 2) ? (w - 1) / 2 : SCROLL_ITEMS;

	if (scr_i < 0) scr_i = 0;
	if (w < 0) w = 0;

	menu->selected += d;
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
		if (menu->ni && menu->items[menu->selected].hotkey != M_BAR)
			break;

		menu->selected += d;
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

/* display_menu() */
void display_menu(struct terminal *term, struct menu *menu)
{
	int p, s;

	fill_area(term,	menu->x + 1, menu->y + 1, menu->xw - 2, menu->yw - 2,
		  COLOR_MENU);

	draw_frame(term, menu->x, menu->y, menu->xw, menu->yw,
		   COLOR_MENU_FRAME, 1);

	for (p = menu->view, s = menu->y + 1;
	     p < menu->ni && p < menu->view + menu->yw - 2;
	     p++, s++) {
		unsigned char *tmptext = _(menu->items[p].text, term);
		int h = 0;
		int co = COLOR_MENU;

		if (p == menu->selected) {
			h = 1;
			co = COLOR_MENU_SELECTED;
		}

		if (h) {
			set_cursor(term, menu->x + 1, s, term->x - 1, term->y - 1);
			set_window_ptr(menu->win, menu->x+menu->xw, s);
			fill_area(term, menu->x + 1, s, menu->xw - 2, 1, co);
		}

		if (menu->items[p].hotkey != M_BAR || (tmptext && tmptext[0])) {
			unsigned char c;
			int l = strlen(_(menu->items[p].rtext, term));
			int x;

			for (x = l - 1;
			     (x >= 0) && (menu->xw - 4 >= l - x) &&
			     (c = _(menu->items[p].rtext, term)[x]);
			     x--) {
				set_char(term, menu->x + menu->xw - 2 - l + x, s, c | co);
			}

			for (x = 0; (x < menu->xw - 4) &&
			            (c = tmptext[x]); x++) {
				int ch = co;

				if (!h
				    && strchr(_(menu->items[p].hotkey, term),
					      upcase(c))) {
					h = 1;
					ch = COLOR_MENU_HOTKEY;
				}

				set_char(term, menu->x + x + 2, s, ch | c);
			}

		} else {
			set_char(term, menu->x, s,
				 COLOR_MENU_FRAME | ATTR_FRAME | 0xc3);

			fill_area(term, menu->x + 1, s, menu->xw - 2, 1,
				  COLOR_MENU_FRAME | ATTR_FRAME | 0xc4);

			set_char(term, menu->x + menu->xw - 1, s,
				 COLOR_MENU_FRAME | ATTR_FRAME | 0xb4);
		}
	}

	redraw_from_window(menu->win);
}

/* menu_func() */
void menu_func(struct window *win, struct event *ev, int fwd)
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
					int s = ev->y - menu->y - 1 + menu->view;

					if (s >= 0 && s < menu->ni &&
					    menu->items[s].hotkey != M_BAR) {

						menu->selected = s;
						scroll_menu(menu, 0);
						display_menu(win->term, menu);

						if ((ev->b & BM_ACT) == B_UP ||
						    menu->items[s].in_m)
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

				case ACT_PAGE_UP:
					menu->selected -= menu->yw - 3;
					if (menu->selected < -1)
						menu->selected = -1;

					menu->view -= menu->yw - 2;
					if (menu->view < 0)
						menu->view = 0;

					scroll_menu(menu, -1);
					break;

				case ACT_PAGE_DOWN:
					menu->selected += menu->yw - 3;
					if (menu->selected > menu->ni)
						menu->selected = menu->ni;

					menu->view += menu->yw - 2;
					if (menu->view >= menu->ni - menu->yw + 2)
						menu->view = menu->ni - menu->yw + 2;

					scroll_menu(menu, 1);
					break;

				default:
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

					if (ev->x > ' ' && ev->x < 256) {
						int i;

						for (i = 0; i < menu->ni; i++) {
							if (strchr(_(menu->items[i].hotkey, win->term),
								   upcase(ev->x))) {
								menu->selected = i;
								scroll_menu(menu, 0);
								s = 1;
							}
						}

						if (s == 0) {
							for (i = 0; i < menu->ni; i++) {
								if (menu->items[i].text &&
								    (upcase(menu->items[i].text[0])
								     == upcase(ev->x))) {
									if (upcase(menu->items[menu->selected].text[0]) ==
									    upcase(ev->x) &&
									    menu->selected >= i) continue;
									menu->selected = i;
 									scroll_menu(menu, 0);
 									break;
								}
							}
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
			if (menu->items->free_i) {
				int i;

				for (i = 0; menu->items[i].text; i++) {
					if (menu->items[i].free_i & 2)
						mem_free(menu->items[i].text);
					if (menu->items[i].free_i & 4)
						mem_free(menu->items[i].rtext);
				}

				mem_free(menu->items);
			}

			break;
	}
}

/* do_mainmenu() */
void do_mainmenu(struct terminal *term,	struct menu_item *items,
		 void *data, int sel)
{
	struct mainmenu *menu;

	menu = mem_alloc(sizeof(struct mainmenu));
	if (!menu) return;

	menu->selected = (sel == -1 ? 0 : sel);
	menu->items = items;
	menu->data = data;
	add_window(term, mainmenu_func, menu);

	if (sel != -1) {
		struct event ev = {EV_KBD, KBD_ENTER, 0, 0};
		struct window *win = term->windows.next;

		win->handler(win, &ev, 0);
	}
}

/* display_mainmenu() */
void display_mainmenu(struct terminal *term, struct mainmenu *menu)
{
	int i;
	int p = 2;

	fill_area(term, 0, 0, term->x, 1, COLOR_MAINMENU | ' ');
	for (i = 0; menu->items[i].text; i++) {
		int s = 0;
		int j;
		int co;
		unsigned char c;
		unsigned char *tmptext = _(menu->items[i].text, term);

		if (i == menu->selected) {
			s = 1;
			co = COLOR_MAINMENU_SELECTED;
		} else {
			co = COLOR_MAINMENU;
		}

		if (s) {
			fill_area(term, p, 0, 2, 1, co);
			fill_area(term, p+strlen(tmptext)+2, 0, 2, 1, co);
			menu->sp = p;
			set_cursor(term, p, 0, term->x - 1, term->y - 1);
			set_window_ptr(menu->win, p, 1);
		}

		p += 2;

		for (j = 0; (c = tmptext[j]); j++, p++) {
			if (!s && strchr(_(menu->items[i].hotkey, term),
					 upcase(c))) {
				s = 1;
				set_char(term, p, 0,
					 COLOR_MAINMENU_HOTKEY | c);
			} else {
				set_char(term, p, 0,
					 co | c);
			}
		}

		p += 2;
	}

	menu->ni = i;
	redraw_from_window(menu->win);
}

/* select_mainmenu() */
void select_mainmenu(struct terminal *term, struct mainmenu *menu)
{
	struct menu_item *it = &menu->items[menu->selected];

	if (menu->selected < 0 || menu->selected >= menu->ni
	    || it->hotkey == M_BAR)
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

/* mainmenu_func() */
void mainmenu_func(struct window *win, struct event *ev, int fwd)
{
	int s = 0;
	struct mainmenu *menu = win->data;

	menu->win = win;
	switch(ev->ev) {
		case EV_INIT:
		case EV_RESIZE:
		case EV_REDRAW:
			display_mainmenu(win->term, menu);
			break;

		case EV_MOUSE:
			if ((ev->b & BM_ACT) == B_DOWN && ev->y) {
				delete_window_ev(win, ev);

			} else if (!ev->y) {
				int p = 2;
				int i;

				for (i = 0; i < menu->ni; i++) {
					int o = p;
					unsigned char *tmptext;

					tmptext = _(menu->items[i].text,
						    win->term);
					p += strlen(tmptext) + 4;

					if (ev->x >= o && ev->x < p) {
						menu->selected = i;
						display_mainmenu(win->term,
								 menu);
						if ((ev->b & BM_ACT) == B_UP
						    || menu->items[s].in_m) {
							/* Uh-oh! But I will
							 * fit here! --pasky */
							select_mainmenu(
								win->term,
								menu);
						}
						break;
					}
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
					if (strchr(_(menu->items[i].hotkey,
						     win->term),
						   upcase(ev->x))) {
						menu->selected = i;
						s = 2;
					}

			} else if (!s) {
				delete_window_ev(win, ev->x != KBD_ESC ? ev
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

/* new_menu() */
struct menu_item *new_menu(int free_i)
{
	struct menu_item *mi;

	mi = mem_alloc(sizeof(struct menu_item));
	if (!mi) return NULL;
	memset(mi, 0, sizeof(struct menu_item));
	mi->free_i = free_i;
	return mi;
}

/* add_to_menu() */
void add_to_menu(struct menu_item **mi, unsigned char *text,
		 unsigned char *rtext, unsigned char *hotkey,
		 void (*func)(struct terminal *, void *, void *),
		 void *data, int in_m)
{
	struct menu_item *mii;
	int n;

	for (n = 0; (*mi)[n].text; n++);

	mii = mem_realloc(*mi, (n + 2) * sizeof(struct menu_item));
	if (!mii) return;

	*mi = mii;
	memcpy(mii + n + 1, mii + n, sizeof(struct menu_item));
	mii[n].text = text;
	mii[n].rtext = rtext;
	mii[n].hotkey = hotkey;
	mii[n].func = func;
	mii[n].data = data;
	mii[n].in_m = in_m;
}
