/* Cookie-related dialogs */
/* $Id: dialogs.c,v 1.31 2004/01/03 13:12:16 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef CONFIG_COOKIES

#include <string.h>

#include "elinks.h"

#include "bfu/button.h"
#include "bfu/dialog.h"
#include "bfu/hierbox.h"
#include "bfu/listbox.h"
#include "bfu/msgbox.h"
#include "bfu/text.h"
#include "cookies/cookies.h"
#include "cookies/dialogs.h"
#include "dialogs/edit.h"
#include "intl/gettext/libintl.h"
#include "sched/session.h"
#include "terminal/kbd.h"
#include "terminal/terminal.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/object.h"
#include "util/string.h"


static void
lock_cookie(struct listbox_item *item)
{
	object_lock((struct cookie *)item->udata);
}

static void
unlock_cookie(struct listbox_item *item)
{
	object_unlock((struct cookie *)item->udata);
}

static int
is_cookie_used(struct listbox_item *item)
{
	if (item->type == BI_FOLDER) {
		struct listbox_item *root = item;

		foreach (item, root->child)
			if (is_object_used((struct cookie *)item->udata))
				return 1;

		return 0;
	}

	return is_object_used((struct cookie *)item->udata);
}

static unsigned char *
get_cookie_info(struct listbox_item *item, struct terminal *term,
		enum listbox_info listbox_info)
{
	struct cookie *cookie = item->udata;
	unsigned char *expires = NULL;
	struct string string;

	if (listbox_info == LISTBOX_URI)
		return NULL;

	if (!init_string(&string)) return NULL;

#ifdef HAVE_STRFTIME
	if (cookie->expires) {
		struct tm *when_local = localtime(&cookie->expires);
		unsigned char str[13];
		int wr = strftime(str, sizeof(str), "%b %e %H:%M", when_local);

		if (wr > 0)
			expires = memacpy(str, wr);
	}
#endif

	add_format_to_string(&string, "%s: %s", _("Server", term), cookie->server);
	add_format_to_string(&string, "\n%s: %s", _("Name", term), cookie->name);
	add_format_to_string(&string, "\n%s: %s", _("Value", term), cookie->value);
	add_format_to_string(&string, "\n%s: %s", _("Domain", term), cookie->domain);
	if (expires) {
		add_format_to_string(&string, "\n%s: %s", _("Expires", term), expires);
	}
	add_format_to_string(&string, "\n%s: %s", _("Secure", term),
				_(cookie->secure ? N_("yes") : N_("no"), term));

	if (expires) mem_free(expires);
	return string.source;
}

static int
can_delete_cookie(struct listbox_item *item)
{
	return 1;
}

static void
delete_cookie(struct listbox_item *item, int last)
{
	struct cookie *cookie = item->udata;

	if (!cookie) return;

	assert(!is_object_used(cookie));

	/* Update the root text if we own it */
	if (item->root
	    && item->root->text == cookie->server) {
		struct listbox_item *item2;

		foreach (item2, item->root->child)
			if (item2 != item) {
				struct cookie *cookie2 = item2->udata;

				item->root->text = cookie2->server;
			}
	}

	del_from_list(cookie);
	free_cookie(cookie);

	if (last
	    && get_opt_bool("cookies.save")
	    && get_opt_bool("cookies.resave"))
		save_cookies();
}

static struct listbox_ops cookies_listbox_ops = {
	lock_cookie,
	unlock_cookie,
	is_cookie_used,
	get_cookie_info,
	can_delete_cookie,
	delete_cookie,
	NULL,
};

static int
push_save_button(struct dialog_data *dlg_data, struct widget_data *button)
{
	save_cookies();
	return 0;
}

static struct hierbox_browser_button cookie_buttons[] = {
	{ N_("Info"),		push_hierbox_info_button,	1 },
	{ N_("Delete"),		push_hierbox_delete_button,	1 },
	{ N_("Clear"),		push_hierbox_clear_button,	1 },
	{ N_("Save"),		push_save_button,		0 },
};

struct_hierbox_browser(
	cookie_browser,
	N_("Cookie manager"),
	cookie_buttons,
	&cookies_listbox_ops
);

void
menu_cookie_manager(struct terminal *term, void *fcp, struct session *ses)
{
	hierbox_browser(&cookie_browser, ses);
}

#endif /* CONFIG_COOKIES */
