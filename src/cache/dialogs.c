/* Cache-related dialogs */
/* $Id: dialogs.c,v 1.49 2004/03/22 14:35:37 jonas Exp $ */

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


static void
lock_cache_entry(struct listbox_item *item)
{
	object_lock((struct cache_entry *)item->udata);
}

static void
unlock_cache_entry(struct listbox_item *item)
{
	object_unlock((struct cache_entry *)item->udata);
}

static int
is_cache_entry_used(struct listbox_item *item)
{
	return is_object_used((struct cache_entry *)item->udata);
}

static unsigned char *
get_cache_entry_info(struct listbox_item *item, struct terminal *term,
		     enum listbox_info listbox_info)
{
	struct cache_entry *ce = item->udata;
	struct string msg;

	if (listbox_info == LISTBOX_URI)
		return stracpy(struri(ce->uri));

	if (!init_string(&msg)) return NULL;

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

#ifdef CONFIG_DEBUG
	add_format_to_string(&msg, "\n%s: %d", "Refcount", ce->refcount);
	add_format_to_string(&msg, "\n%s: %d", _("ID tag", term),
						ce->id_tag);

	if (ce->head && *ce->head) {
		add_format_to_string(&msg, "\n%s:\n\n%s", _("Header", term),
				     ce->head);
	}
#endif

	return msg.source;
}

static int
can_delete_cache_entry(struct listbox_item *item)
{
	return 1;
}

static void
delete_cache_entry_item(struct listbox_item *item, int last)
{
	struct cache_entry *cache_entry = item->udata;

	assert(!is_object_used(cache_entry));

	delete_cache_entry(cache_entry);
}

static struct listbox_ops cache_entry_listbox_ops = {
	lock_cache_entry,
	unlock_cache_entry,
	is_cache_entry_used,
	get_cache_entry_info,
	can_delete_cache_entry,
	delete_cache_entry_item,
	NULL,
};

static struct hierbox_browser_button cache_buttons[] = {
	{ N_("Info"),		push_hierbox_info_button,	1 },
	{ N_("Goto"),		push_hierbox_goto_button,	1 },
	{ N_("Delete"),		push_hierbox_delete_button,	1 },
};

struct_hierbox_browser(
	cache_browser,
	N_("Cache manager"),
	cache_buttons,
	&cache_entry_listbox_ops
);

void
cache_manager(struct session *ses)
{
	hierbox_browser(&cache_browser, ses);
}
