/* HTTP Auth dialog stuff */
/* $Id: dialogs.c,v 1.95 2004/04/11 00:59:19 jonas Exp $ */

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
#include "intl/gettext/libintl.h"
#include "protocol/auth/auth.h"
#include "protocol/auth/dialogs.h"
#include "sched/session.h"
#include "terminal/terminal.h"
#include "util/color.h"
#include "util/memory.h"
#include "util/snprintf.h"


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
	unsigned char *dlg_login, *dlg_pass;

	if (!a || a->blocked) return;

	dlg_login = straconcat(_("Login", term), ": ", NULL);
	if (!dlg_login) return;
	dlg_pass = straconcat(_("Password", term), ": ", NULL);
	if (!dlg_pass) {
		mem_free(dlg_login);
		return;
	}

	snprintf(sticker, sizeof(sticker),
		 _("Authentication required for %s at %s", term),
		 a->realm, a->url);

#define AUTH_WIDGETS_COUNT 5
	dlg = calloc_dialog(AUTH_WIDGETS_COUNT, strlen(sticker) + 1);
	if (!dlg) {
		mem_free(dlg_login);
		mem_free(dlg_pass);
		return;
	}

	a->blocked = 1;

	dlg->title = _("HTTP Authentication", term);
	dlg->layouter = generic_dialog_layouter;

	text = get_dialog_offset(dlg, AUTH_WIDGETS_COUNT);
	strcpy(text, sticker);

	dlg->udata = (void *) ses;
	dlg->udata2 = a;

	add_dlg_text(dlg, text, AL_LEFT, 0);
	add_dlg_field(dlg, dlg_login, 0, 0, NULL, HTTP_AUTH_USER_MAXLEN, a->user, NULL);
	dlg->widgets[dlg->widgets_size - 1].info.field.float_label = 1;
	add_dlg_field_pass(dlg, dlg_pass, 0, 0, NULL, HTTP_AUTH_PASSWORD_MAXLEN, a->password);
	dlg->widgets[dlg->widgets_size - 1].info.field.float_label = 1;

	add_dlg_ok_button(dlg, B_ENTER, _("OK", term), auth_ok, dlg);
	add_dlg_ok_button(dlg, B_ESC, _("Cancel", term), auth_cancel, a);

	add_dlg_end(dlg, AUTH_WIDGETS_COUNT);

	dlg_data = do_dialog(term, dlg, getml(dlg, dlg_login, dlg_pass, NULL));
	/* When there's some username, but no password, automagically jump at
	 * the password. */
	if (dlg_data && a->user[0] && !a->password[0])
		dlg_data->selected = 1;
}
