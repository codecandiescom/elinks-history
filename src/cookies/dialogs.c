/* Cookie-related dialogs */
/* $Id: dialogs.c,v 1.5 2003/11/18 13:29:00 jonas Exp $ */

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


static INIT_LIST_HEAD(cookie_dialog_list);

void
update_all_cookie_dialogs(void)
{
	struct hierbox_dialog_list_item *item;

	foreach (item, cookie_dialog_list) {
		struct widget_data *widget_data =
			item->dlg_data->widgets_data;

		display_dlg_item(item->dlg_data, widget_data, 1);
	}
}


/* Cleans up after the cache dialog */
static void
cookie_dialog_abort_handler(struct dialog_data *dlg_data)
{
	struct hierbox_dialog_list_item *item;

	foreach (item, cookie_dialog_list) {
		if (item->dlg_data == dlg_data) {
			del_from_list(item);
			mem_free(item);
			break;
		}
	}
}


static unsigned char *
get_cookie_info(struct cookie *cookie, struct terminal *term)
{
	unsigned char *expires = NULL;
	struct string string;

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

	add_format_to_string(&string,
		_("Server: %s\n"
		"Name: %s\n"
		"Value: %s\n"
		"Domain: %s\n"
		"Expires: %s\n"
		"Secure: %s", term), 
		cookie->server, cookie->name, cookie->value,
		cookie->domain,
		expires ? expires : _("unknown",  term),
		_(cookie->secure ? N_("yes") : N_("no"), term));

	if (expires) mem_free(expires);
	return string.source;
}

static void
done_info_button(void *vhop)
{
	struct cookie *cookie = vhop;

	object_unlock(cookie);
}

static int
push_info_button(struct dialog_data *dlg_data, struct widget_data *button)
{
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct terminal *term = dlg_data->win->term;
	struct cookie *cookie;
	unsigned char *msg;

	/* Show history item info */
	if (!box->sel) return 0;
	cookie = box->sel->udata;
	if (!cookie) return 0;

	msg = get_cookie_info(cookie, term);

	object_lock(cookie);

	msg_box(term, NULL, MSGBOX_FREE_TEXT,
		N_("Info"), AL_LEFT,
		msg,
		cookie, 1,
		N_("OK"), done_info_button, B_ESC | B_ENTER);

	return 0;
}

static int
push_save_button(struct dialog_data *dlg_data, struct widget_data *button)
{
	save_cookies();
	return 0;
}

void
menu_cookie_manager(struct terminal *term, void *fcp, struct session *ses)
{
	struct dialog_data *dlg_data;
	struct hierbox_dialog_list_item *item;
	struct listbox_item *litem;

	foreach (litem, cookie_box_items) {
		litem->visible = 1;
	}

	dlg_data = hierbox_browser(term, N_("Cookie manager"),
			0,
			hierbox_browser_box_build(&cookie_boxes,
						  &cookie_box_items, NULL),
			ses,
			2,
			N_("Info"), push_info_button, B_ENTER, ses,
			N_("Save"), push_save_button, B_ENTER, ses);

	if (!dlg_data) return;
	dlg_data->dlg->abort = cookie_dialog_abort_handler;

	item = mem_alloc(sizeof(struct hierbox_dialog_list_item));
	if (item) {
		item->dlg_data = dlg_data;
		add_to_list(cookie_dialog_list, item);
	}
}
