/* Options dialogs */
/* $Id: options.c,v 1.57 2003/05/03 01:07:46 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/align.h"
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
#include "lowlevel/terminal.h"
#include "sched/session.h"
#include "util/memory.h"
#include "util/memlist.h"


static void
display_codepage(struct terminal *term, void *pcp, struct session *ses)
{
	struct option *opt = get_opt_rec(term->spec, "charset");

	if (*((int *) opt->ptr) != (int) pcp) {
		*((int *) opt->ptr) = (int) pcp;
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
			    MENU_FUNC display_codepage, (void *)i, 0);
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
	int type, m11_hack, restrict_852, block_cursor, colors, utf_8_io, trans;
};

static void
terminal_options_ok(void *p)
{
	struct termopt_hop *termopt_hop = p;

#define maybe_update(val, name) \
{ \
	struct option *o = get_opt_rec(termopt_hop->term->spec, name); \
	if (*((int *) o->ptr) != val) { \
		*((int *) o->ptr) = val; \
		o->flags |= OPT_TOUCHED; \
	} \
}
	maybe_update(termopt_hop->type, "type");
	maybe_update(termopt_hop->m11_hack, "m11_hack");
	maybe_update(termopt_hop->restrict_852, "restrict_852");
	maybe_update(termopt_hop->block_cursor, "block_cursor");
	maybe_update(termopt_hop->colors, "colors");
	maybe_update(termopt_hop->trans, "transparency");
	maybe_update(termopt_hop->utf_8_io, "utf_8_io");
#undef maybe_update

	cls_redraw_all_terminals();
}

static int
terminal_options_save(struct dialog_data *dlg,
		struct widget_data *some_useless_info_button)
{
	update_dialog_data(dlg, some_useless_info_button);
	terminal_options_ok(dlg->dlg->udata);
	if (!get_opt_int_tree(&cmdline_options, "anonymous"))
	        write_config(dlg->win->term);
        return 0;
}

static unsigned char *td_labels[] = {
	N_("No frames"),
	N_("VT 100 frames"),
	N_("Linux or OS/2 frames"),
	N_("KOI8-R frames"),
	N_("Use ^[[11m"),
	N_("Restrict frames in cp850/852"),
	N_("Block the cursor"),
	N_("Color"),
	N_("Transparency"),
	N_("UTF-8 I/O"),
	NULL
};

/* Stolen checkbox_list_fn(). Code duplication forever. */
static void
terminal_options_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = 0;

	checkboxes_width(term, td_labels, &max, max_text_width);
	checkboxes_width(term, td_labels, &min, min_text_width);
	max_buttons_width(term, dlg->items + dlg->n - 3, 3, &max);
	min_buttons_width(term, dlg->items + dlg->n - 3, 3, &min);

	w = term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max) w = max;
	if (w < min) w = min;
	if (w > term->x - 2 * DIALOG_LB) w = term->x - 2 * DIALOG_LB;
	if (w < 5) w = 5;

	rw = 0;
	dlg_format_checkboxes(NULL, term, dlg->items, dlg->n - 3, 0, &y, w,
			&rw, td_labels);

	y++;
	dlg_format_buttons(NULL, term, dlg->items + dlg->n - 3, 3, 0, &y, w,
			&rw, AL_CENTER);

	w = rw;
	dlg->xw = rw + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);

	draw_dlg(dlg);

	y = dlg->y + DIALOG_TB + 1;
	dlg_format_checkboxes(term, term, dlg->items, dlg->n - 3,
			dlg->x + DIALOG_LB, &y, w, NULL,
			td_labels);

	y++;
	dlg_format_buttons(term, term, dlg->items + dlg->n - 3, 3,
			dlg->x + DIALOG_LB, &y, w, &rw,
			AL_CENTER);
}

void
terminal_options(struct terminal *term, void *xxx, struct session *ses)
{
	struct termopt_hop *termopt_hop;
	struct dialog *d;

	termopt_hop = mem_calloc(1, sizeof(struct termopt_hop));
	if (!termopt_hop) return;

	d = mem_calloc(1, sizeof(struct dialog) + 14 * sizeof(struct widget));
	if (!d) {
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
	termopt_hop->utf_8_io = get_opt_int_tree(term->spec, "utf_8_io");

	d->title = N_("Terminal options");
	d->fn = terminal_options_fn;
	d->udata = termopt_hop;
	d->refresh = (void (*)(void *)) terminal_options_ok;
	d->refresh_data = termopt_hop;

	d->items[0].type = D_CHECKBOX;
	d->items[0].gid = 1;
	d->items[0].gnum = TERM_DUMB;
	d->items[0].dlen = sizeof(int);
	d->items[0].data = (unsigned char *) &termopt_hop->type;

	d->items[1].type = D_CHECKBOX;
	d->items[1].gid = 1;
	d->items[1].gnum = TERM_VT100;
	d->items[1].dlen = sizeof(int);
	d->items[1].data = (unsigned char *) &termopt_hop->type;

	d->items[2].type = D_CHECKBOX;
	d->items[2].gid = 1;
	d->items[2].gnum = TERM_LINUX;
	d->items[2].dlen = sizeof(int);
	d->items[2].data = (unsigned char *) &termopt_hop->type;

	d->items[3].type = D_CHECKBOX;
	d->items[3].gid = 1;
	d->items[3].gnum = TERM_KOI8;
	d->items[3].dlen = sizeof(int);
	d->items[3].data = (unsigned char *) &termopt_hop->type;

	d->items[4].type = D_CHECKBOX;
	d->items[4].gid = 0;
	d->items[4].dlen = sizeof(int);
	d->items[4].data = (unsigned char *) &termopt_hop->m11_hack;

	d->items[5].type = D_CHECKBOX;
	d->items[5].gid = 0;
	d->items[5].dlen = sizeof(int);
	d->items[5].data = (unsigned char *) &termopt_hop->restrict_852;

	d->items[6].type = D_CHECKBOX;
	d->items[6].gid = 0;
	d->items[6].dlen = sizeof(int);
	d->items[6].data = (unsigned char *) &termopt_hop->block_cursor;

	d->items[7].type = D_CHECKBOX;
	d->items[7].gid = 0;
	d->items[7].dlen = sizeof(int);
	d->items[7].data = (unsigned char *) &termopt_hop->colors;

	d->items[8].type = D_CHECKBOX;
	d->items[8].gid = 0;
	d->items[8].dlen = sizeof(int);
	d->items[8].data = (unsigned char *) &termopt_hop->trans;

	d->items[9].type = D_CHECKBOX;
	d->items[9].gid = 0;
	d->items[9].dlen = sizeof(int);
	d->items[9].data = (unsigned char *) &termopt_hop->utf_8_io;

	d->items[10].type = D_BUTTON;
	d->items[10].gid = B_ENTER;
	d->items[10].fn = ok_dialog;
	d->items[10].text = N_("OK");

	d->items[11].type = D_BUTTON;
	d->items[11].gid = B_ENTER;
	d->items[11].fn = terminal_options_save;
	d->items[11].text = N_("Save");

	d->items[12].type = D_BUTTON;
	d->items[12].gid = B_ESC;
	d->items[12].fn = cancel_dialog;
	d->items[12].text = N_("Cancel");

	d->items[13].type = D_END;

	do_dialog(term, d, getml(d, termopt_hop, NULL));
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
			    MENU_FUNC menu_set_language, (void *)i, 0);
	}
	sel = current_language;
	do_menu_selected(term, mi, ses, sel, 0);
#endif
}


/* FIXME: This doesn't in fact belong here at all. --pasky */

static unsigned char *resize_texts[] = {
	N_("Columns"),
	N_("Rows")
};

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
	struct dialog *d;
	int x = term->x > 999 ? 999 : term->x;
	int y = term->y > 999 ? 999 : term->y;

	sprintf(x_str, "%d", x);
	sprintf(y_str, "%d", y);

	d = mem_calloc(1, sizeof(struct dialog) + 5 * sizeof(struct widget));
	if (!d) return;

	d->title = N_("Resize ~terminal");
	d->fn = group_fn;
	d->udata = resize_texts;
	d->refresh = (void (*)(void *))do_resize_terminal;
	d->refresh_data = term;

	d->items[0].type = D_FIELD;
	d->items[0].dlen = 4;
	d->items[0].data = x_str;
	d->items[0].fn = check_number;
	d->items[0].gid = 1;
	d->items[0].gnum = 999;

	d->items[1].type = D_FIELD;
	d->items[1].dlen = 4;
	d->items[1].data = y_str;
	d->items[1].fn = check_number;
	d->items[1].gid = 1;
	d->items[1].gnum = 999;

	d->items[2].type = D_BUTTON;
	d->items[2].gid = B_ENTER;
	d->items[2].fn = ok_dialog;
	d->items[2].text = N_("OK");

	d->items[3].type = D_BUTTON;
	d->items[3].gid = B_ESC;
	d->items[3].fn = cancel_dialog;
	d->items[3].text = N_("Cancel");

	d->items[4].type = D_END;

	do_dialog(term, d, getml(d, NULL));
}
