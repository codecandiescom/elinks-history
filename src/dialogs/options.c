/* Options dialogs */
/* $Id: options.c,v 1.42 2002/12/11 15:21:08 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/align.h"
#include "bfu/dialog.h"
#include "bfu/button.h"
#include "bfu/checkbox.h"
#include "bfu/group.h"
#include "bfu/inpfield.h"
#include "bfu/menu.h"
#include "config/options.h"
#include "dialogs/options.h"
#include "document/session.h"
#include "intl/charsets.h"
#include "intl/language.h"
#include "lowlevel/terminal.h"
#include "util/memory.h"
#include "util/memlist.h"


static void
display_codepage(struct terminal *term, void *pcp, struct session *ses)
{
	get_opt_int_tree(term->spec, "charset") = (int) pcp;
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
		add_to_menu(&mi, get_cp_name(i), "", "",
			    MENU_FUNC display_codepage, (void *)i, 0);
	}
	sel = get_opt_int_tree(term->spec, "charset");
	if (sel < 0) sel = 0;
	do_menu_selected(term, mi, ses, sel);
}


/* TODO: Build this automagically. But that will need "captions" for options
 * not to lose translations and so on. 0.5 stuff or even later. --pasky */

static void
terminal_options_ok(void *p)
{
	cls_redraw_all_terminals();
}

static unsigned char *td_labels[] = {
	TEXT(T_NO_FRAMES),
	TEXT(T_VT_100_FRAMES),
	TEXT(T_LINUX_OR_OS2_FRAMES),
	TEXT(T_KOI8R_FRAMES),
	TEXT(T_USE_11M),
	TEXT(T_RESTRICT_FRAMES_IN_CP850_852),
	TEXT(T_BLOCK_CURSOR),
	TEXT(T_COLOR),
	TEXT(T_UTF_8_IO),
	NULL
};

void
terminal_options(struct terminal *term, void *xxx, struct session *ses)
{
	struct dialog *d;
	void *opt_term_type = (void *) get_opt_ptr_tree(term->spec, "type");

	d = mem_calloc(1, sizeof(struct dialog) + 12 * sizeof(struct widget));
	if (!d) return;

	d->title = TEXT(T_TERMINAL_OPTIONS);
	d->fn = checkbox_list_fn;
	d->udata = td_labels;
	d->refresh = (void (*)(void *)) terminal_options_ok;

	d->items[0].type = D_CHECKBOX;
	d->items[0].gid = 1;
	d->items[0].gnum = TERM_DUMB;
	d->items[0].dlen = sizeof(int);
	d->items[0].data = opt_term_type;

	d->items[1].type = D_CHECKBOX;
	d->items[1].gid = 1;
	d->items[1].gnum = TERM_VT100;
	d->items[1].dlen = sizeof(int);
	d->items[1].data = opt_term_type;

	d->items[2].type = D_CHECKBOX;
	d->items[2].gid = 1;
	d->items[2].gnum = TERM_LINUX;
	d->items[2].dlen = sizeof(int);
	d->items[2].data = opt_term_type;

	d->items[3].type = D_CHECKBOX;
	d->items[3].gid = 1;
	d->items[3].gnum = TERM_KOI8;
	d->items[3].dlen = sizeof(int);
	d->items[3].data = opt_term_type;

	d->items[4].type = D_CHECKBOX;
	d->items[4].gid = 0;
	d->items[4].dlen = sizeof(int);
	d->items[4].data = (void *) get_opt_ptr_tree(term->spec, "m11_hack");

	d->items[5].type = D_CHECKBOX;
	d->items[5].gid = 0;
	d->items[5].dlen = sizeof(int);
	d->items[5].data = (void *) get_opt_ptr_tree(term->spec, "restrict_852");

	d->items[6].type = D_CHECKBOX;
	d->items[6].gid = 0;
	d->items[6].dlen = sizeof(int);
	d->items[6].data = (void *) get_opt_ptr_tree(term->spec, "block_cursor");

	d->items[7].type = D_CHECKBOX;
	d->items[7].gid = 0;
	d->items[7].dlen = sizeof(int);
	d->items[7].data = (void *) get_opt_ptr_tree(term->spec, "colors");

	d->items[8].type = D_CHECKBOX;
	d->items[8].gid = 0;
	d->items[8].dlen = sizeof(int);
	d->items[8].data = (void *) get_opt_ptr_tree(term->spec, "utf_8_io");

	d->items[9].type = D_BUTTON;
	d->items[9].gid = B_ENTER;
	d->items[9].fn = ok_dialog;
	d->items[9].text = TEXT(T_OK);

	d->items[10].type = D_BUTTON;
	d->items[10].gid = B_ESC;
	d->items[10].fn = cancel_dialog;
	d->items[10].text = TEXT(T_CANCEL);

	d->items[11].type = D_END;

	do_dialog(term, d, getml(d, NULL));
}


static void
menu_set_language(struct terminal *term, void *pcp, struct session *ses)
{
	set_language((int)pcp);
	cls_redraw_all_terminals();
}

void
menu_language_list(struct terminal *term, void *xxx, struct session *ses)
{
	int i, sel;
	unsigned char *n;
	struct menu_item *mi = new_menu(FREE_LIST);

	if (!mi) return;
	for (i = 0; i < n_languages(); i++) {
		n = language_name(i);
		add_to_menu(&mi, n, "", "",
			    MENU_FUNC menu_set_language, (void *)i, 0);
	}
	sel = current_language;
	do_menu_selected(term, mi, ses, sel);
}


/* FIXME: This doesn't in fact belong here at all. --pasky */

static unsigned char *resize_texts[] = {
	TEXT(T_COLUMNS),
	TEXT(T_ROWS)
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

	d->title = TEXT(T_RESIZE_TERMINAL);
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
	d->items[2].text = TEXT(T_OK);

	d->items[3].type = D_BUTTON;
	d->items[3].gid = B_ESC;
	d->items[3].fn = cancel_dialog;
	d->items[3].text = TEXT(T_CANCEL);

	d->items[4].type = D_END;

	do_dialog(term, d, getml(d, NULL));
}
