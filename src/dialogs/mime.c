/* Internal MIME types implementation dialogs */
/* $Id: mime.c,v 1.78 2003/11/24 01:22:20 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "bfu/button.h"
#include "bfu/inpfield.h"
#include "bfu/menu.h"
#include "bfu/msgbox.h"
#include "bfu/style.h"
#include "bfu/text.h"
#include "config/options.h"
#include "dialogs/mime.h"
#include "intl/gettext/libintl.h"
#include "terminal/terminal.h"
#include "util/color.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"


static struct option *
get_real_opt(unsigned char *base, unsigned char *id)
{
	struct option *opt = get_opt_rec_real(config_options, base);

	return (opt ? get_opt_rec_real(opt, id) : NULL);
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
	add_to_string(&str, opt->value.string);

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
	struct dialog *dlg;
	struct string translated;

	if (fcp && init_string(&translated)
	    && add_optname_to_string(&translated, fcp, strlen(fcp))) {
		opt = get_real_opt("mime.extension", translated.source);
	} else {
		opt = NULL;
	}

#define MIME_WIDGETS_COUNT 4
	dlg = calloc_dialog(MIME_WIDGETS_COUNT, sizeof(struct extension) + 3 * MAX_STR_LEN);
	if (!dlg) {
		if (fcp) {
			mem_free(fcp);
			done_string(&translated);
		}
		return;
	}

	new = (struct extension *) get_dialog_offset(dlg, MIME_WIDGETS_COUNT);
	new->ext = ext = (unsigned char *) (new + 1);
	new->ct = ct = ext + MAX_STR_LEN;
	new->ext_orig = ext_orig = ct + MAX_STR_LEN;

	if (opt) {
		safe_strncpy(ext, empty_string_or_(fcp), MAX_STR_LEN);
		safe_strncpy(ct, empty_string_or_(opt->value.string), MAX_STR_LEN);
		safe_strncpy(ext_orig, empty_string_or_(translated.source), MAX_STR_LEN);
	}

	if (fcp) done_string(&translated);

	dlg->title = _("Extension", term);
	dlg->layouter = generic_dialog_layouter;
	dlg->refresh = (void (*)(void *)) really_add_ext;
	dlg->refresh_data = new;

	add_dlg_field(dlg, _("Extension(s)", term), 0, 0, check_nonempty, MAX_STR_LEN, ext, NULL);
	add_dlg_field(dlg, _("Content-Type", term), 0, 0, check_nonempty, MAX_STR_LEN, ct, NULL);

	add_dlg_button(dlg, B_ENTER, ok_dialog, _("OK", term), NULL);
	add_dlg_button(dlg, B_ESC, cancel_dialog, _("Cancel", term), NULL);

	add_dlg_end(dlg, MIME_WIDGETS_COUNT);

	do_dialog(term, dlg, getml(dlg, NULL));

	if (fcp) mem_free(fcp);
}


static struct menu_item mi_no_ext[] = {
	INIT_MENU_ITEM(N_("No extensions"), M_BAR, NULL, NULL, FREE_LIST),
	NULL_MENU_ITEM
};

void
menu_list_ext(struct terminal *term, void *fn, void *xxx)
{
	struct list_head *opt_tree = get_opt_tree("mime.extension");
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
		optptr2 = stracpy(opt->value.string);

		if (translated2 && optptr2) {
			add_to_menu(&mi, translated.source, optptr2,
				    (menu_func) fn, translated2, 0);
		} else {
			if (optptr2) mem_free(optptr2);
			if (translated2) mem_free(translated2);
			done_string(&translated);
		}
	}

	if (!mi) mi = mi_no_ext;
	do_menu(term, mi, xxx, 0);
}
