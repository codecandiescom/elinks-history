/* Cache-related dialogs */
/* $Id: dialogs.c,v 1.53 2004/04/03 13:11:58 jonas Exp $ */

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
	struct cache_entry *cache = item->udata;
	struct string msg;

	if (listbox_info == LISTBOX_URI)
		return get_uri_string(cache->uri, ~0);

	if (!init_string(&msg)) return NULL;

	add_to_string(&msg, _("URL", term));
	add_to_string(&msg, ": ");
	add_uri_to_string(&msg, cache->uri, ~(URI_PASSWORD | URI_POST));

	if (cache->redirect) {
		add_format_to_string(&msg, "\n%s: %s", _("Redirect", term),
						cache->redirect);

		if (cache->redirect_get) {
			add_to_string(&msg, " (GET)");
		}
	}

	add_format_to_string(&msg, "\n%s: %d", _("Size", term), cache->length);
	add_format_to_string(&msg, "\n%s: %d", _("Loaded size", term),
						cache->data_size);
	if (cache->last_modified) {
		add_format_to_string(&msg, "\n%s: %s", _("Last modified", term),
				     cache->last_modified);
	}
	if (cache->etag) {
		add_format_to_string(&msg, "\n%s: %s", "ETag",
						cache->etag);
	}
	if (cache->ssl_info) {
		add_format_to_string(&msg, "\n%s: %s", _("SSL Cipher", term),
						cache->ssl_info);
	}
	if (cache->encoding_info) {
		add_format_to_string(&msg, "\n%s: %s", _("Encoding", term),
						cache->encoding_info);
	}

	if (cache->incomplete || !cache->valid) {
		add_char_to_string(&msg, '\n');
		add_to_string(&msg, _("Flags", term));
		add_to_string(&msg, ": ");
		if (cache->incomplete) {
			add_to_string(&msg, _("incomplete", term));
			add_char_to_string(&msg, ' ');
		}
		if (!cache->valid) add_to_string(&msg, _("invalid", term));
	}

#ifdef CONFIG_DEBUG
	add_format_to_string(&msg, "\n%s: %d", "Refcount", cache->refcount);
	add_format_to_string(&msg, "\n%s: %d", _("ID tag", term),
						cache->id_tag);

	if (cache->head && *cache->head) {
		add_format_to_string(&msg, "\n%s:\n\n%s", _("Header", term),
				     cache->head);
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
