/* HTTP Auth dialog stuff */
/* $Id: auth.c,v 1.84 2003/11/10 00:32:36 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/button.h"
#include "bfu/dialog.h"
#include "bfu/inpfield.h"
#include "bfu/style.h"
#include "bfu/text.h"
#include "dialogs/auth.h"
#include "intl/gettext/libintl.h"
#include "protocol/http/auth.h"
#include "sched/session.h"
#include "terminal/terminal.h"
#include "util/color.h"
#include "util/memory.h"
#include "util/snprintf.h"


static int
auth_ok(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct http_auth_basic *entry = dlg_data->dlg->udata2;

	entry->blocked = 0;
	entry->valid = auth_entry_has_userinfo(entry);
	reload(dlg_data->dlg->refresh_data, -1);
	return ok_dialog(dlg_data, widget_data);
}

static int
auth_cancel(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	((struct http_auth_basic *)dlg_data->dlg->udata2)->blocked = 0;
	del_auth_entry(dlg_data->dlg->udata2);
	return cancel_dialog(dlg_data, widget_data);
}

void
do_auth_dialog(struct session *ses)
{
	struct dialog *dlg;
	struct dialog_data *dlg_data;
	struct terminal *term = ses->tab->term;
	struct http_auth_basic *a = get_invalid_auth_entry();
	unsigned char sticker[MAX_STR_LEN], *text;

	if (!a || a->blocked) return;
	a->blocked = 1;

	snprintf(sticker, sizeof(sticker),
		_("Authentication required for %s at %s", term),
		a->realm, a->url);

#define AUTH_WIDGETS_COUNT 5
	dlg = calloc_dialog(AUTH_WIDGETS_COUNT, strlen(sticker) + 1);
	if (!dlg) return;

	dlg->title = _("HTTP Authentication", term);
	dlg->layouter = generic_dialog_layouter;

	text = (unsigned char *) dlg + sizeof_dialog(AUTH_WIDGETS_COUNT, 0);
	strcpy(text, sticker);

	dlg->udata2 = a;
	dlg->refresh_data = ses;

	add_dlg_text(dlg, text, AL_LEFT, 0);
	add_dlg_field(dlg, _("Login", term), 0, 0, NULL, HTTP_AUTH_USER_MAXLEN, a->user, NULL);
	add_dlg_field_pass(dlg, _("Password", term), 0, 0, NULL, HTTP_AUTH_PASSWORD_MAXLEN, a->password);

	add_dlg_button(dlg, B_ENTER, auth_ok, _("OK", term), NULL);
	add_dlg_button(dlg, B_ESC, auth_cancel, _("Cancel", term), NULL);

	add_dlg_end(dlg, AUTH_WIDGETS_COUNT);

	dlg_data = do_dialog(term, dlg, getml(dlg, NULL));
	/* When there's some username, but no password, automagically jump at
	 * the password. */
	if (dlg_data && a->user[0] && !a->password[0])
		dlg_data->selected = 1;
}
