/* HTTP Auth dialog stuff */
/* $Id: dialogs.c,v 1.109 2004/07/22 02:06:02 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/button.h"
#include "bfu/dialog.h"
#include "bfu/hierbox.h"
#include "bfu/inpfield.h"
#include "bfu/listbox.h"
#include "bfu/msgbox.h"
#include "bfu/style.h"
#include "bfu/text.h"
#include "intl/gettext/libintl.h"
#include "protocol/auth/auth.h"
#include "protocol/auth/dialogs.h"
#include "protocol/uri.h"
#include "sched/session.h"
#include "terminal/terminal.h"
#include "util/color.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/object.h"
#include "util/snprintf.h"
#include "util/string.h"


static void
auth_ok(struct dialog *dlg)
{
	struct http_auth_basic *entry = dlg->udata2;

	entry->blocked = 0;
	entry->valid = auth_entry_has_userinfo(entry);
	reload(dlg->udata, CACHE_MODE_INCREMENT);
}

static void
auth_cancel(struct http_auth_basic *entry)
{
	entry->blocked = 0;
	del_auth_entry(entry);
}

/* TODO: Take http_auth_basic from data. --jonas */
void
do_auth_dialog(struct session *ses, void *data)
{
	struct dialog *dlg;
	struct dialog_data *dlg_data;
	struct terminal *term = ses->tab->term;
	struct http_auth_basic *a = get_invalid_auth_entry();
	unsigned char sticker[MAX_STR_LEN], *text;

	if (!a || a->blocked) return;

	text = get_uri_string(a->uri, URI_HTTP_AUTH);
	if (!text) return;

	snprintf(sticker, sizeof(sticker),
		 _("Authentication required for %s at %s", term),
		 a->realm, text);
	mem_free(text);

#define AUTH_WIDGETS_COUNT 5
	dlg = calloc_dialog(AUTH_WIDGETS_COUNT, strlen(sticker) + 1);
	if (!dlg) return;

	a->blocked = 1;

	dlg->title = _("HTTP Authentication", term);
	dlg->layouter = generic_dialog_layouter;

	text = get_dialog_offset(dlg, AUTH_WIDGETS_COUNT);
	strcpy(text, sticker);

	dlg->udata = (void *) ses;
	dlg->udata2 = a;

	add_dlg_text(dlg, text, ALIGN_LEFT, 0);
	add_dlg_field(dlg, _("Login", term), 0, 0, NULL, HTTP_AUTH_USER_MAXLEN, a->user, NULL);
	dlg->widgets[dlg->widgets_size - 1].info.field.float_label = 1;
	add_dlg_field_pass(dlg, _("Password", term), 0, 0, NULL, HTTP_AUTH_PASSWORD_MAXLEN, a->password);
	dlg->widgets[dlg->widgets_size - 1].info.field.float_label = 1;

	add_dlg_ok_button(dlg, B_ENTER, _("OK", term), auth_ok, dlg);
	add_dlg_ok_button(dlg, B_ESC, _("Cancel", term), auth_cancel, a);

	add_dlg_end(dlg, AUTH_WIDGETS_COUNT);

	dlg_data = do_dialog(term, dlg, getml(dlg, NULL));
	/* When there's some username, but no password, automagically jump at
	 * the password. */
	if (dlg_data && a->user[0] && !a->password[0])
		dlg_data->selected = 1;
}


static void
lock_http_auth_basic(struct listbox_item *item)
{
	object_lock((struct http_auth_basic *) item->udata);
}

static void
unlock_http_auth_basic(struct listbox_item *item)
{
	object_unlock((struct http_auth_basic *) item->udata);
}

static int
is_http_auth_basic_used(struct listbox_item *item)
{
	return is_object_used((struct http_auth_basic *) item->udata);
}

static unsigned char *
get_http_auth_basic_text(struct listbox_item *item, struct terminal *term)
{
	struct http_auth_basic *http_auth_basic = item->udata;

	return get_uri_string(http_auth_basic->uri, URI_HTTP_AUTH);
}

static unsigned char *
get_http_auth_basic_info(struct listbox_item *item, struct terminal *term)
{
	struct http_auth_basic *http_auth_basic = item->udata;
	struct string info;

	if (item->type == BI_FOLDER) return NULL;
	if (!init_string(&info)) return NULL;

	add_format_to_string(&info, "%s: ", _("URL", term));
	add_uri_to_string(&info, http_auth_basic->uri, URI_HTTP_AUTH);
	add_format_to_string(&info, "\n%s: %s\n", _("Realm", term), http_auth_basic->realm);
	add_format_to_string(&info, "%s: %s\n", _("State", term),
		http_auth_basic->valid ? _("valid", term) : _("invalid", term));

	return info.source;
}

static struct uri *
get_http_auth_basic_uri(struct listbox_item *item)
{
	struct http_auth_basic *http_auth_basic = item->udata;

	return get_composed_uri(http_auth_basic->uri, URI_HTTP_AUTH);
}

static struct listbox_item *
get_http_auth_basic_root(struct listbox_item *box_item)
{
	return NULL;
}

static int
can_delete_http_auth_basic(struct listbox_item *item)
{
	return 1;
}

static void
delete_http_auth_basic(struct listbox_item *item, int last)
{
	struct http_auth_basic *http_auth_basic = item->udata;

	assert(!is_object_used(http_auth_basic));

	del_auth_entry(http_auth_basic);
}

static struct listbox_ops_messages http_auth_messages = {
	/* cant_delete_item */
	N_("Sorry, but auth entry \"%s\" cannot be deleted."),
	/* cant_delete_used_item */
	N_("Sorry, but auth entry \"%s\" is being used by something else."),
	/* cant_delete_folder */
	NULL,
	/* cant_delete_used_folder */
	NULL,
	/* delete_marked_items_title */
	N_("Delete marked auth entries"),
	/* delete_marked_items */
	N_("Delete marked auth entries?"),
	/* delete_folder_title */
	NULL,
	/* delete_folder */
	NULL,
	/* delete_item_title */
	N_("Delete auth entry"),
	/* delete_item */
	N_("Delete this auth entry?"),
	/* clear_all_items_title */
	N_("Clear all auth entries"),
	/* clear_all_items_title */
	N_("Do you really want to remove all auth entries?"),
};

static struct listbox_ops auth_listbox_ops = {
	lock_http_auth_basic,
	unlock_http_auth_basic,
	is_http_auth_basic_used,
	get_http_auth_basic_text,
	get_http_auth_basic_info,
	get_http_auth_basic_uri,
	get_http_auth_basic_root,
	NULL,
	can_delete_http_auth_basic,
	delete_http_auth_basic,
	NULL,
	&http_auth_messages,
};

static struct hierbox_browser_button auth_buttons[] = {
	{ N_("Goto"),		push_hierbox_goto_button,	1 },
	{ N_("Info"),		push_hierbox_info_button,	1 },
	{ N_("Delete"),		push_hierbox_delete_button,	1 },
	{ N_("Clear"),		push_hierbox_clear_button,	1 },
};

struct_hierbox_browser(
	auth_browser,
	N_("Authentication manager"),
	auth_buttons,
	&auth_listbox_ops
);

void
auth_manager(struct session *ses)
{
	hierbox_browser(&auth_browser, ses);
}
