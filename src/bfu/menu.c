/* Menu system implementation. */
/* $Id: menu.c,v 1.191 2004/01/25 12:52:55 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/hotkey.h"
#include "bfu/menu.h"
#include "bfu/style.h"
#include "config/kbdbind.h"
#include "dialogs/menu.h"		/* For do_action() */
#include "intl/gettext/libintl.h"
#include "sched/action.h"
#include "terminal/draw.h"
#include "terminal/event.h"
#include "terminal/kbd.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/color.h"
#include "util/conv.h"
#include "util/memory.h"

/* Left and right main menu reserved spaces. */
#define L_MAINMENU_SPACE	2
#define R_MAINMENU_SPACE	2

/* Left and right padding spaces around labels in main menu. */
#define L_MAINTEXT_SPACE	1
#define R_MAINTEXT_SPACE	1

/* Spaces before and after right text of submenu. */
#define L_RTEXT_SPACE		1
#define R_RTEXT_SPACE		1

/* Spaces before and after left text of submenu. */
#define L_TEXT_SPACE		1
#define R_TEXT_SPACE		1

/* Border size in submenu. */
#define MENU_BORDER_SIZE	1


/* Types and structures */

struct mainmenu {
	MENU_HEAD;
	int sp;
	int first_displayed;
	int last_displayed;
};

/* Submenu indicator, displayed at right. */
static unsigned char m_submenu[] = ">>";
static int m_submenu_len = sizeof(m_submenu) - 1;

/* Prototypes */
static void menu_handler(struct window *, struct term_event *, int);
static void mainmenu_handler(struct window *, struct term_event *, int);


static inline int
count_items(struct menu_item *items)
{
	register int i = 0;

	if (items)
		for (; !mi_is_end_of_menu(items[i]); i++);

	return i;
}

static void
free_menu_items(struct menu_item *items)
{
	int i;

	if (!items) return;

	/* Note that flags & FREE_DATA applies only when menu is aborted;
	 * it is zeroed when some menu field is selected. */

	for (i = 0; !mi_is_end_of_menu(items[i]); i++) {
		if (items[i].flags & FREE_TEXT && items[i].text)
			mem_free(items[i].text);
		if (items[i].flags & FREE_RTEXT && items[i].rtext)
			mem_free(items[i].rtext);
		if (items[i].flags & FREE_DATA && items[i].data)
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
		add_window(term, menu_handler, menu);
	} else if (items->flags & FREE_ANY) {
		free_menu_items(items);
	}
}

void
do_menu(struct terminal *term, struct menu_item *items, void *data, int hotkeys)
{
	do_menu_selected(term, items, data, 0, hotkeys);
}

static void
select_menu_item(struct terminal *term, struct menu_item *it, void *data)
{
	/* We save these values due to delete_window() call below. */
	menu_func func = it->func;
	void *it_data = it->data;
	enum main_action action = it->action;

	if (mi_is_unselectable(*it)) return;

	if (!mi_is_submenu(*it)) {
		/* Don't free data! */
		it->flags &= ~FREE_DATA;

		while (!list_empty(term->windows)) {
			struct window *win = term->windows.next;

			if (win->handler != menu_handler
			    && win->handler != mainmenu_handler)
				break;

			delete_window(win);
		}
	}

	if (action != ACT_MENU_NONE && !func) {
		struct session *ses = data;

		do_action(ses, action, 1);
		return;
	}

	assertm(func, "No menu function");
	if_assert_failed return;

	func(term, it_data, data);
}

static inline void
select_menu(struct terminal *term, struct menu *menu)
{
	if (menu->selected < 0 || menu->selected >= menu->ni)
		return;

	select_menu_item(term, &menu->items[menu->selected], menu->data);
}

/* Get desired width for left text in menu item, accounting spacing. */
static int
get_menuitem_text_width(struct terminal *term, struct menu_item *mi)
{
	unsigned char *text;

	if (!mi_has_left_text(*mi)) return 0;

	text = mi->text;
	if (mi_text_translate(*mi))
		text = _(text, term);

	if (!text[0]) return 0;

	return L_TEXT_SPACE + strlen(text) - !!mi->hotkey_pos + R_TEXT_SPACE;
}

/* Get desired width for right text in menu item, accounting spacing. */
static int
get_menuitem_rtext_width(struct terminal *term, struct menu_item *mi)
{
	int rtext_width = 0;

	if (mi_is_submenu(*mi)) {
		rtext_width = L_RTEXT_SPACE + m_submenu_len + R_RTEXT_SPACE;

	} else if (mi->action != ACT_MENU_NONE) {
		struct string keystroke;

		if (init_string(&keystroke)) {
			add_keystroke_to_string(&keystroke, mi->action, KM_MAIN);
			rtext_width = L_RTEXT_SPACE + keystroke.length + R_RTEXT_SPACE;
			done_string(&keystroke);
		}

	} else if (mi_has_right_text(*mi)) {
		unsigned char *rtext = mi->rtext;

		if (mi_rtext_translate(*mi))
			rtext = _(rtext, term);

		if (rtext[0])
			rtext_width = L_RTEXT_SPACE + strlen(rtext) + R_RTEXT_SPACE;
	}

	return rtext_width;
}

static int
get_menuitem_width(struct terminal *term, struct menu_item *mi, int max_width)
{
	int text_width = get_menuitem_text_width(term, mi);
	int rtext_width = get_menuitem_rtext_width(term, mi);

	int_upper_bound(&text_width, max_width);
	int_upper_bound(&rtext_width, max_width - text_width);

	return text_width + rtext_width;
}

static void
count_menu_size(struct terminal *term, struct menu *menu)
{
	int width = term->width - MENU_BORDER_SIZE * 2;
	int height = term->height - MENU_BORDER_SIZE * 2;
	int mx = 0;
	int my;

	for (my = 0; my < menu->ni; my++)
		int_lower_bound(&mx, get_menuitem_width(term, &menu->items[my], width));

	int_upper_bound(&my, height);

	menu->width = mx + MENU_BORDER_SIZE * 2;
	menu->height = my + MENU_BORDER_SIZE * 2;

	menu->x = menu->parent_x;
	menu->y = menu->parent_y;

	int_bounds(&menu->x, 0, width - mx);
	int_bounds(&menu->y, 0, height - my);
}

static void
scroll_menu(struct menu *menu, int d)
{
	int w = int_max(1, menu->height - MENU_BORDER_SIZE * 2);
	int scr_i = int_min((w - 1) / 2, SCROLL_ITEMS);

	if (!d) return;

	int_lower_bound(&scr_i, 0);
	int_lower_bound(&w, 0);

	if (menu->ni < 1) {
		menu->selected = -1;
		menu->view = 0;
		return;
	}

	menu->selected += d;

	menu->selected %= menu->ni;
	if (menu->selected < 0)
		menu->selected += menu->ni;

	int_bounds(&menu->selected, 0, menu->ni - 1);
	while (!mi_is_selectable(menu->items[menu->selected])) {
		menu->selected += d/abs(d);

		if (menu->selected < 0 || menu->selected >= menu->ni) {
			menu->selected = -1;
			menu->view = 0;
			return;
		}
	}

	int_bounds(&menu->view, menu->selected - w + scr_i + 1, menu->selected - scr_i);
	int_bounds(&menu->view, 0, menu->ni - w);
}

static inline void
draw_menu_left_text(struct terminal *term, unsigned char *text, int len,
		    int x, int y, int width, struct color_pair *color)
{
	int xbase = x + L_TEXT_SPACE;
	int w = width - (L_TEXT_SPACE + R_TEXT_SPACE);

	if (w <= 0) return;

	if (len < 0) len = strlen(text);
	if (!len) return;
	if (len > w) len = w;

	for (x = 0; len; x++, len--)
		draw_char(term, xbase + x, y, text[x], 0, color);
}


static inline void
draw_menu_left_text_hk(struct terminal *term, unsigned char *text,
		       int hotkey_pos, int x, int y, int width,
		       struct color_pair *color, int selected)
{
	struct color_pair *hk_color = get_bfu_color(term, "menu.hotkey.normal");
	struct color_pair *hk_color_sel = get_bfu_color(term, "menu.hotkey.selected");
	enum screen_char_attr hk_attr = get_opt_bool("ui.dialogs.underline_hotkeys")
				      ? SCREEN_ATTR_UNDERLINE : 0;
	unsigned char c;
	int xbase = x + L_TEXT_SPACE;
	int w = width - (L_TEXT_SPACE + R_TEXT_SPACE);
	int hk = 0;
#ifdef DEBUG
	/* For redundant hotkeys highlighting. */
	int double_hk = 0;

	if (hotkey_pos < 0) hotkey_pos = -hotkey_pos, double_hk = 1;
#endif

	if (!hotkey_pos || w <= 0) return;

	if (selected) {
		struct color_pair *tmp = hk_color;

		hk_color = hk_color_sel;
		hk_color_sel = tmp;
	}

	for (x = 0;
	     x < w + !!hk
	     && (c = text[x]);
	     x++) {
		if (!hk && x == hotkey_pos - 1) {
			hk = 1;
			continue;
		}

		if (hk == 1) {
#ifdef DEBUG
			draw_char(term, xbase + x - 1, y, c, hk_attr,
				  (double_hk ? hk_color_sel : hk_color));
#else
			draw_char(term, xbase + x - 1, y, c, hk_attr, hk_color);
#endif
			hk = 2;
		} else {
			draw_char(term, xbase + x - !!hk, y, c, 0, color);
		}
	}
}

static inline void
draw_menu_right_text(struct terminal *term, unsigned char *text, int len,
		     int x, int y, int width, struct color_pair *color)
{
	int xbase = x;
	int w = width - (L_RTEXT_SPACE + R_RTEXT_SPACE);

	if (w <= 0) return;

	if (len < 0) len = strlen(text);
	if (!len) return;
	if (len > w) len = w;

	xbase += w - len + L_RTEXT_SPACE + L_TEXT_SPACE;

	for (x = 0; len; x++, len--)
		draw_char(term, xbase + x, y, text[x], 0, color);
}

static void
display_menu(struct terminal *term, struct menu *menu)
{
	struct color_pair *normal_color = get_bfu_color(term, "menu.normal");
	struct color_pair *selected_color = get_bfu_color(term, "menu.selected");
	struct color_pair *frame_color = get_bfu_color(term, "menu.frame");
	int mx = menu->x + MENU_BORDER_SIZE;
	int mwidth = int_max(0, menu->width - MENU_BORDER_SIZE * 2);
	int my = menu->y + MENU_BORDER_SIZE;
	int mheight = int_max(0, menu->height - MENU_BORDER_SIZE * 2);
	int p, y;

	draw_area(term,	mx, my, mwidth, mheight, ' ', 0, normal_color);
	draw_border(term, menu->x, menu->y, menu->width, menu->height, frame_color, 1);

	for (p = menu->view, y = my;
	     p < menu->ni && p < menu->view + mheight;
	     p++, y++) {
		struct color_pair *color = normal_color;

#ifdef DEBUG
		/* Sanity check. */
		if (mi_is_end_of_menu(menu->items[p]))
			INTERNAL("Unexpected end of menu [%p:%d]", menu->items[p], p);
#endif

		if (p == menu->selected) {
			/* This entry is selected. */
			color = selected_color;

			set_cursor(term, mx, y, 1);
			set_window_ptr(menu->win, menu->x + menu->width, y);
			draw_area(term, mx, y, mwidth, 1, ' ', 0, color);
		}

		if (mi_is_horizontal_bar(menu->items[p])) {
			/* Horizontal separator */
			draw_border_char(term, menu->x, y,
					 BORDER_SRTEE, frame_color);

			draw_area(term, mx, y, mwidth, 1,
				  BORDER_SHLINE, SCREEN_ATTR_FRAME, frame_color);

			draw_border_char(term, mx + mwidth, y,
					 BORDER_SLTEE, frame_color);

		} else {
			if (mi_has_left_text(menu->items[p])) {
				int l = menu->items[p].hotkey_pos;
				unsigned char *text = menu->items[p].text;

				if (mi_text_translate(menu->items[p]))
					text = _(text, term);

				if (mi_is_unselectable(menu->items[p]))
					l = 0;

				if (l) {
					draw_menu_left_text_hk(term, text, l,
							       mx, y, mwidth, color,
							       (p == menu->selected));

				} else {
					draw_menu_left_text(term, text, -1,
							    mx, y, mwidth, color);
		  		}
			}

			if (mi_is_submenu(menu->items[p])) {
				draw_menu_right_text(term, m_submenu, m_submenu_len,
						     menu->x, y, mwidth, color);
			} else if (menu->items[p].action != ACT_MENU_NONE) {
				struct string keystroke;

#ifdef DEBUG
				/* Help to detect action + right text. --Zas */
				if (mi_has_right_text(menu->items[p])) {
					if (color == selected_color)
						color = normal_color;
					else
						color = selected_color;
				}
#endif /* DEBUG */

				if (init_string(&keystroke)) {
					add_keystroke_to_string(&keystroke,
								menu->items[p].action,
								KM_MAIN);
					draw_menu_right_text(term, keystroke.source,
							     keystroke.length,
							     menu->x, y, mwidth, color);
					done_string(&keystroke);
				}

			} else if (mi_has_right_text(menu->items[p])) {
				unsigned char *rtext = menu->items[p].rtext;

				if (mi_rtext_translate(menu->items[p]))
					rtext = _(rtext, term);

				if (*rtext) {
					/* There's a right text, so print it */
					draw_menu_right_text(term, rtext, -1,
							     menu->x, y, mwidth, color);
				}
			}
		}
	}

	redraw_from_window(menu->win);
}


#ifdef CONFIG_MOUSE
static void
menu_mouse_handler(struct menu *menu, struct term_event *ev)
{
	struct window *win = menu->win;

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

	if (ev->x < menu->x
	    || ev->x >= menu->x + menu->width
	    || ev->y < menu->y
	    || ev->y >= menu->y + menu->height) {
		if ((ev->b & BM_ACT) == B_DOWN) {
			delete_window_ev(win, NULL);

		} else {
			struct window *w1;

			for (w1 = win;
			     (void *)w1 != &win->term->windows;
			     w1 = w1->next) {
				struct menu *m1;

				if (w1->handler == mainmenu_handler) {
					if (!ev->y)
						delete_window_ev(win, ev);
					break;
				}

				if (w1->handler != menu_handler) break;

				m1 = w1->data;

				if (ev->x > m1->x
				    && ev->x < m1->x + m1->width - 1
				    && ev->y > m1->y
				    && ev->y < m1->y + m1->height - 1)
					delete_window_ev(win, ev);
			}
		}

	} else {
		if (ev->x >=  menu->x
		    && ev->x < menu->x + menu->width
		    && ev->y >=  menu->y + 1
		    && ev->y < menu->y + menu->height - 1) {
			int sel = ev->y - menu->y - 1 + menu->view;

			if (sel >= 0 && sel < menu->ni
			    && mi_is_selectable(menu->items[sel])) {
				menu->selected = sel;
				scroll_menu(menu, 0);
				display_menu(win->term, menu);

				if ((ev->b & BM_ACT) == B_UP ||
				    mi_is_submenu(menu->items[sel]))
					select_menu(win->term, menu);
			}
		}
	}
}
#endif

#define DIST 5

static void
menu_page_up(struct menu *menu)
{
	int current = int_max(0, int_min(menu->selected, menu->ni - 1));
	int step;
	int i;
	int next_sep = 0;

	for (i = current - 1; i > 0; i--)
		if (mi_is_horizontal_bar(menu->items[i])) {
			next_sep = i;
			break;
		}

	step = current - next_sep + 1;
	int_bounds(&step, 0, int_min(current, DIST));

	scroll_menu(menu, -step);
}

static void
menu_page_down(struct menu *menu)
{
	int current = int_max(0, int_min(menu->selected, menu->ni - 1));
	int step;
	int i;
	int next_sep = menu->ni - 1;

	for (i = current + 1; i < menu->ni; i++)
		if (mi_is_horizontal_bar(menu->items[i])) {
			next_sep = i;
			break;
		}

	step = next_sep - current + 1;
	int_bounds(&step, 0, int_min(menu->ni - 1 - current, DIST));

	scroll_menu(menu, step);
}

#undef DIST

static void
menu_kbd_handler(struct menu *menu, struct term_event *ev)
{
	struct window *win = menu->win;
	enum menu_action action = kbd_action(KM_MENU, ev, NULL);
	int s = 0;

	switch (action) {
		case ACT_MENU_LEFT:
		case ACT_MENU_RIGHT:
			if ((void *) win->next != &win->term->windows
			    && win->next->handler == mainmenu_handler) {
				delete_window_ev(win, ev);
				goto break2;
			}

			if (action == ACT_MENU_RIGHT)
				goto enter;

			delete_window(win);

			goto break2;

		case ACT_MENU_UP:
			scroll_menu(menu, -1);
			break;

		case ACT_MENU_DOWN:
			scroll_menu(menu, 1);
			break;

		case ACT_MENU_HOME:
			menu->selected = -1;
			scroll_menu(menu, 1);
			break;

		case ACT_MENU_END:
			menu->selected = menu->ni;
			scroll_menu(menu, -1);
			break;

		case ACT_MENU_PAGE_UP:
			menu_page_up(menu);
			break;

		case ACT_MENU_PAGE_DOWN:
			menu_page_down(menu);
			break;

		case ACT_MENU_ENTER:
		case ACT_MENU_SELECT:
			goto enter;

		case ACT_MENU_CANCEL:
			if ((void *) win->next != &win->term->windows
			    && win->next->handler == mainmenu_handler)
				delete_window_ev(win, ev);
			else
				delete_window_ev(win, NULL);

			goto break2;

		default:
		{
			if ((ev->x >= KBD_F1 && ev->x <= KBD_F12) ||
			    ev->y == KBD_ALT) {
				delete_window_ev(win, ev);
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
	if (s) {
enter:
		select_menu(win->term, menu);
	}

break2:
	return;
}

static void
menu_handler(struct window *win, struct term_event *ev, int fwd)
{
	struct menu *menu = win->data;

	menu->win = win;

	switch (ev->ev) {
		case EV_INIT:
		case EV_RESIZE:
		case EV_REDRAW:
			get_parent_ptr(win, &menu->parent_x, &menu->parent_y);
			count_menu_size(win->term, menu);
			menu->selected--;
			scroll_menu(menu, 1);
			display_menu(win->term, menu);
			break;

		case EV_MOUSE:
#ifdef CONFIG_MOUSE
			menu_mouse_handler(menu, ev);
#endif /* CONFIG_MOUSE */
			break;

		case EV_KBD:
			menu_kbd_handler(menu, ev);
			break;

		case EV_ABORT:
			if (menu->items->flags & FREE_ANY)
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
	add_window(term, mainmenu_handler, menu);

	if (sel != -1) {
		struct term_event ev =
			INIT_TERM_EVENT(EV_KBD, KBD_ENTER, 0, 0);

		term_send_event(term, &ev);
	}
}

static void
display_mainmenu(struct terminal *term, struct mainmenu *menu)
{
	struct color_pair *normal_color = get_bfu_color(term, "menu.normal");
	struct color_pair *selected_color = get_bfu_color(term, "menu.selected");
	int p = 0;
	int i;

	/* FIXME: menu horizontal scrolling do not work well yet, we need to cache
	 * menu items width and recalculate them only when needed (ie. language change)
	 * instead of looping and calculate them each time. --Zas */

	/* Try to make current selected menu entry visible. */
	while (1) {
		if (menu->selected < menu->first_displayed) {
			menu->first_displayed--;
			menu->last_displayed--;

		} else if (menu->selected > menu->last_displayed) {
			menu->first_displayed++;
			menu->last_displayed++;
		} else
			break;
	}

	if (menu->last_displayed <= 0)
		menu->last_displayed = menu->ni - 1;

	int_bounds(&menu->last_displayed, 0, menu->ni - 1);
	int_bounds(&menu->first_displayed, 0, menu->last_displayed);

	draw_area(term, 0, 0, term->width, 1, ' ', 0, normal_color);

	if (menu->first_displayed != 0)
		draw_area(term, 0, 0, L_MAINMENU_SPACE, 1, '<', 0, normal_color);

	p += L_MAINMENU_SPACE;

	for (i = menu->first_displayed; i < menu->ni; i++) {
		struct color_pair *color = normal_color;
		unsigned char *text = menu->items[i].text;
		int l = menu->items[i].hotkey_pos;
		int textlen;

		if (mi_text_translate(menu->items[i]))
			text = _(text, term);

		textlen = strlen(text) - !!l;

		if (i == menu->selected) {
			menu->sp = p;
			color = selected_color;
			draw_area(term, p, 0,
				  L_MAINTEXT_SPACE + L_TEXT_SPACE
				  + textlen
				  + R_TEXT_SPACE + R_MAINTEXT_SPACE,
				  1, ' ', 0, color);
			set_cursor(term, p, 0, 1);
			set_window_ptr(menu->win, p, 1);
		}

		p += L_MAINTEXT_SPACE;

		if (l) {
			draw_menu_left_text_hk(term, text, l,
					       p, 0, textlen + R_TEXT_SPACE + L_TEXT_SPACE,
					       color, (i == menu->selected));
		} else {
			draw_menu_left_text(term, text, textlen,
					    p, 0, textlen + R_TEXT_SPACE + L_TEXT_SPACE,
					    color);
		}

		p += textlen;

		if (p >= term->width - R_MAINMENU_SPACE)
			break;

		p += R_MAINTEXT_SPACE + R_TEXT_SPACE + L_TEXT_SPACE;
	}

	menu->last_displayed = i - 1;
	int_lower_bound(&menu->last_displayed, menu->first_displayed);
	if (menu->last_displayed < menu->ni - 1)
		draw_area(term, term->width - R_MAINMENU_SPACE, 0, R_MAINMENU_SPACE, 1, '>', 0, normal_color);

	redraw_from_window(menu->win);
}

static inline void
select_mainmenu(struct terminal *term, struct mainmenu *menu)
{
	if (menu->selected < 0 || menu->selected >= menu->ni)
		return;

	select_menu_item(term, &menu->items[menu->selected], menu->data);
}


#ifdef CONFIG_MOUSE
static void
mainmenu_mouse_handler(struct mainmenu *menu, struct term_event *ev)
{
	struct window *win = menu->win;

	if ((ev->b & BM_BUTT) >= B_WHEEL_UP)
		return;

	if ((ev->b & BM_ACT) == B_DOWN && ev->y) {
		delete_window_ev(win, NULL);

	} else if (!ev->y) {
		int p = L_MAINMENU_SPACE;
		int i;

		/* We don't initialize to
		 * menu->first_displayed here,
		 * since it breaks horizontal
		 * scrolling using mouse in some
		 * cases. --Zas */
		for (i = 0; i < menu->ni; i++) {
			int o = p;

			if (mi_has_left_text(menu->items[i])) {
				unsigned char *text = menu->items[i].text;

				if (mi_text_translate(menu->items[i]))
					text = _(text, win->term);

				p += L_MAINTEXT_SPACE + L_TEXT_SPACE
				     + strlen(text)
				     - !!menu->items[i].hotkey_pos
				     + R_TEXT_SPACE + R_MAINTEXT_SPACE;
			}

			if (ev->x < o) {
				if (ev->x > L_MAINMENU_SPACE)
					continue;

				menu->selected--;
				if (menu->selected < 0)
					menu->selected = menu->ni - 1;

			} else if (ev->x >= p) {
				if (ev->x < win->term->width - R_MAINMENU_SPACE)
					continue;

				menu->selected++;
				if (menu->selected > menu->ni - 1)
					menu->selected = 0;

			} else {
				menu->selected = i;
			}

			display_mainmenu(win->term, menu);

			if ((ev->b & BM_ACT) == B_UP
			    || mi_is_submenu(menu->items[menu->selected])) {
				select_mainmenu(win->term,
						menu);
			}
			break;
		}
	}
}
#endif

static void
mainmenu_kbd_handler(struct mainmenu *menu, struct term_event *ev, int fwd)
{
	struct window *win = menu->win;
	enum menu_action action = kbd_action(KM_MENU, ev, NULL);
	int s = 0;

	if (action == ACT_MENU_ENTER
	    || action == ACT_MENU_DOWN
	    || action == ACT_MENU_UP
	    || action == ACT_MENU_PAGE_UP
	    || action == ACT_MENU_PAGE_DOWN) {
		select_mainmenu(win->term, menu);
		return;
	}

	if (action == ACT_MENU_LEFT) {
		menu->selected--;
		if (menu->selected < 0)
			menu->selected = menu->ni - 1;
		s = 1;

	} else if (action == ACT_MENU_RIGHT) {
		menu->selected++;
		if (menu->selected >= menu->ni)
			menu->selected = 0;
		s = 1;
	}

	if (fwd && (action == ACT_MENU_LEFT || action == ACT_MENU_RIGHT)) {
		display_mainmenu(win->term, menu);
		select_mainmenu(win->term, menu);
		return;
	}

	if (ev->x > ' ' && ev->x < 256 &&
	    check_hotkeys((struct menu_head *)menu, ev->x, win->term))
		s = 2;

	if (!s) {
		delete_window_ev(win, action != ACT_MENU_CANCEL
				      ? ev : NULL);
	} else {
		display_mainmenu(win->term, menu);
		if (s == 2)
			select_mainmenu(win->term, menu);
	}
}

static void
mainmenu_handler(struct window *win, struct term_event *ev, int fwd)
{
	struct mainmenu *menu = win->data;

	menu->win = win;

	switch (ev->ev) {
		case EV_INIT:
		case EV_RESIZE:
		case EV_REDRAW:
			display_mainmenu(win->term, menu);
			break;

		case EV_MOUSE:
#ifdef CONFIG_MOUSE
			mainmenu_mouse_handler(menu, ev);
#endif /* CONFIG_MOUSE */
			break;

		case EV_KBD:
			mainmenu_kbd_handler(menu, ev, fwd);
			break;

		case EV_ABORT:
			break;
	}
}

/* For dynamic menus the last (cleared) item is used to mark the end. */
#define realloc_menu_items(mi_, size) \
	mem_align_alloc(mi_, size, (size) + 2, sizeof(struct menu_item), 0xF)

struct menu_item *
new_menu(enum menu_item_flags flags)
{
	struct menu_item *mi = NULL;

	if (realloc_menu_items(&mi, 0)) mi->flags = flags;

	return mi;
}

void
add_to_menu(struct menu_item **mi, unsigned char *text, unsigned char *rtext,
	    enum main_action action, menu_func func, void *data,
	    enum menu_item_flags flags)
{
	int n = count_items(*mi);
	/* XXX: Don't clear the last and special item. */
	struct menu_item *item = realloc_menu_items(mi, n + 1);

	if (!item) return;

	item += n;

	/* Shift current last item by one place. */
	memcpy(item + 1, item, sizeof(struct menu_item));

	/* Setup the new item. All menu items share the item_free value. */
	SET_MENU_ITEM(item, text, rtext, action, func, data,
		      item->flags | flags, HKS_SHOW, 0);
}

#undef L_MAINMENU_SPACE
#undef R_MAINMENU_SPACE
#undef L_MAINTEXT_SPACE
#undef R_MAINTEXT_SPACE
#undef L_RTEXT_SPACE
#undef R_RTEXT_SPACE
#undef L_TEXT_SPACE
#undef R_TEXT_SPACE
#undef MENU_BORDER_SIZE
