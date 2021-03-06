/* Cookie-related dialogs */
/* $Id: dialogs.c,v 1.88 2005/09/06 13:38:45 witekfl Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "cookies/cookies.h"
#include "cookies/dialogs.h"
#include "dialogs/edit.h"
#include "intl/gettext/libintl.h"
#include "main/object.h"
#include "session/session.h"
#include "terminal/draw.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/string.h"


INIT_LIST_HEAD(cookie_queries);

static void
add_cookie_info_to_string(struct string *string, struct cookie *cookie,
			  struct terminal *term)
{
	add_format_to_string(string, "\n%s: %s", _("Name", term), cookie->name);
	add_format_to_string(string, "\n%s: %s", _("Value", term), cookie->value);
	add_format_to_string(string, "\n%s: %s", _("Domain", term), cookie->domain);
	add_format_to_string(string, "\n%s: %s", _("Path", term), cookie->path);

	if (!cookie->expires) {
		add_format_to_string(string, "\n%s: ", _("Expires", term));
		add_to_string(string, _("at quit time", term));
#ifdef HAVE_STRFTIME
	} else {
		add_format_to_string(string, "\n%s: ", _("Expires", term));
		add_date_to_string(string, get_opt_str("ui.date_format"), &cookie->expires);
#endif
	}

	add_format_to_string(string, "\n%s: %s", _("Secure", term),
			     _(cookie->secure ? N_("yes") : N_("no"), term));
}

/* TODO: Store cookie in data arg. --jonas*/
void
accept_cookie_dialog(struct session *ses, void *data)
{
	struct cookie *cookie = cookie_queries.next;
	struct string string;

	assert(ses);

	if (list_empty(cookie_queries)
	    || !init_string(&string))
		return;

	del_from_list(cookie);

	add_format_to_string(&string,
		_("Do you want to accept a cookie from %s?", ses->tab->term),
		cookie->server->host);

	add_to_string(&string, "\n\n");

	add_cookie_info_to_string(&string, cookie, ses->tab->term);

	msg_box(ses->tab->term, NULL, MSGBOX_FREE_TEXT,
		N_("Accept cookie?"), ALIGN_LEFT,
		string.source,
		cookie, 2,
		N_("~Accept"), accept_cookie, B_ENTER,
		N_("~Reject"), done_cookie, B_ESC);
}


static void
lock_cookie(struct listbox_item *item)
{
	if (item->type == BI_LEAF)
		object_lock((struct cookie *) item->udata);
	else
		object_lock((struct cookie_server *) item->udata);
}

static void
unlock_cookie(struct listbox_item *item)
{
	if (item->type == BI_LEAF)
		object_unlock((struct cookie *) item->udata);
	else
		object_unlock((struct cookie_server *) item->udata);
}

static int
is_cookie_used(struct listbox_item *item)
{
	if (item->type == BI_FOLDER) {
		struct listbox_item *root = item;

		foreach (item, root->child)
			if (is_object_used((struct cookie *) item->udata))
				return 1;

		return 0;
	}

	return is_object_used((struct cookie *) item->udata);
}

static unsigned char *
get_cookie_text(struct listbox_item *item, struct terminal *term)
{
	/* Are we dealing with a folder? */
	if (item->type == BI_FOLDER) {
		struct cookie_server *server = item->udata;

		return stracpy(server->host);

	} else {
		struct cookie *cookie = item->udata;

		return stracpy(cookie->name);
	}
}

static unsigned char *
get_cookie_info(struct listbox_item *item, struct terminal *term)
{
	struct cookie *cookie = item->udata;
	struct cookie_server *server;
	struct string string;

	if (item->type == BI_FOLDER) return NULL;

	if (!init_string(&string)) return NULL;

	server = cookie->server;

	add_format_to_string(&string, "%s: %s", _("Server", term), server->host);

	add_cookie_info_to_string(&string, cookie, term);

	return string.source;
}

static struct listbox_item *
get_cookie_root(struct listbox_item *item)
{
	/* Are we dealing with a folder? */
	if (item->type == BI_FOLDER) {
		return NULL;
	} else {
		struct cookie *cookie = item->udata;

		return cookie->server->box_item;
	}
}

static int
can_delete_cookie(struct listbox_item *item)
{
	return 1;
}

static void
delete_cookie_item(struct listbox_item *item, int last)
{
	struct cookie *cookie = item->udata;

	if (item->type == BI_FOLDER) {
		struct listbox_item *next, *root = item;

		/* Releasing refcounts on the cookie_server will automagically
		 * delete it. */
		foreachsafe (item, next, root->child)
			delete_cookie_item(item, 0);
	} else {
		assert(!is_object_used(cookie));

		delete_cookie(cookie);
	}

	if (last
	    && get_opt_bool("cookies.save")
	    && get_opt_bool("cookies.resave"))
		save_cookies();
}

static struct listbox_ops_messages cookies_messages = {
	/* cant_delete_item */
	N_("Sorry, but cookie \"%s\" cannot be deleted."),
	/* cant_delete_used_item */
	N_("Sorry, but cookie \"%s\" is being used by something else."),
	/* cant_delete_folder */
	N_("Sorry, but cookie domain \"%s\" cannot be deleted."),
	/* cant_delete_used_folder */
	N_("Sorry, but cookie domain \"%s\" is being used by something else."),
	/* delete_marked_items_title */
	N_("Delete marked cookies"),
	/* delete_marked_items */
	N_("Delete marked cookies?"),
	/* delete_folder_title */
	N_("Delete domain's cookies"),
	/* delete_folder */
	N_("Delete all cookies from domain \"%s\"?"),
	/* delete_item_title */
	N_("Delete cookie"),
	/* delete_item */
	N_("Delete this cookie?"),
	/* clear_all_items_title */
	N_("Clear all cookies"),
	/* clear_all_items_title */
	N_("Do you really want to remove all cookies?"),
};

static struct listbox_ops cookies_listbox_ops = {
	lock_cookie,
	unlock_cookie,
	is_cookie_used,
	get_cookie_text,
	get_cookie_info,
	NULL,
	get_cookie_root,
	NULL,
	can_delete_cookie,
	delete_cookie_item,
	NULL,
	&cookies_messages,
};

static widget_handler_status_T
set_cookie_name(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct cookie *cookie = dlg_data->dlg->udata;
	unsigned char *value = widget_data->cdata;

	if (!value || !cookie) return EVENT_NOT_PROCESSED;
	mem_free_set(&cookie->name, stracpy(value));
	return EVENT_PROCESSED;
}

static widget_handler_status_T
set_cookie_value(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct cookie *cookie = dlg_data->dlg->udata;
	unsigned char *value = widget_data->cdata;

	if (!value || !cookie) return EVENT_NOT_PROCESSED;
	mem_free_set(&cookie->value, stracpy(value));
	return EVENT_PROCESSED;
}

static widget_handler_status_T
set_cookie_domain(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct cookie *cookie = dlg_data->dlg->udata;
	unsigned char *value = widget_data->cdata;

	if (!value || !cookie) return EVENT_NOT_PROCESSED;
	mem_free_set(&cookie->domain, stracpy(value));
	return EVENT_PROCESSED;
}

static widget_handler_status_T
set_cookie_expires(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct cookie *cookie = dlg_data->dlg->udata;
	unsigned char *value = widget_data->cdata;
	unsigned char *end;
	long number;

	if (!value || !cookie) return EVENT_NOT_PROCESSED;

	errno = 0;
	number = strtol(value, (char **) &end, 10);
	if (errno || *end || number < 0) return EVENT_NOT_PROCESSED;

	cookie->expires = (time_t) number;
	return EVENT_PROCESSED;
}

static widget_handler_status_T
set_cookie_secure(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct cookie *cookie = dlg_data->dlg->udata;
	unsigned char *value = widget_data->cdata;
	unsigned char *end;
	long number;

	if (!value || !cookie) return EVENT_NOT_PROCESSED;

	errno = 0;
	number = strtol(value, (char **) &end, 10);
	if (errno || *end) return EVENT_NOT_PROCESSED;

	cookie->secure = (number != 0);
	return EVENT_PROCESSED;
}

static void
build_edit_dialog(struct terminal *term, struct cookie *cookie)
{
#define EDIT_WIDGETS_COUNT 8
	struct dialog *dlg;
	unsigned char *name, *value, *domain, *expires, *secure;
	unsigned char *dlg_server;
	int length = 0;

	dlg = calloc_dialog(EDIT_WIDGETS_COUNT, MAX_STR_LEN * 5);
	if (!dlg) return;

	dlg->title = _("Edit", term);
	dlg->layouter = generic_dialog_layouter;
	dlg->udata = cookie;
	dlg->udata2 = NULL;

	name = get_dialog_offset(dlg, EDIT_WIDGETS_COUNT);
	value = name + MAX_STR_LEN;
	domain = value + MAX_STR_LEN;
	expires = domain + MAX_STR_LEN;
	secure = expires + MAX_STR_LEN;

	safe_strncpy(name, cookie->name, MAX_STR_LEN);
	safe_strncpy(value, cookie->value, MAX_STR_LEN);
	safe_strncpy(domain, cookie->domain, MAX_STR_LEN);
	ulongcat(expires, &length, cookie->expires, MAX_STR_LEN, 0);
	length = 0;
	ulongcat(secure, &length, cookie->secure, MAX_STR_LEN, 0);

	dlg_server = cookie->server->host;
	dlg_server = straconcat(_("Server", term), ": ", dlg_server, "\n", NULL);

	if (!dlg_server) {
		mem_free(dlg);
		return;
	}

	add_dlg_text(dlg, dlg_server, ALIGN_LEFT, 0);
	add_dlg_field_float(dlg, _("Name", term), 0, 0, set_cookie_name, MAX_STR_LEN, name, NULL);
	add_dlg_field_float(dlg, _("Value", term), 0, 0, set_cookie_value, MAX_STR_LEN, value, NULL);
	add_dlg_field_float(dlg, _("Domain", term), 0, 0, set_cookie_domain, MAX_STR_LEN, domain, NULL);
	add_dlg_field_float(dlg, _("Expires", term), 0, 0, set_cookie_expires, MAX_STR_LEN, expires, NULL);
	add_dlg_field_float(dlg, _("Secure", term), 0, 0, set_cookie_secure, MAX_STR_LEN, secure, NULL);

	add_dlg_button(dlg, _("~OK", term), B_ENTER, ok_dialog, NULL);
	add_dlg_button(dlg, _("~Cancel", term), B_ESC, cancel_dialog, NULL);

	add_dlg_end(dlg, EDIT_WIDGETS_COUNT);

	do_dialog(term, dlg, getml(dlg, dlg_server, NULL));
#undef EDIT_WIDGETS_COUNT
}

static widget_handler_status_T
push_edit_button(struct dialog_data *dlg_data, struct widget_data *button)
{
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct terminal *term = dlg_data->win->term;
	struct cookie *cookie;

	if (!box->sel) return EVENT_PROCESSED;
	if (box->sel->type == BI_FOLDER) return EVENT_PROCESSED;
	cookie = box->sel->udata;
	if (!cookie) return EVENT_PROCESSED;
	build_edit_dialog(term, cookie);
	return EVENT_PROCESSED;
}

static widget_handler_status_T
push_add_button(struct dialog_data *dlg_data, struct widget_data *button)
{
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct terminal *term = dlg_data->win->term;
	struct cookie *new_cookie;
	struct cookie_server *server;

	if (!box->sel || !box->sel->udata) return EVENT_PROCESSED;

	new_cookie = mem_calloc(1, sizeof(*new_cookie));
	if (!new_cookie) return EVENT_PROCESSED;

	if (box->sel->type == BI_FOLDER) {
		assert(box->sel->depth == 0);
		server = box->sel->udata;
	} else {
		struct cookie *cookie = box->sel->udata;

		server = cookie->server;
	}

	object_lock(server);
	new_cookie->server = server;

	new_cookie->name = stracpy("");
	new_cookie->value = stracpy("");
	new_cookie->domain = stracpy("");
	accept_cookie(new_cookie);
	build_edit_dialog(term, new_cookie);
	return EVENT_PROCESSED;
}

static widget_handler_status_T
push_save_button(struct dialog_data *dlg_data, struct widget_data *button)
{
	save_cookies();
	return EVENT_PROCESSED;
}

static struct hierbox_browser_button cookie_buttons[] = {
	{ N_("~Info"),		push_hierbox_info_button,	1 },
	{ N_("~Add"),		push_add_button,		1 },
	{ N_("~Edit"),		push_edit_button,		1 },
	{ N_("~Delete"),	push_hierbox_delete_button,	1 },
	{ N_("~Clear"),		push_hierbox_clear_button,	1 },
	{ N_("Sa~ve"),		push_save_button,		0 },
};

struct_hierbox_browser(
	cookie_browser,
	N_("Cookie manager"),
	cookie_buttons,
	&cookies_listbox_ops
);

void
cookie_manager(struct session *ses)
{
	hierbox_browser(&cookie_browser, ses);
}
