/* Cache-related dialogs */
/* $Id: dialogs.c,v 1.54 2004/04/03 13:51:58 jonas Exp $ */

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
#include "protocol/uri.h"
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
	struct cache_entry *cached = item->udata;
	struct string msg;

	if (listbox_info == LISTBOX_URI)
		return get_uri_string(cached->uri, ~0);

	if (!init_string(&msg)) return NULL;

	add_to_string(&msg, _("URL", term));
	add_to_string(&msg, ": ");
	add_uri_to_string(&msg, cached->uri, ~(URI_PASSWORD | URI_POST));

	if (cached->redirect) {
		add_format_to_string(&msg, "\n%s: %s", _("Redirect", term),
						cached->redirect);

		if (cached->redirect_get) {
			add_to_string(&msg, " (GET)");
		}
	}

	add_format_to_string(&msg, "\n%s: %d", _("Size", term), cached->length);
	add_format_to_string(&msg, "\n%s: %d", _("Loaded size", term),
						cached->data_size);
	if (cached->last_modified) {
		add_format_to_string(&msg, "\n%s: %s", _("Last modified", term),
				     cached->last_modified);
	}
	if (cached->etag) {
		add_format_to_string(&msg, "\n%s: %s", "ETag",
						cached->etag);
	}
	if (cached->ssl_info) {
		add_format_to_string(&msg, "\n%s: %s", _("SSL Cipher", term),
						cached->ssl_info);
	}
	if (cached->encoding_info) {
		add_format_to_string(&msg, "\n%s: %s", _("Encoding", term),
						cached->encoding_info);
	}

	if (cached->incomplete || !cached->valid) {
		add_char_to_string(&msg, '\n');
		add_to_string(&msg, _("Flags", term));
		add_to_string(&msg, ": ");
		if (cached->incomplete) {
			add_to_string(&msg, _("incomplete", term));
			add_char_to_string(&msg, ' ');
		}
		if (!cached->valid) add_to_string(&msg, _("invalid", term));
	}

#ifdef CONFIG_DEBUG
	add_format_to_string(&msg, "\n%s: %d", "Refcount", cached->refcount);
	add_format_to_string(&msg, "\n%s: %d", _("ID tag", term),
						cached->id_tag);

	if (cached->head && *cached->head) {
		add_format_to_string(&msg, "\n%s:\n\n%s", _("Header", term),
				     cached->head);
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
	struct cache_entry *cached = item->udata;

	assert(!is_object_used(cached));

	delete_cache_entry(cached);
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
