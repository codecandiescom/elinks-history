/* Cookie-related dialogs */
/* $Id: dialogs.c,v 1.45 2004/05/20 23:02:54 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef CONFIG_COOKIES

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/button.h"
#include "bfu/dialog.h"
#include "bfu/hierbox.h"
#include "bfu/inpfield.h"
#include "bfu/listbox.h"
#include "bfu/msgbox.h"
#include "bfu/text.h"
#include "cookies/cookies.h"
#include "cookies/dialogs.h"
#include "dialogs/edit.h"
#include "intl/gettext/libintl.h"
#include "sched/session.h"
#include "terminal/draw.h"
#include "terminal/kbd.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/object.h"
#include "util/string.h"


static void
lock_cookie(struct listbox_item *item)
{
	if (item->udata) object_lock((struct cookie *)item->udata);
}

static void
unlock_cookie(struct listbox_item *item)
{
	if (item->udata) object_unlock((struct cookie *)item->udata);
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
	struct string string;

	if (listbox_info == LISTBOX_URI)
		return NULL;

	if (!init_string(&string)) return NULL;

	add_format_to_string(&string, "%s: %s", _("Server", term), cookie->server);
	add_format_to_string(&string, "\n%s: %s", _("Name", term), cookie->name);
	add_format_to_string(&string, "\n%s: %s", _("Value", term), cookie->value);
	add_format_to_string(&string, "\n%s: %s", _("Domain", term), cookie->domain);

#ifdef HAVE_STRFTIME
	if (cookie->expires) {
		struct tm *when_local = localtime(&cookie->expires);
		unsigned char str[13];
		int wr = strftime(str, sizeof(str), "%b %e %H:%M", when_local);

		if (wr > 0)
			add_format_to_string(&string,
				"\n%s: %s", _("Expires", term), str);
	}
#endif

	add_format_to_string(&string, "\n%s: %s", _("Secure", term),
			     _(cookie->secure ? N_("yes") : N_("no"), term));

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
	NULL
};

static int
set_cookie_name(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct cookie *cookie = dlg_data->dlg->udata;
	unsigned char *value = widget_data->cdata;

	if (!value || !cookie) return 1;
	mem_free_set(&cookie->name, stracpy(value));

	assert(cookie->box_item);
	cookie->box_item->text = cookie->name;
	return 0;
}

static int
set_cookie_value(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct cookie *cookie = dlg_data->dlg->udata;
	unsigned char *value = widget_data->cdata;

	if (!value || !cookie) return 1;
	mem_free_set(&cookie->value, stracpy(value));
	return 0;
}

static int
set_cookie_domain(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct cookie *cookie = dlg_data->dlg->udata;
	unsigned char *value = widget_data->cdata;

	if (!value || !cookie) return 1;
	mem_free_set(&cookie->domain, stracpy(value));
	return 0;
}

static int
set_cookie_expires(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct cookie *cookie = dlg_data->dlg->udata;
	unsigned char *value = widget_data->cdata;
	unsigned char *end;
	long number;

	if (!value || !cookie) return 1;

	errno = 0;
	number = strtol(value, (char **)&end, 10);
	if (errno || *end || number < 0) return 1;

	cookie->expires = (ttime) number;
	return 0;
}

static int
set_cookie_secure(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct cookie *cookie = dlg_data->dlg->udata;
	unsigned char *value = widget_data->cdata;
	unsigned char *end;
	long number;

	if (!value || !cookie) return 1;

	errno = 0;
	number = strtol(value, (char **)&end, 10);
	if (errno || *end) return 1;

	cookie->secure = (number != 0);
	return 0;
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

	dlg_server = straconcat(_("Server", term), ": ", cookie->server, "\n", NULL);

	if (!dlg_server) {
		mem_free(dlg);
		return;
	}

	add_dlg_text(dlg, dlg_server, AL_LEFT, 0);
	add_dlg_field(dlg, _("Name", term), 0, 0, set_cookie_name, MAX_STR_LEN, name, NULL);
	dlg->widgets[dlg->widgets_size - 1].info.field.float_label = 1;
	add_dlg_field(dlg, _("Value", term), 0, 0, set_cookie_value, MAX_STR_LEN, value, NULL);
	dlg->widgets[dlg->widgets_size - 1].info.field.float_label = 1;
	add_dlg_field(dlg, _("Domain", term), 0, 0, set_cookie_domain, MAX_STR_LEN, domain, NULL);
	dlg->widgets[dlg->widgets_size - 1].info.field.float_label = 1;
	add_dlg_field(dlg, _("Expires", term), 0, 0, set_cookie_expires, MAX_STR_LEN, expires, NULL);
	dlg->widgets[dlg->widgets_size - 1].info.field.float_label = 1;
	add_dlg_field(dlg, _("Secure", term), 0, 0, set_cookie_secure, MAX_STR_LEN, secure, NULL);
	dlg->widgets[dlg->widgets_size - 1].info.field.float_label = 1;

	add_dlg_button(dlg, B_ENTER, ok_dialog, _("OK", term), NULL);
	add_dlg_button(dlg, B_ESC, cancel_dialog, _("Cancel", term), NULL);

	add_dlg_end(dlg, EDIT_WIDGETS_COUNT);

	do_dialog(term, dlg, getml(dlg, dlg_server, NULL));
#undef EDIT_WIDGETS_COUNT
}

static int
push_edit_button(struct dialog_data *dlg_data, struct widget_data *button)
{
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct terminal *term = dlg_data->win->term;
	struct cookie *cookie;

	if (!box->sel) return 0;
	if (box->sel->type == BI_FOLDER) return 0;
	cookie = box->sel->udata;
	if (!cookie) return 0;
	build_edit_dialog(term, cookie);
	return 0;
}

static int
push_add_button(struct dialog_data *dlg_data, struct widget_data *button)
{
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct terminal *term = dlg_data->win->term;
	struct cookie *new_cookie;

	if (!box->sel) return 0;

	new_cookie = mem_calloc(1, sizeof(struct cookie));
	if (!new_cookie) return 0;

	if (box->sel->type == BI_FOLDER) {
		new_cookie->server = stracpy(box->sel->text);
	} else {
		struct cookie *cookie = box->sel->udata;

		if (cookie) {
			new_cookie->server = stracpy(cookie->server);
		} else {
			mem_free(new_cookie);
			return 0;
		}
	}
	new_cookie->name = stracpy("");
	new_cookie->value = stracpy("");
	new_cookie->domain = stracpy("");
	accept_cookie(new_cookie);
	build_edit_dialog(term, new_cookie);
	return 0;
}

static int
push_save_button(struct dialog_data *dlg_data, struct widget_data *button)
{
	save_cookies();
	return 0;
}

static struct hierbox_browser_button cookie_buttons[] = {
	{ N_("Info"),		push_hierbox_info_button,	1 },
	{ N_("Add"),		push_add_button,		1 },
	{ N_("Edit"),		push_edit_button,		1 },
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
cookie_manager(struct session *ses)
{
	hierbox_browser(&cookie_browser, ses);
}

#endif /* CONFIG_COOKIES */
