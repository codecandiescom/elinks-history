/* Internal MIME types implementation dialogs */
/* $Id: dialogs.c,v 1.40 2003/08/01 11:41:35 zas Exp $ */

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
#include "intl/gettext/libintl.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"


static struct option *
get_real_opt(unsigned char *base, unsigned char *id)
{
	struct option *opt = get_opt_rec_real(config_options, base);

	return (opt ? get_opt_rec_real(opt, id) : NULL);
}


static unsigned char *ext_msg[] = {
	N_("Extension(s)"),
	N_("Content-Type"),
};


static void
add_ext_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = -1;
	unsigned char dialog_text_color = get_bfu_color(term, "dialog.text");

	text_width(term, ext_msg[0], &min, &max);
	text_width(term, ext_msg[1], &min, &max);
	buttons_width(term, dlg->items + 2, 2, &min, &max);

	w = term->x * 9 / 10 - 2 * DIALOG_LB;
	int_upper_bound(&w, max);
	int_lower_bound(&w, min);
	int_upper_bound(&w, dlg->win->term->x - 2 * DIALOG_LB);
	int_lower_bound(&w, 1);

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
	struct option *opt = get_real_opt("mime.extension",
					  (unsigned char *) fcp);

	if (opt) delete_option(opt);
}

void
menu_del_ext(struct terminal *term, void *fcp, void *xxx2)
{
	struct string translated;
	struct option *opt = NULL;
	struct string str;

	if (!fcp) return;

	if (!init_string(&str)) {
		mem_free(fcp);
		return;
	}

	add_to_string(&str, (unsigned char *) fcp);
	mem_free(fcp);

	if (!init_string(&translated)) {
		done_string(&str);
		return;
	}

	if (add_optname_to_string(&translated, str.source, str.length))
		opt = get_real_opt("mime.extension", translated.source);

	if (!opt) {
		done_string(&translated);
		done_string(&str);
		return;
	}

	/* Finally add */
	add_to_string(&str, " -> ");
	add_to_string(&str, (unsigned char *) opt->ptr);

	msg_box(term, getml(str.source, translated.source, NULL), MSGBOX_FREE_TEXT,
		N_("Delete extension"), AL_CENTER,
		msg_text(term, N_("Delete extension %s?"), str.source),
		translated.source, 2,
		N_("Yes"), really_del_ext, B_ENTER,
		N_("No"), NULL, B_ESC);
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
	struct string name;

	if (!ext || !init_string(&name)) return;

	add_to_string(&name, "mime.extension.");
	add_optname_to_string(&name, ext->ext, strlen(ext->ext));

	really_del_ext(ext->ext_orig); /* ..or rename ;) */
	safe_strncpy(get_opt_str(name.source), ext->ct, MAX_STR_LEN);
	done_string(&name);
}

void
menu_add_ext(struct terminal *term, void *fcp, void *xxx2)
{
	struct option *opt;
	struct extension *new;
	unsigned char *ext;
	unsigned char *ct;
	unsigned char *ext_orig;
	struct dialog *d;
	struct string translated;

	if (fcp && init_string(&translated)
	    && add_optname_to_string(&translated, fcp, strlen(fcp))) {
		opt = get_real_opt("mime.extension", translated.source);
	} else {
		opt = NULL;
	}

	d = mem_calloc(1, sizeof(struct dialog) + 5 * sizeof(struct widget)
			  + sizeof(struct extension) + 3 * MAX_STR_LEN);
	if (!d) {
		if (fcp) {
			mem_free(fcp);
			done_string(&translated);
		}
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
		safe_strncpy(ext_orig, no_null(translated.source), MAX_STR_LEN);
		#undef no_null
	}

	if (fcp) done_string(&translated);

	d->title = _("Extension", term);
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
	d->items[2].text = _("OK", term);

	d->items[3].type = D_BUTTON;
	d->items[3].gid = B_ESC;
	d->items[3].text = _("Cancel", term);
	d->items[3].fn = cancel_dialog;

	d->items[4].type = D_END;

	do_dialog(term, d, getml(d, NULL));

	if (fcp) mem_free(fcp);
}


static struct menu_item mi_no_ext[] = {
	{N_("No extensions"), M_BAR, NULL, NULL, 0, 0},
	{NULL, NULL, NULL, NULL, 0, 0}
};

void
menu_list_ext(struct terminal *term, void *fn, void *xxx)
{
	struct option *opt_tree = get_opt_ptr("mime.extension");
	struct option *opt;
	struct menu_item *mi = NULL;

	foreachback (opt, *opt_tree) {
		struct string translated;
		unsigned char *translated2;
		unsigned char *optptr2;

		if (!strcmp(opt->name, "_template_")) continue;

		if (!init_string(&translated)
		    || !add_real_optname_to_string(&translated, opt->name,
						   strlen(opt->name))) {
			done_string(&translated);
			continue;
		}

		if (!mi) {
			mi = new_menu(FREE_LIST | FREE_TEXT | FREE_RTEXT | FREE_DATA);
			if (!mi) {
				done_string(&translated);
				return;
			}
		}

		translated2 = memacpy(translated.source, translated.length);
		optptr2 = stracpy(opt->ptr);

		if (translated2 && optptr2) {
			add_to_menu(&mi, translated.source, optptr2,
				    MENU_FUNC fn, translated2, 0, 0);
		} else {
			if (optptr2) mem_free(optptr2);
			if (translated2) mem_free(translated2);
			done_string(&translated);
		}
	}

	if (!mi) mi = mi_no_ext;
	do_menu(term, mi, xxx, 0);
}
