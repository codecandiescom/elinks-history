/* Options dialogs */
/* $Id: options.c,v 1.146 2004/05/06 14:39:40 jonas Exp $ */

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
#include "bfu/msgbox.h"
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
display_codepage(struct terminal *term, unsigned char *name, struct session *ses)
{
	struct option *opt = get_opt_rec(term->spec, "charset");
	int index = get_cp_index(name);

	assertm(index != -1, "%s", name);

	if (opt->value.number != index) {
		opt->value.number = index;
		opt->flags |= OPT_TOUCHED;
	}

	cls_redraw_all_terminals();
}

void
charset_list(struct terminal *term, void *xxx, struct session *ses)
{
	int i, sel = int_max(0, get_opt_int_tree(term->spec, "charset"));
	unsigned char *n;
	struct menu_item *mi = new_menu(FREE_LIST);

	if (!mi) return;

	for (i = 0; (n = get_cp_name(i)); i++) {
		if (is_cp_special(i)) continue;
		add_to_menu(&mi, get_cp_name(i), NULL, ACT_MAIN_NONE,
			    (menu_func) display_codepage, get_cp_mime_name(i), 0);
	}

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

static struct option_resolver resolvers[] = {
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
	union option_value *values = dlg_data->dlg->udata;

	update_dialog_data(dlg_data, button);

	if (commit_option_values(resolvers, term->spec, values, TERM_OPTIONS)) {
		/* TODO: The change hook thing should be handled by the
		 * commit function. */
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
	write_config(dlg_data->win->term);

	return 0;
}

#ifdef CONFIG_256_COLORS
#define TERMOPT_WIDGETS_COUNT 19
#else
#define TERMOPT_WIDGETS_COUNT 18
#endif

void
terminal_options(struct terminal *term, void *xxx, struct session *ses)
{
	struct dialog *dlg;
	union option_value *values;
	int anonymous = get_opt_int_tree(cmdline_options, "anonymous");

	dlg = calloc_dialog(TERMOPT_WIDGETS_COUNT, sizeof(union option_value) * TERM_OPTIONS);
	if (!dlg) return;

	values = (union option_value *) get_dialog_offset(dlg, TERMOPT_WIDGETS_COUNT);
	checkout_option_values(resolvers, term->spec, values, TERM_OPTIONS);

	dlg->title = _("Terminal options", term);
	dlg->layouter = generic_dialog_layouter;
	dlg->layout.padding_top = 1;
	dlg->udata = values;

	add_dlg_text(dlg, _("Frame handling:", term), AL_LEFT, 1);
	add_dlg_radio(dlg, _("No frames", term), 1, TERM_DUMB, values[TERM_OPT_TYPE]);
	add_dlg_radio(dlg, _("VT 100 frames", term), 1,  TERM_VT100, values[TERM_OPT_TYPE]);
	add_dlg_radio(dlg, _("Linux or OS/2 frames", term), 1, TERM_LINUX, values[TERM_OPT_TYPE]);
	add_dlg_radio(dlg, _("FreeBSD frames", term), 1, TERM_FREEBSD, values[TERM_OPT_TYPE]);
	add_dlg_radio(dlg, _("KOI8-R frames", term), 1, TERM_KOI8, values[TERM_OPT_TYPE]);

	add_dlg_text(dlg, _("Color mode:", term), AL_LEFT, 1);
	add_dlg_radio(dlg, _("No colors (mono)", term), 2, COLOR_MODE_MONO, values[TERM_OPT_COLORS]);
	add_dlg_radio(dlg, _("16 colors", term), 2, COLOR_MODE_16, values[TERM_OPT_COLORS]);
#ifdef CONFIG_256_COLORS
	add_dlg_radio(dlg, _("256 colors", term), 2, COLOR_MODE_256, values[TERM_OPT_COLORS]);
#endif

	add_dlg_checkbox(dlg, _("Use ^[[11m", term), values[TERM_OPT_M11_HACK]);
	add_dlg_checkbox(dlg, _("Restrict frames in cp850/852", term), values[TERM_OPT_RESTRICT_852]);
	add_dlg_checkbox(dlg, _("Block the cursor", term), values[TERM_OPT_BLOCK_CURSOR]);
	add_dlg_checkbox(dlg, _("Transparency", term), values[TERM_OPT_TRANSPARENCY]);
	add_dlg_checkbox(dlg, _("Underline", term), values[TERM_OPT_UNDERLINE]);
	add_dlg_checkbox(dlg, _("UTF-8 I/O", term), values[TERM_OPT_UTF_8_IO]);

	add_dlg_button(dlg, B_ENTER, push_ok_button, _("OK", term), NULL);
	if (!anonymous)
		add_dlg_button(dlg, B_ENTER, push_save_button, _("Save", term), NULL);
	add_dlg_button(dlg, B_ESC, cancel_dialog, _("Cancel", term), NULL);

	add_dlg_end(dlg, TERMOPT_WIDGETS_COUNT - anonymous);

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
#ifdef ENABLE_NLS
	int i;
	struct menu_item *mi = new_menu(FREE_LIST);

	if (!mi) return;
	for (i = 0; languages[i].name; i++) {
		add_to_menu(&mi, languages[i].name, language_to_iso639(i), ACT_MAIN_NONE,
			    (menu_func) menu_set_language, (void *)i, 0);
	}

	do_menu_selected(term, mi, ses, current_language, 0);
#endif
}


/* FIXME: This doesn't in fact belong here at all. --pasky */

static unsigned char x_str[4];
static unsigned char y_str[4];

static void
push_resize_button(struct terminal *term)
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

	add_dlg_field(dlg, NULL, 1, 999, check_number, 4, x_str, NULL);
	add_dlg_field(dlg, NULL, 1, 999, check_number, 4, y_str, NULL);

	add_dlg_ok_button(dlg, B_ENTER, _("OK", term), push_resize_button, term);
	add_dlg_button(dlg, B_ESC, cancel_dialog, _("Cancel", term), NULL);

	add_dlg_end(dlg, RESIZE_WIDGETS_COUNT);

	do_dialog(term, dlg, getml(dlg, NULL));
}
