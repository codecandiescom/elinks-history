/* Internal MIME types implementation dialogs */
/* $Id: dialogs.c,v 1.96 2004/04/13 22:47:32 jonas Exp $ */

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
#include "mime/dialogs.h"
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
	unsigned char *extension = fcp;

	if (!extension) return;

	if (!init_string(&translated)) {
		mem_free(extension);
		return;
	}

	if (add_optname_to_string(&translated, extension, strlen(extension)))
		opt = get_real_opt("mime.extension", translated.source);

	if (!opt) {
		done_string(&translated);
		mem_free(extension);
		return;
	}

	msg_box(term, getml(translated.source, NULL), MSGBOX_FREE_TEXT,
		N_("Delete extension"), AL_CENTER,
		msg_text(term, N_("Delete extension %s -> %s?"),
			 extension, opt->value.string),
		translated.source, 2,
		N_("Yes"), really_del_ext, B_ENTER,
		N_("No"), NULL, B_ESC);

	mem_free(extension);
}


struct extension {
	unsigned char ext_orig[MAX_STR_LEN];
	unsigned char ext[MAX_STR_LEN];
	unsigned char ct[MAX_STR_LEN];
};

static void
add_mime_extension(struct extension *ext)
{
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
	struct extension *new;
	struct dialog *dlg;

#define MIME_WIDGETS_COUNT 4
	dlg = calloc_dialog(MIME_WIDGETS_COUNT, sizeof(struct extension));
	if (!dlg) {
		if (fcp) mem_free(fcp);
		return;
	}

	new = (struct extension *) get_dialog_offset(dlg, MIME_WIDGETS_COUNT);

	if (fcp) {
		struct option *opt = NULL;
		struct string translated;

		if (init_string(&translated)
		    && add_optname_to_string(&translated, fcp, strlen(fcp)))
			opt = get_real_opt("mime.extension", translated.source);

		if (opt) {
			safe_strncpy(new->ext, fcp, MAX_STR_LEN);
			safe_strncpy(new->ct, opt->value.string, MAX_STR_LEN);
			safe_strncpy(new->ext_orig, translated.source, MAX_STR_LEN);
		}

		done_string(&translated);
		mem_free(fcp);
	}

	dlg->title = _("Extension", term);
	dlg->layouter = generic_dialog_layouter;

	add_dlg_field(dlg, _("Extension(s)", term), 0, 0, check_nonempty, MAX_STR_LEN, new->ext, NULL);
	add_dlg_field(dlg, _("Content-Type", term), 0, 0, check_nonempty, MAX_STR_LEN, new->ct, NULL);

	add_dlg_ok_button(dlg, B_ENTER, _("OK", term), add_mime_extension, new);
	add_dlg_button(dlg, B_ESC, cancel_dialog, _("Cancel", term), NULL);

	add_dlg_end(dlg, MIME_WIDGETS_COUNT);

	do_dialog(term, dlg, getml(dlg, NULL));
}


static struct menu_item mi_no_ext[] = {
	INIT_MENU_ITEM(N_("No extensions"), NULL, ACT_MAIN_NONE, NULL, NULL, FREE_LIST | NO_SELECT),
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
			mi = new_menu(FREE_ANY);
			if (!mi) {
				done_string(&translated);
				return;
			}
		}

		translated2 = memacpy(translated.source, translated.length);
		optptr2 = stracpy(opt->value.string);

		if (translated2 && optptr2) {
			add_to_menu(&mi, translated.source, optptr2, ACT_MAIN_NONE,
				    (menu_func) fn, translated2, 0);
		} else {
			if (optptr2) mem_free(optptr2);
			if (translated2) mem_free(translated2);
			done_string(&translated);
		}
	}

	if (!mi) mi = mi_no_ext;
	do_menu(term, mi, NULL, 0);
}
