/* Cache-related dialogs */
/* $Id: dialogs.c,v 1.60 2004/05/03 23:04:08 zas Exp $ */

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
#include "terminal/draw.h"
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
	add_uri_to_string(&msg, cached->uri, URI_PUBLIC);

	if (cached->proxy_uri != cached->uri) {
		add_format_to_string(&msg, "\n%s: %s", _("Proxy URL", term),
						struri(cached->proxy_uri));
	}

	if (cached->redirect) {
		add_format_to_string(&msg, "\n%s: %s", _("Redirect", term),
						struri(cached->redirect));

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
	add_format_to_string(&msg, "\n%s: %d", "Refcount", get_object_refcount(cached));
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

static void
draw_cache_entry_item(struct listbox_item *item, struct listbox_context *context,
		      int x, int y, int width)
{
	struct cache_entry *cached = item->udata;
	unsigned char *stylename;
	struct color_pair *color;
	struct string msg;
	int trimmedlen;

	/* We have nothing to work with */
	if (width < 4) return;

	if (!init_string(&msg)) return;

	add_uri_to_string(&msg, cached->uri, URI_PUBLIC);
	if (cached->uri->post)
		add_to_string(&msg, " (POST DATA)");

	stylename = (item == context->box->sel) ? "menu.selected"
		  : ((item->marked)	        ? "menu.marked"
					        : "menu.normal");

	color = get_bfu_color(context->term, stylename);
	trimmedlen = int_min(msg.length, width - 4);

	draw_text(context->term, x, y, msg.source, trimmedlen, 0, color);
	if (trimmedlen < msg.length)
		draw_text(context->term, x + trimmedlen, y, " ...", 4, 0, color);

	done_string(&msg);
}

static struct listbox_ops cache_entry_listbox_ops = {
	lock_cache_entry,
	unlock_cache_entry,
	is_cache_entry_used,
	get_cache_entry_info,
	can_delete_cache_entry,
	delete_cache_entry_item,
	draw_cache_entry_item,
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
