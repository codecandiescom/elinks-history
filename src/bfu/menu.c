/* Menu system implementation. */
/* $Id: menu.c,v 1.234 2004/06/14 00:53:47 jonas Exp $ */

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
#include "intl/gettext/libintl.h"
#include "sched/action.h"
#include "terminal/draw.h"
#include "terminal/event.h"
#include "terminal/kbd.h"
#include "terminal/mouse.h"
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

/* Submenu indicator, displayed at right. */
static unsigned char m_submenu[] = ">>";
static int m_submenu_len = sizeof(m_submenu) - 1;

/* Prototypes */
static void menu_handler(struct window *, struct term_event *, int);
static void mainmenu_handler(struct window *, struct term_event *, int);


static inline int
count_items(struct menu_item *items)
{
	struct menu_item *item;
	register int i = 0;

	if (items)
		foreach_menu_item (item, items) i++;

	return i;
}

static void
free_menu_items(struct menu_item *items)
{
	struct menu_item *item;

	if (!items) return;

	/* Note that flags & FREE_DATA applies only when menu is aborted;
	 * it is zeroed when some menu field is selected. */

	foreach_menu_item (item, items) {
		if (item->flags & FREE_TEXT) mem_free_if(item->text);
		if (item->flags & FREE_RTEXT) mem_free_if(item->rtext);
		if (item->flags & FREE_DATA) mem_free_if(item->data);
	}

	mem_free(items);
}

void
do_menu_selected(struct terminal *term, struct menu_item *items,
		 void *data, int selected, int hotkeys)
{
	struct menu *menu = mem_calloc(1, sizeof(struct menu));

	if (menu) {
		menu->selected = selected;
		menu->items = items;
		menu->data = data;
		menu->size = count_items(items);
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

	if (action != ACT_MAIN_NONE && !func) {
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
	if (menu->selected < 0 || menu->selected >= menu->size)
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

	} else if (mi->action != ACT_MAIN_NONE) {
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
	struct menu_item *item;
	int width = term->width - MENU_BORDER_SIZE * 2;
	int height = term->height - MENU_BORDER_SIZE * 2;
	int my = int_min(menu->size, height);
	int mx = 0;

	foreach_menu_item (item, menu->items)
		int_lower_bound(&mx, get_menuitem_width(term, item, width));

	set_box(&menu->box,
		menu->parent_x, menu->parent_y,
		mx + MENU_BORDER_SIZE * 2,
		my + MENU_BORDER_SIZE * 2);

	int_bounds(&menu->box.x, 0, width - mx);
	int_bounds(&menu->box.y, 0, height - my);
}

static void
scroll_menu(struct menu *menu, int d)
{
	int w = int_max(1, menu->box.height - MENU_BORDER_SIZE * 2);
	int scr_i = int_min((w - 1) / 2, SCROLL_ITEMS);

	int_lower_bound(&scr_i, 0);
	int_lower_bound(&w, 0);

	if (menu->size < 1) {
		menu->selected = -1;
		menu->first = 0;
		return;
	}

	menu->selected += d;

	menu->selected %= menu->size;
	if (menu->selected < 0)
		menu->selected += menu->size;

	int_bounds(&menu->selected, 0, menu->size - 1);

	while (!mi_is_selectable(menu->items[menu->selected])) {
		menu->selected += d ? d/abs(d) : 1;

		if (menu->selected < 0 || menu->selected >= menu->size) {
			menu->selected = -1;
			menu->first = 0;
			return;
		}
	}

	/* The rest is not needed for horizontal menus like the mainmenu.
	 * FIXME: We need a better way to figure out which menus are horizontal and
	 * which are vertical (normal) --jonas */
	if (w <= 1) return;

	int_bounds(&menu->first, menu->selected - w + scr_i + 1, menu->selected - scr_i);
	int_bounds(&menu->first, 0, menu->size - w);
}

static inline void
draw_menu_left_text(struct terminal *term, unsigned char *text, int len,
		    int x, int y, int width, struct color_pair *color)
{
	int w = width - (L_TEXT_SPACE + R_TEXT_SPACE);

	if (w <= 0) return;

	if (len < 0) len = strlen(text);
	if (!len) return;
	if (len > w) len = w;

	draw_text(term, x + L_TEXT_SPACE, y, text, len, 0, color);
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
#ifdef CONFIG_DEBUG
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

	for (x = 0; x < w + !!hk && (c = text[x]); x++) {
		if (!hk && x == hotkey_pos - 1) {
			hk = 1;
			continue;
		}

		if (hk == 1) {
#ifdef CONFIG_DEBUG
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
	int w = width - (L_RTEXT_SPACE + R_RTEXT_SPACE);

	if (w <= 0) return;

	if (len < 0) len = strlen(text);
	if (!len) return;
	if (len > w) len = w;

	x += w - len + L_RTEXT_SPACE + L_TEXT_SPACE;

	draw_text(term, x, y, text, len, 0, color);
}

static void
display_menu(struct terminal *term, struct menu *menu)
{
	struct color_pair *normal_color = get_bfu_color(term, "menu.normal");
	struct color_pair *selected_color = get_bfu_color(term, "menu.selected");
	struct color_pair *frame_color = get_bfu_color(term, "menu.frame");
	struct box box, nbox;
	int p, y;

	set_box(&box,
		menu->box.x + MENU_BORDER_SIZE,
		menu->box.y + MENU_BORDER_SIZE,
		int_max(0, menu->box.width - MENU_BORDER_SIZE * 2),
		int_max(0, menu->box.height - MENU_BORDER_SIZE * 2));

	draw_box(term, &box, ' ', 0, normal_color);
	draw_border(term, &box, frame_color, 1);

	copy_box(&nbox, &box);

	for (p = menu->first, y = box.y;
	     p < menu->size && p < menu->first + box.height;
	     p++, y++) {
		struct color_pair *color = normal_color;

#ifdef CONFIG_DEBUG
		/* Sanity check. */
		if (mi_is_end_of_menu(menu->items[p]))
			INTERNAL("Unexpected end of menu [%p:%d]", menu->items[p], p);
#endif

		nbox.y = y;
		nbox.height = 1;

		if (p == menu->selected) {
			/* This entry is selected. */
			color = selected_color;

			set_cursor(term, box.x, y, 1);
			set_window_ptr(menu->win, menu->box.x + menu->box.width, y);
			draw_box(term, &nbox, ' ', 0, color);
		}

		if (mi_is_horizontal_bar(menu->items[p])) {
			/* Horizontal separator */
			draw_border_char(term, menu->box.x, y,
					 BORDER_SRTEE, frame_color);

			draw_box(term, &nbox, BORDER_SHLINE,
				 SCREEN_ATTR_FRAME, frame_color);

			draw_border_char(term, box.x + box.width, y,
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
							       box.x, y, box.width, color,
							       (p == menu->selected));

				} else {
					draw_menu_left_text(term, text, -1,
							    box.x, y, box.width, color);
		  		}
			}

			if (mi_is_submenu(menu->items[p])) {
				draw_menu_right_text(term, m_submenu, m_submenu_len,
						     menu->box.x, y, box.width, color);
			} else if (menu->items[p].action != ACT_MAIN_NONE) {
				struct string keystroke;

#ifdef CONFIG_DEBUG
				/* Help to detect action + right text. --Zas */
				if (mi_has_right_text(menu->items[p])) {
					if (color == selected_color)
						color = normal_color;
					else
						color = selected_color;
				}
#endif /* CONFIG_DEBUG */

				if (init_string(&keystroke)) {
					add_keystroke_to_string(&keystroke,
								menu->items[p].action,
								KM_MAIN);
					draw_menu_right_text(term, keystroke.source,
							     keystroke.length,
							     menu->box.x, y,
							     box.width, color);
					done_string(&keystroke);
				}

			} else if (mi_has_right_text(menu->items[p])) {
				unsigned char *rtext = menu->items[p].rtext;

				if (mi_rtext_translate(menu->items[p]))
					rtext = _(rtext, term);

				if (*rtext) {
					/* There's a right text, so print it */
					draw_menu_right_text(term, rtext, -1,
							     menu->box.x,
							     y, box.width, color);
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

	switch (get_mouse_button(ev)) {
		/* XXX: We return here directly because we
		 * would just break this switch instead of the
		 * large one. If you will add some generic
		 * action after the former switch, replace the
		 * return with goto here. --pasky */
		case B_WHEEL_UP:
			if (check_mouse_action(ev, B_DOWN)) {
				scroll_menu(menu, -1);
				display_menu(win->term, menu);
			}
			return;
		case B_WHEEL_DOWN:
			if (check_mouse_action(ev, B_DOWN)) {
				scroll_menu(menu, 1);
				display_menu(win->term, menu);
			}
			return;
	}

	if (!is_in_box(&menu->box, ev->x, ev->y)) {
		if (check_mouse_action(ev, B_DOWN)) {
			delete_window_ev(win, NULL);

		} else {
			struct window *w1;
			struct window *end = (struct window *)&win->term->windows;

			for (w1 = win; w1 != end; w1 = w1->next) {
				struct menu *m1;

				if (w1->handler == mainmenu_handler) {
					if (!ev->y)
						delete_window_ev(win, ev);
					break;
				}

				if (w1->handler != menu_handler) break;

				m1 = w1->data;

				if (is_in_box(&m1->box, ev->x, ev->y)) {
					delete_window_ev(win, ev);
					break;
				}
			}
		}

	} else {
		if (is_in_box(&menu->box, ev->x, ev->y)) {
			int sel = ev->y - menu->box.y - 1 + menu->first;

			if (sel >= 0 && sel < menu->size
			    && mi_is_selectable(menu->items[sel])) {
				menu->selected = sel;
				scroll_menu(menu, 0);
				display_menu(win->term, menu);

				if (check_mouse_action(ev, B_UP) ||
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
	int current = int_max(0, int_min(menu->selected, menu->size - 1));
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
	int current = int_max(0, int_min(menu->selected, menu->size - 1));
	int step;
	int i;
	int next_sep = menu->size - 1;

	for (i = current + 1; i < menu->size; i++)
		if (mi_is_horizontal_bar(menu->items[i])) {
			next_sep = i;
			break;
		}

	step = next_sep - current + 1;
	int_bounds(&step, 0, int_min(menu->size - 1 - current, DIST));

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
				return;
			}

			if (action == ACT_MENU_RIGHT)
				goto enter;

			delete_window(win);
			return;

		case ACT_MENU_UP:
			scroll_menu(menu, -1);
			break;

		case ACT_MENU_DOWN:
			scroll_menu(menu, 1);
			break;

		case ACT_MENU_HOME:
			scroll_menu(menu, -menu->selected);
			break;

		case ACT_MENU_END:
			scroll_menu(menu, menu->size - menu->selected - 1);
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

			return;

		default:
		{
			if ((ev->x >= KBD_F1 && ev->x <= KBD_F12) ||
			    ev->y == KBD_ALT) {
				delete_window_ev(win, ev);
				return;
			}

			if (ev->x > ' ' && ev->x < 255) {
				if (check_hotkeys(menu, ev->x, win->term))
					s = 1, scroll_menu(menu, 0);
				else if (check_not_so_hot_keys(menu, ev->x, win->term))
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
	struct menu *menu = mem_calloc(1, sizeof(struct menu));

	if (!menu) return;

	menu->selected = (sel == -1 ? 0 : sel);
	menu->items = items;
	menu->data = data;
	menu->size = count_items(items);

#ifdef ENABLE_NLS
	clear_hotkeys_cache(items, menu->size, 1);
#endif
	init_hotkeys(term, items, menu->size, 1);
	add_window(term, mainmenu_handler, menu);

	if (sel != -1) {
		struct term_event ev =
			INIT_TERM_EVENT(EV_KBD, KBD_ENTER, 0, 0);

		term_send_event(term, &ev);
	}
}

static void
display_mainmenu(struct terminal *term, struct menu *menu)
{
	struct color_pair *normal_color = get_bfu_color(term, "menu.normal");
	struct color_pair *selected_color = get_bfu_color(term, "menu.selected");
	int p = 0;
	int i;
	struct box box;

	/* FIXME: menu horizontal scrolling do not work well yet, we need to cache
	 * menu items width and recalculate them only when needed (ie. language change)
	 * instead of looping and calculate them each time. --Zas */

	/* Try to make current selected menu entry visible. */
	while (1) {
		if (menu->selected < menu->first) {
			menu->first--;
			menu->last--;

		} else if (menu->selected > menu->last) {
			menu->first++;
			menu->last++;
		} else
			break;
	}

	if (menu->last <= 0)
		menu->last = menu->size - 1;

	int_bounds(&menu->last, 0, menu->size - 1);
	int_bounds(&menu->first, 0, menu->last);

	set_box(&box, 0, 0, term->width, 1);
	draw_box(term, &box, ' ', 0, normal_color);

	if (menu->first != 0) {
		box.width = L_MAINMENU_SPACE;
		draw_box(term, &box, '<', 0, normal_color);
	}

	p += L_MAINMENU_SPACE;

	for (i = menu->first; i < menu->size; i++) {
		struct color_pair *color = normal_color;
		unsigned char *text = menu->items[i].text;
		int l = menu->items[i].hotkey_pos;
		int textlen;

		if (mi_text_translate(menu->items[i]))
			text = _(text, term);

		textlen = strlen(text) - !!l;

		if (i == menu->selected) {
			color = selected_color;
			box.x = p;
			box.width = L_MAINTEXT_SPACE + L_TEXT_SPACE
				    + textlen
				    + R_TEXT_SPACE + R_MAINTEXT_SPACE;
			draw_box(term, &box, ' ', 0, color);
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

	menu->last = i - 1;
	int_lower_bound(&menu->last, menu->first);
	if (menu->last < menu->size - 1) {
		set_box(&box,
			term->width - R_MAINMENU_SPACE, 0,
			R_MAINMENU_SPACE, 1);
		draw_box(term, &box, '>', 0, normal_color);
	}

	redraw_from_window(menu->win);
}


#ifdef CONFIG_MOUSE
static void
mainmenu_mouse_handler(struct menu *menu, struct term_event *ev)
{
	struct window *win = menu->win;
	struct menu_item *item;
	int p = L_MAINMENU_SPACE;
	int scroll = 0;

	if (check_mouse_wheel(ev))
		return;

	if (check_mouse_action(ev, B_DOWN) && ev->y) {
		delete_window_ev(win, NULL);
		return;
	}

	if (ev->y) return;

	/* First check if the mouse button was pressed in the side of the
	 * terminal and simply scroll one step in that direction else iterate
	 * through the menu items to see if it was pressed on a label. */
	if (ev->x < p) {
		scroll = -1;

	} else if (ev->x >= win->term->width - R_MAINMENU_SPACE) {
		scroll = 1;

	} else {
		/* We don't initialize to menu->first here, since it breaks
		 * horizontal scrolling using mouse in some cases. --Zas */
		foreach_menu_item (item, menu->items) {
			unsigned char *text = item->text;

			if (!mi_has_left_text(*item)) continue;

			if (mi_text_translate(*item))
				text = _(item->text, win->term);

			/* The label width is made up of a little padding on
			 * the sides followed by the text width substracting
			 * one char if it has hotkeys (the '~' char) */
			p += L_MAINTEXT_SPACE + L_TEXT_SPACE
			  + strlen(text) - !!item->hotkey_pos
			  + R_TEXT_SPACE + R_MAINTEXT_SPACE;

			if (ev->x < p) {
				scroll = (item - menu->items) - menu->selected;
				break;
			}
		}
	}

	if (scroll) {
		scroll_menu(menu, scroll);
		display_mainmenu(win->term, menu);
	}

	/* We need to select the menu item even if we didn't scroll
	 * apparently because we will delete any drop down menus
	 * in the clicking process. */
	if (check_mouse_action(ev, B_UP)
	    || mi_is_submenu(menu->items[menu->selected])) {
		select_menu(win->term, menu);
	}
}
#endif

static void
mainmenu_kbd_handler(struct menu *menu, struct term_event *ev, int fwd)
{
	struct window *win = menu->win;
	enum menu_action action = kbd_action(KM_MENU, ev, NULL);

	switch (action) {
	case ACT_MENU_ENTER:
	case ACT_MENU_DOWN:
	case ACT_MENU_UP:
	case ACT_MENU_PAGE_UP:
	case ACT_MENU_PAGE_DOWN:
	case ACT_MENU_SELECT:
		select_menu(win->term, menu);
		return;

	case ACT_MENU_HOME:
		scroll_menu(menu, -menu->selected);
		break;

	case ACT_MENU_END:
		scroll_menu(menu, menu->size - menu->selected - 1);
		break;

	case ACT_MENU_NEXT_ITEM:
	case ACT_MENU_PREVIOUS_ITEM:
		/* This is pretty western centric since `what is next'?
		 * Anyway we cycle clockwise by resetting the action ... */
		action = (action == ACT_MENU_NEXT_ITEM)
		       ? ACT_MENU_RIGHT : ACT_MENU_LEFT;
		/* ... and then letting left/right handling take over. */

	case ACT_MENU_LEFT:
	case ACT_MENU_RIGHT:
		scroll_menu(menu, action == ACT_MENU_LEFT ? -1 : 1);
		break;

	case ACT_MENU_REDRAW:
		/* Just call display_mainmenu() */
		break;

	default:
		/* Fallback to see if any hotkey matches the pressed key */
		if (ev->x > ' ' && ev->x < 256
		    && check_hotkeys(menu, ev->x, win->term)) {
			fwd = 1;
			break;
		}

	case ACT_MENU_CANCEL:
		delete_window_ev(win, action != ACT_MENU_CANCEL ? ev : NULL);
		return;
	}

	/* Redraw the menu */
	display_mainmenu(win->term, menu);
	if (fwd) select_menu(win->term, menu);
}

static void
mainmenu_handler(struct window *win, struct term_event *ev, int fwd)
{
	struct menu *menu = win->data;

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
	mem_align_alloc(mi_, size, (size) + 2, struct menu_item, 0xF)

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
