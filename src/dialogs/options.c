/* Options dialogs */
/* $Id: options.c,v 1.125 2003/11/16 14:34:32 zas Exp $ */

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
			    (menu_func) display_codepage, (void *)i, 0);
	}
	sel = get_opt_int_tree(term->spec, "charset");
	if (sel < 0) sel = 0;
	do_menu_selected(term, mi, ses, sel, 0);
}


/* TODO: Build this automagically. But that will need gettextted options
 * captions not to lose translations and so on. 0.5 stuff or even later.
 * --pasky */

enum termopt {
	TERM_OPT_TYPE = 0,
	TERM_OPT_M11_HACK,
	TERM_OPT_RESTRICT_852,
	TERM_OPT_BLOCK_CURSOR,
	TERM_OPT_COLORS,
	TERM_OPT_UTF_8_IO,
	TERM_OPT_TRANSPARENCY,
	TERM_OPT_UNDERLINE,

	TERM_OPTIONS,
};

struct termopt_info {
	enum termopt id;
	unsigned char *name;
};

static struct termopt_info termopt_info[] = {
	{ TERM_OPT_TYPE,	 "type"		},
	{ TERM_OPT_M11_HACK,	 "m11_hack"	},
	{ TERM_OPT_RESTRICT_852, "restrict_852"	},
	{ TERM_OPT_BLOCK_CURSOR, "block_cursor"	},
	{ TERM_OPT_COLORS,	 "colors"	},
	{ TERM_OPT_TRANSPARENCY, "transparency"	},
	{ TERM_OPT_UTF_8_IO,	 "utf_8_io"	},
	{ TERM_OPT_UNDERLINE,	 "underline"	},
};

static int
push_ok_button(struct dialog_data *dlg_data, struct widget_data *button)
{
	struct terminal *term = dlg_data->win->term;
	int *values = dlg_data->dlg->udata;
	int touched = 0;
	int i;

	update_dialog_data(dlg_data, button);

	for (i = 0; i < TERM_OPTIONS; i++) {
		unsigned char *name = termopt_info[i].name;
		struct option *o = get_opt_rec(term->spec, name);
		enum termopt id = termopt_info[i].id;

		if (o->value.number != values[id]) {
			o->value.number = values[id];
			o->flags |= OPT_TOUCHED;
			touched++;
		}
	}

	if (touched) {
		term->spec->change_hook(NULL, term->spec, NULL);
		cls_redraw_all_terminals();
	}

	if (button->widget->fn == push_ok_button)
		cancel_dialog(dlg_data, button);

	return 0;
}

static int
push_save_button(struct dialog_data *dlg_data, struct widget_data *button)
{
	push_ok_button(dlg_data, button);
	if (!get_opt_int_tree(cmdline_options, "anonymous"))
	        write_config(dlg_data->win->term);
        return 0;
}

#ifdef USE_256_COLORS
#define TERMOPT_WIDGETS_COUNT 18
#else
#define TERMOPT_WIDGETS_COUNT 17
#endif

void
terminal_options(struct terminal *term, void *xxx, struct session *ses)
{
	struct dialog *dlg;
	int i, *values;

	dlg = calloc_dialog(TERMOPT_WIDGETS_COUNT, sizeof(int) * TERM_OPTIONS);
	if (!dlg) return;

	i = sizeof_dialog(TERMOPT_WIDGETS_COUNT, 0);
	values = (int *) ((unsigned char *) dlg + i);
	for (i = 0; i < TERM_OPTIONS; i++) {
		unsigned char *name = termopt_info[i].name;
		enum termopt id = termopt_info[i].id;

		values[id] = get_opt_int_tree(term->spec, name);
	}

	dlg->title = _("Terminal options", term);
	dlg->layouter = generic_dialog_layouter;
	dlg->layout.padding_top = 1;
	dlg->udata = values;

	add_dlg_text(dlg, _("Frame handling:", term), AL_LEFT, 1);
	add_dlg_radio(dlg, _("No frames", term), 1, TERM_DUMB, values[TERM_OPT_TYPE]);
	add_dlg_radio(dlg, _("VT 100 frames", term), 1,  TERM_VT100, values[TERM_OPT_TYPE]);
	add_dlg_radio(dlg, _("Linux or OS/2 frames", term), 1, TERM_LINUX, values[TERM_OPT_TYPE]);
	add_dlg_radio(dlg, _("KOI8-R frames", term), 1, TERM_KOI8, values[TERM_OPT_TYPE]);

	add_dlg_text(dlg, _("Color mode:", term), AL_LEFT, 1);
	add_dlg_radio(dlg, _("No colors (mono)", term), 2, COLOR_MODE_MONO, values[TERM_OPT_COLORS]);
	add_dlg_radio(dlg, _("16 colors", term), 2, COLOR_MODE_16, values[TERM_OPT_COLORS]);
#ifdef USE_256_COLORS
	add_dlg_radio(dlg, _("256 colors", term), 2, COLOR_MODE_256, values[TERM_OPT_COLORS]);
#endif

	add_dlg_checkbox(dlg, _("Use ^[[11m", term), values[TERM_OPT_M11_HACK]);
	add_dlg_checkbox(dlg, _("Restrict frames in cp850/852", term), values[TERM_OPT_RESTRICT_852]);
	add_dlg_checkbox(dlg, _("Block the cursor", term), values[TERM_OPT_BLOCK_CURSOR]);
	add_dlg_checkbox(dlg, _("Transparency", term), values[TERM_OPT_TRANSPARENCY]);
	add_dlg_checkbox(dlg, _("Underline", term), values[TERM_OPT_UNDERLINE]);
	add_dlg_checkbox(dlg, _("UTF-8 I/O", term), values[TERM_OPT_UTF_8_IO]);

	add_dlg_button(dlg, B_ENTER, push_ok_button, _("OK", term), NULL);
	add_dlg_button(dlg, B_ENTER, push_save_button, _("Save", term), NULL);
	add_dlg_button(dlg, B_ESC, cancel_dialog, _("Cancel", term), NULL);

	add_dlg_end(dlg, TERMOPT_WIDGETS_COUNT);

	do_dialog(term, dlg, getml(dlg, NULL));
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
			    (menu_func) menu_set_language, (void *)i, NO_INTL);
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

	add_dlg_field(dlg, NULL, 1, 999, check_number, 4, x_str, NULL);
	add_dlg_field(dlg, NULL, 1, 999, check_number, 4, y_str, NULL);

	add_dlg_button(dlg, B_ENTER, ok_dialog, _("OK", term), NULL);
	add_dlg_button(dlg, B_ESC, cancel_dialog, _("Cancel", term), NULL);

	add_dlg_end(dlg, RESIZE_WIDGETS_COUNT);

	do_dialog(term, dlg, getml(dlg, NULL));
}
