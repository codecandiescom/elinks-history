/* Options dialogs */
/* $Id: options.c,v 1.114 2003/11/06 22:02:51 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/button.h"
#include "bfu/checkbox.h"
#include "bfu/dialog.h"
#include "bfu/group.h"
#include "bfu/inpfield.h"
#include "bfu/menu.h"
#include "bfu/text.h"
#include "config/conf.h"
#include "config/options.h"
#include "dialogs/options.h"
#include "intl/charsets.h"
#include "intl/gettext/libintl.h"
#include "sched/session.h"
#include "terminal/color.h"
#include "terminal/terminal.h"
#include "util/memory.h"
#include "util/memlist.h"


static void
display_codepage(struct terminal *term, void *pcp, struct session *ses)
{
	struct option *opt = get_opt_rec(term->spec, "charset");

	if (opt->value.number != (int) pcp) {
		opt->value.number = (int) pcp;
		opt->flags |= OPT_TOUCHED;
	}

	cls_redraw_all_terminals();
}

void
charset_list(struct terminal *term, void *xxx, struct session *ses)
{
	int i, sel;
	unsigned char *n;
	struct menu_item *mi = new_menu(FREE_LIST);

	if (!mi) return;
	for (i = 0; (n = get_cp_name(i)); i++) {
		if (is_cp_special(i)) continue;
		add_to_menu(&mi, get_cp_name(i), "",
			    (menu_func) display_codepage, (void *)i, 0, 0);
	}
	sel = get_opt_int_tree(term->spec, "charset");
	if (sel < 0) sel = 0;
	do_menu_selected(term, mi, ses, sel, 0);
}


/* TODO: Build this automagically. But that will need gettextted options
 * captions not to lose translations and so on. 0.5 stuff or even later.
 * --pasky */

struct termopt_hop {
	struct terminal *term;
	int type;
	int m11_hack;
	int restrict_852;
	int block_cursor;
	int colors;
	int utf_8_io;
	int trans;
	int underline;
};

static void
terminal_options_ok(void *p)
{
	struct termopt_hop *termopt_hop = p;
	struct terminal *term = termopt_hop->term;
	int touched = 0;

#define maybe_update(val, name) 					\
{ 									\
	struct option *o = get_opt_rec(term->spec, name);	 	\
	if (o->value.number != val) {					\
		o->value.number = val;	 				\
		o->flags |= OPT_TOUCHED; 				\
		touched++;						\
	} 								\
}
	maybe_update(termopt_hop->type, "type");
	maybe_update(termopt_hop->m11_hack, "m11_hack");
	maybe_update(termopt_hop->restrict_852, "restrict_852");
	maybe_update(termopt_hop->block_cursor, "block_cursor");
	maybe_update(termopt_hop->colors, "colors");
	maybe_update(termopt_hop->trans, "transparency");
	maybe_update(termopt_hop->utf_8_io, "utf_8_io");
	maybe_update(termopt_hop->underline, "underline");
#undef maybe_update

	if (touched)
		term->spec->change_hook(NULL, term->spec, NULL);

	cls_redraw_all_terminals();
}

static int
terminal_options_save(struct dialog_data *dlg_data,
		      struct widget_data *some_useless_info_button)
{
	update_dialog_data(dlg_data, some_useless_info_button);
	terminal_options_ok(dlg_data->dlg->udata);
	if (!get_opt_int_tree(cmdline_options, "anonymous"))
	        write_config(dlg_data->win->term);
        return 0;
}

/* Stolen checkbox_list_fn(). Code duplication forever. */
static void
terminal_options_layouter(struct dialog_data *dlg_data)
{
	struct terminal *term = dlg_data->win->term;
	int max = 0, min = 0;
	int w = dialog_max_width(term);
	int rw = 0;
	int y = 0;

	checkboxes_width(term, dlg_data->widgets_data, dlg_data->n - 3, &min, &max);
	buttons_width(dlg_data->widgets_data + dlg_data->n - 3, 3, &min, &max);

	int_bounds(&w, min, max);
	int_bounds(&w, 5, term->width - 2 * DIALOG_LB);

	dlg_format_checkboxes(NULL, dlg_data->widgets_data, dlg_data->n - 3, 0, &y, w,
			      &rw);

	y++;
	dlg_format_buttons(NULL, dlg_data->widgets_data + dlg_data->n - 3, 3, 0, &y, w,
			   &rw, AL_CENTER);

	w = rw;

	draw_dialog(dlg_data, w, y, AL_CENTER);

	y = dlg_data->y + DIALOG_TB + 1;
	dlg_format_checkboxes(term, dlg_data->widgets_data, dlg_data->n - 3,
			      dlg_data->x + DIALOG_LB, &y, w, NULL);


	y++;
	dlg_format_buttons(term, dlg_data->widgets_data + dlg_data->n - 3, 3,
			   dlg_data->x + DIALOG_LB, &y, w, &rw,
			   AL_CENTER);
}

#ifdef USE_256_COLORS
#define TERMOPT_WIDGETS_COUNT 16
#else
#define TERMOPT_WIDGETS_COUNT 15
#endif

void
terminal_options(struct terminal *term, void *xxx, struct session *ses)
{
	struct termopt_hop *termopt_hop;
	struct dialog *dlg;

	termopt_hop = mem_calloc(1, sizeof(struct termopt_hop));
	if (!termopt_hop) return;

	dlg = calloc_dialog(TERMOPT_WIDGETS_COUNT, 0);
	if (!dlg) {
		mem_free(termopt_hop);
		return;
	}

	termopt_hop->term = term;
	termopt_hop->type = get_opt_int_tree(term->spec, "type");
	termopt_hop->m11_hack = get_opt_int_tree(term->spec, "m11_hack");
	termopt_hop->restrict_852 = get_opt_int_tree(term->spec, "restrict_852");
	termopt_hop->block_cursor = get_opt_int_tree(term->spec, "block_cursor");
	termopt_hop->colors = get_opt_int_tree(term->spec, "colors");
	termopt_hop->trans = get_opt_int_tree(term->spec, "transparency");
	termopt_hop->underline = get_opt_int_tree(term->spec, "underline");
	termopt_hop->utf_8_io = get_opt_int_tree(term->spec, "utf_8_io");

	dlg->title = _("Terminal options", term);
	dlg->layouter = terminal_options_layouter;
	dlg->udata = termopt_hop;
	dlg->refresh = (void (*)(void *)) terminal_options_ok;
	dlg->refresh_data = termopt_hop;

	add_dlg_radio(dlg, _("No frames", term), 1, TERM_DUMB, termopt_hop->type);
	add_dlg_radio(dlg, _("VT 100 frames", term), 1,  TERM_VT100, termopt_hop->type);
	add_dlg_radio(dlg, _("Linux or OS/2 frames", term), 1, TERM_LINUX, termopt_hop->type);
	add_dlg_radio(dlg, _("KOI8-R frames", term), 1, TERM_KOI8, termopt_hop->type);

	add_dlg_radio(dlg, _("No colors (mono)", term), 2, COLOR_MODE_MONO, termopt_hop->colors);
	add_dlg_radio(dlg, _("16 colors", term), 2, COLOR_MODE_16, termopt_hop->colors);
#ifdef USE_256_COLORS
	add_dlg_radio(dlg, _("256 colors", term), 2, COLOR_MODE_256, termopt_hop->colors);
#endif

	add_dlg_checkbox(dlg, _("Use ^[[11m", term), termopt_hop->m11_hack);
	add_dlg_checkbox(dlg, _("Restrict frames in cp850/852", term), termopt_hop->restrict_852);
	add_dlg_checkbox(dlg, _("Block the cursor", term), termopt_hop->block_cursor);
	add_dlg_checkbox(dlg, _("Transparency", term), termopt_hop->trans);
	add_dlg_checkbox(dlg, _("Underline", term), termopt_hop->underline);
	add_dlg_checkbox(dlg, _("UTF-8 I/O", term), termopt_hop->utf_8_io);

	add_dlg_button(dlg, B_ENTER, ok_dialog, _("OK", term), NULL);
	add_dlg_button(dlg, B_ENTER, terminal_options_save, _("Save", term), NULL);
	add_dlg_button(dlg, B_ESC, cancel_dialog, _("Cancel", term), NULL);

	add_dlg_end(dlg, TERMOPT_WIDGETS_COUNT);

	do_dialog(term, dlg, getml(dlg, termopt_hop, NULL));
}

#ifdef ENABLE_NLS
static void
menu_set_language(struct terminal *term, void *pcp, struct session *ses)
{
	set_language((int)pcp);
	cls_redraw_all_terminals();
}
#endif

void
menu_language_list(struct terminal *term, void *xxx, struct session *ses)
{
/* FIXME */
#ifdef ENABLE_NLS
	int i, sel;
	struct menu_item *mi = new_menu(FREE_LIST);

	if (!mi) return;
	for (i = 0; languages[i].name; i++) {
		add_to_menu(&mi, languages[i].name, "",
			    (menu_func) menu_set_language, (void *)i, 0, 1);
	}
	sel = current_language;
	do_menu_selected(term, mi, ses, sel, 0);
#endif
}


/* FIXME: This doesn't in fact belong here at all. --pasky */

static unsigned char x_str[4];
static unsigned char y_str[4];

static void
do_resize_terminal(struct terminal *term)
{
	unsigned char str[8];

	strcpy(str, x_str);
	strcat(str, ",");
	strcat(str, y_str);
	do_terminal_function(term, TERM_FN_RESIZE, str);
}

void
dlg_resize_terminal(struct terminal *term, void *xxx, struct session *ses)
{
	struct dialog *dlg;
	int x = int_min(term->width, 999);
	int y = int_min(term->height, 999);

	sprintf(x_str, "%d", x);
	sprintf(y_str, "%d", y);

#define RESIZE_WIDGETS_COUNT 4
	dlg = calloc_dialog(RESIZE_WIDGETS_COUNT, 0);
	if (!dlg) return;

	dlg->title = _("Resize terminal", term);
	dlg->layouter = group_layouter;
	dlg->refresh = (void (*)(void *)) do_resize_terminal;
	dlg->refresh_data = term;

	add_dlg_field(dlg, 1, 999, check_number, 4, x_str, NULL);
	add_dlg_field(dlg, 1, 999, check_number, 4, y_str, NULL);

	add_dlg_button(dlg, B_ENTER, ok_dialog, _("OK", term), NULL);
	add_dlg_button(dlg, B_ESC, cancel_dialog, _("Cancel", term), NULL);

	add_dlg_end(dlg, RESIZE_WIDGETS_COUNT);

	do_dialog(term, dlg, getml(dlg, NULL));
}
