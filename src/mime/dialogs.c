/* Internal MIME types implementation dialogs */
/* $Id: dialogs.c,v 1.11 2002/12/26 21:47:08 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/align.h"
#include "bfu/dialog.h"
#include "bfu/button.h"
#include "bfu/inpfield.h"
#include "bfu/menu.h"
#include "bfu/msgbox.h"
#include "bfu/text.h"
#include "config/options.h"
#include "dialogs/mime.h"
#include "intl/language.h"
#include "lowlevel/terminal.h"
#include "protocol/mime.h"
#include "util/memory.h"
#include "util/string.h"


static struct option *
get_real_opt(unsigned char *base, unsigned char *id)
{
	struct option *opt;
	unsigned char *name = straconcat(base, ".", id, NULL);

	if (!name) return NULL;

	opt = get_opt_rec_real(&root_options, name);

	mem_free(name);
	return opt;
}


static unsigned char *ext_msg[] = {
	TEXT(T_EXTENSION_S),
	TEXT(T_CONTENT_TYPE),
};


static void
add_ext_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = -1;
	int dialog_text_color = get_bfu_color(term, "dialog.text");

	max_text_width(term, ext_msg[0], &max);
	min_text_width(term, ext_msg[0], &min);
	max_text_width(term, ext_msg[1], &max);
	min_text_width(term, ext_msg[1], &min);
	max_buttons_width(term, dlg->items + 2, 2, &max);
	min_buttons_width(term, dlg->items + 2, 2, &min);

	w = term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max) w = max;
	if (w < min) w = min;
	if (w > term->x - 2 * DIALOG_LB) w = term->x - 2 * DIALOG_LB;
	if (w < 1) w = 1;

	rw = 0;
	dlg_format_text(NULL, term,
			ext_msg[0],
			0, &y, w, &rw,
			dialog_text_color, AL_LEFT);

	y += 2;
	dlg_format_text(NULL, term,
			ext_msg[1],
			0, &y, w, &rw,
			dialog_text_color, AL_LEFT);

	y += 2;
	dlg_format_buttons(NULL, term,
			   dlg->items + 2, 2,
			   0, &y, w, &rw,
			   AL_CENTER);

	w = rw;
	dlg->xw = w + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;

	center_dlg(dlg);
	draw_dlg(dlg);

	y = dlg->y + DIALOG_TB;
	dlg_format_text(term, term,
			ext_msg[0],
			dlg->x + DIALOG_LB, &y, w, NULL,
			dialog_text_color, AL_LEFT);
	dlg_format_field(term, term,
			 &dlg->items[0],
			 dlg->x + DIALOG_LB, &y, w, NULL,
			 AL_LEFT);

	y++;
	dlg_format_text(term, term,
			ext_msg[1],
			dlg->x + DIALOG_LB, &y, w, NULL,
			dialog_text_color, AL_LEFT);
	dlg_format_field(term, term,
			 &dlg->items[1],
			 dlg->x + DIALOG_LB, &y, w, NULL,
			 AL_LEFT);

	y++;
	dlg_format_buttons(term, term,
			   &dlg->items[2], 2,
			   dlg->x + DIALOG_LB, &y, w, NULL,
			   AL_CENTER);
}


static void
really_del_ext(void *fcp)
{
	struct option *opt;

	opt = get_real_opt("mime.extension", (unsigned char *) fcp);
	if (opt) delete_option(opt);
}


void
menu_del_ext(struct terminal *term, void *fcp, void *xxx2)
{
	unsigned char *translated = fcp ? stracpy((unsigned char *) fcp) : NULL;
	struct option *opt;
	unsigned char *str;
	int strl;
	int i;

	if (!translated) goto end;

	for (i = strlen(translated) - 1; i >= 0; i--)
		if (translated[i] == '.')
			translated[i] = '*';

	opt = get_real_opt("mime.extension", translated);
	if (!opt) goto end;

	str = init_str();
	if (!str) goto end;
	strl = 0;
	add_to_str(&str, &strl, (unsigned char *) fcp);
	add_to_str(&str, &strl, " -> ");
	add_to_str(&str, &strl, (unsigned char *) opt->ptr);

	msg_box(term, getml(str, translated, NULL),
		TEXT(T_DELETE_EXTENSION), AL_CENTER | AL_EXTD_TEXT,
		TEXT(T_DELETE_EXTENSION), " ", str, "?", NULL,
		translated, 2,
		TEXT(T_YES), really_del_ext, B_ENTER,
		TEXT(T_NO), NULL, B_ESC);

end:
	if (fcp) mem_free(fcp);
}


struct extension {
	unsigned char *ext_orig;
	unsigned char *ext;
	unsigned char *ct;
};

static void
really_add_ext(void *fcp)
{
	struct extension *ext = (struct extension *) fcp;
	unsigned char *translated;
	unsigned char *name;
	int i;

	if (!ext) return;

	translated = stracpy(ext->ext);
	if (!translated) return;

	for (i = strlen(translated) - 1; i >= 0; i--)
		if (translated[i] == '.')
			translated[i] = '*';

	name = straconcat("mime.extension.", translated, NULL);
	mem_free(translated);
	if (!name) return;

	really_del_ext(ext->ext_orig); /* ..or rename ;) */
	safe_strncpy(get_opt_str(name), ext->ct, MAX_STR_LEN);
	mem_free(name);
}

void
menu_add_ext(struct terminal *term, void *fcp, void *xxx2)
{
	unsigned char *translated = fcp ? stracpy((unsigned char *) fcp) : NULL;
	struct option *opt = NULL;
	struct extension *new;
	unsigned char *ext;
	unsigned char *ct;
	unsigned char *ext_orig;
	struct dialog *d;
	int i;

	if (translated) {

		for (i = strlen(translated) - 1; i >= 0; i--)
			if (translated[i] == '.')
				translated[i] = '*';

		opt = get_real_opt("mime.extension", translated);
	}

	d = mem_calloc(1, sizeof(struct dialog) + 5 * sizeof(struct widget)
			  + sizeof(struct extension) + 3 * MAX_STR_LEN);
	if (!d) {
		if (fcp) mem_free(fcp);
		return;
	}

	new = (struct extension *) &d->items[5];
	new->ext = ext = (unsigned char *) (new + 1);
	new->ct = ct = ext + MAX_STR_LEN;
	new->ext_orig = ext_orig = ct + MAX_STR_LEN;

	if (opt) {
		#define no_null(x) ((x) ? (unsigned char *) (x) \
					: (unsigned char *) "")
		safe_strncpy(ext, no_null(fcp), MAX_STR_LEN);
		safe_strncpy(ct, no_null(opt->ptr), MAX_STR_LEN);
		safe_strncpy(ext_orig, no_null(translated), MAX_STR_LEN);
		#undef no_null
	}

	if (translated) mem_free(translated);

	d->title = TEXT(T_EXTENSION);
	d->fn = add_ext_fn;
	d->refresh = (void (*)(void *)) really_add_ext;
	d->refresh_data = new;

	d->items[0].type = D_FIELD;
	d->items[0].dlen = MAX_STR_LEN;
	d->items[0].data = ext;
	d->items[0].fn = check_nonempty;

	d->items[1].type = D_FIELD;
	d->items[1].dlen = MAX_STR_LEN;
	d->items[1].data = ct;
	d->items[1].fn = check_nonempty;

	d->items[2].type = D_BUTTON;
	d->items[2].gid = B_ENTER;
	d->items[2].fn = ok_dialog;
	d->items[2].text = TEXT(T_OK);

	d->items[3].type = D_BUTTON;
	d->items[3].gid = B_ESC;
	d->items[3].text = TEXT(T_CANCEL);
	d->items[3].fn = cancel_dialog;

	d->items[4].type = D_END;

	do_dialog(term, d, getml(d, NULL));

	if (fcp) mem_free(fcp);
}


static struct menu_item mi_no_ext[] = {
	{TEXT(T_NO_EXTENSIONS), "", M_BAR, NULL, NULL, 0, 0},
	{NULL, NULL, 0, NULL, NULL, 0, 0}
};

void
menu_list_ext(struct terminal *term, void *fn, void *xxx)
{
	struct list_head *opt_tree;
	struct option *opt;
	struct menu_item *mi = NULL;

	opt_tree = (struct list_head *) get_opt_ptr("mime.extension");

	foreachback (opt, *opt_tree) {
		unsigned char *translated;
		unsigned char *translated2;
		unsigned char *optptr2;
		int i;

		if (!strcmp(opt->name, "_template_")) continue;

		translated = stracpy(opt->name);
		if (!translated) continue;

		for (i = strlen(translated) - 1; i >= 0; i--)
			if (translated[i] == '*')
				translated[i] = '.';


		if (!mi) {
			mi = new_menu(FREE_LIST | FREE_TEXT | FREE_RTEXT | FREE_DATA);
		       	if (!mi) { mem_free(translated); return; }
		}

		translated2 = stracpy(translated);
		optptr2 = stracpy(opt->ptr);

		if (translated2 && optptr2) {
			add_to_menu(&mi, translated,
				    optptr2,
				    "", MENU_FUNC fn, translated2, 0);
		} else {
			if (optptr2) mem_free(optptr2);
			if (translated2) mem_free(translated2);
			if (translated) mem_free(translated);
		}

	}

	if (!mi)
		do_menu(term, mi_no_ext, xxx);
	else
		do_menu(term, mi, xxx);
}
