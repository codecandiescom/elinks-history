/* Cache-related dialogs */
/* $Id: dialogs.c,v 1.28 2003/11/21 01:13:26 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/button.h"
#include "bfu/dialog.h"
#include "bfu/hierbox.h"
#include "bfu/listbox.h"
#include "bfu/msgbox.h"
#include "bfu/text.h"
#include "cache/cache.h"
#include "cache/dialogs.h"
#include "dialogs/edit.h"
#include "intl/gettext/libintl.h"
#include "sched/session.h"
#include "terminal/kbd.h"
#include "terminal/terminal.h"
#include "util/memory.h"
#include "util/object.h"
#include "util/string.h"


struct hierbox_browser cache_browser = {
	{ D_LIST_HEAD(cache_browser.boxes) },
	&cache_entry_box_items,
	{ D_LIST_HEAD(cache_browser.dialogs) },
	NULL,
};

static void
done_info_button(void *vhop)
{
	struct cache_entry *ce = vhop;

	object_unlock(ce);
}

static int
push_info_button(struct dialog_data *dlg_data,
		  struct widget_data *some_useless_info_button)
{
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct terminal *term = dlg_data->win->term;
	struct cache_entry *ce;
	struct string msg;

	/* Show history item info */
	if (!box->sel) return 0;
	ce = box->sel->udata;
	if (!ce) return 0;
	if (!init_string(&msg)) return 0;

	object_lock(ce);

	add_to_string(&msg, _("URL", term));
	add_to_string(&msg, ": ");
	add_uri_to_string(&msg, &ce->uri, ~(URI_PASSWORD | URI_POST));

	if (ce->redirect) {
		add_format_to_string(&msg, "\n%s: %s", _("Redirect", term),
						ce->redirect);

		if (ce->redirect_get) {
			add_to_string(&msg, " (GET)");
		}
	}

	add_format_to_string(&msg, "\n%s: %d", _("Size", term), ce->length);
	add_format_to_string(&msg, "\n%s: %d", _("Loaded size", term),
						ce->data_size);
	if (ce->last_modified) {
		add_format_to_string(&msg, "\n%s: %s", _("Last modified", term),
				     ce->last_modified);
	}
	if (ce->etag) {
		add_format_to_string(&msg, "\n%s: %s", "ETag",
						ce->etag);
	}
	if (ce->ssl_info) {
		add_format_to_string(&msg, "\n%s: %s", _("SSL Cipher", term),
						ce->ssl_info);
	}
	if (ce->encoding_info) {
		add_format_to_string(&msg, "\n%s: %s", _("Encoding", term),
						ce->encoding_info);
	}

	if (ce->incomplete || !ce->valid) {
		add_char_to_string(&msg, '\n');
		add_to_string(&msg, _("Flags", term));
		add_to_string(&msg, ": ");
		if (ce->incomplete) {
			add_to_string(&msg, _("incomplete", term));
			add_char_to_string(&msg, ' ');
		}
		if (!ce->valid) add_to_string(&msg, _("invalid", term));
	}

#ifdef DEBUG
	/* Show refcount - 1 because we have the entry locked now. */
	add_format_to_string(&msg, "\n%s: %d", "Refcount",
						ce->refcount - 1);
	add_format_to_string(&msg, "\n%s: %d", _("ID tag", term),
						ce->id_tag);

	if (ce->head && *ce->head) {
		add_format_to_string(&msg, "\n%s:\n\n%s", _("Header", term),
				     ce->head);
	}
#endif

	msg_box(term, NULL, MSGBOX_FREE_TEXT,
		N_("Info"), AL_LEFT,
		msg.source,
		ce, 1,
		N_("OK"), done_info_button, B_ESC | B_ENTER);

	return 0;
}


void
menu_cache_manager(struct terminal *term, void *fcp, struct session *ses)
{
	hierbox_browser(term, N_("Cache"),
			0, &cache_browser, ses,
			1,
			N_("Info"), push_info_button, B_ENTER, ses);
}
