/* HTTP Auth dialog stuff */
/* $Id: auth.c,v 1.74 2003/11/06 16:52:08 jonas Exp $ */

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


static void
auth_layout(struct dialog_data *dlg_data)
{
	struct terminal *term = dlg_data->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = -1;
	struct color_pair *dialog_text_color = get_bfu_color(term, "dialog.text");
	unsigned char *label_login = N_("Login");
	unsigned char *label_password = N_("Password");

	if (dlg_data->dlg->udata)
		text_width(term, dlg_data->dlg->udata, &min, &max);

	text_width(term, label_login, &min, &max);
	text_width(term, label_password, &min, &max);
	buttons_width(dlg_data->widgets_data + 2, 2, &min, &max);

	w = term->width * 9 / 10 - 2 * DIALOG_LB;
	int_bounds(&w, min, max);
	int_bounds(&w, 1, term->width - 2 * DIALOG_LB);

	rw = 0;
	if (dlg_data->dlg->udata) {
		dlg_format_text(NULL,
				dlg_data->dlg->udata, 0, &y, w, &rw,
				dialog_text_color, AL_LEFT);
		y++;
	}

	dlg_format_text(NULL,
			label_login, 0, &y, w, &rw,
			dialog_text_color, AL_LEFT);
	y += 2;
	dlg_format_text(NULL,
			label_password, 0, &y, w, &rw,
			dialog_text_color, AL_LEFT);
	y += 2;
	dlg_format_buttons(NULL,
			   dlg_data->widgets_data + 2, 2,
			   0, &y, w, &rw, AL_CENTER);
	w = rw;

	draw_dlg(dlg_data, w, y, AL_CENTER);

	y = dlg_data->y + DIALOG_TB;
	if (dlg_data->dlg->udata) {
		dlg_format_text(term,
				dlg_data->dlg->udata, dlg_data->x + DIALOG_LB,
				&y, w, NULL,
				dialog_text_color, AL_LEFT);
		y++;
	}
	dlg_format_text(term,
			label_login, dlg_data->x + DIALOG_LB, &y, w, NULL,
			dialog_text_color, AL_LEFT);
	dlg_format_field(term,
			 &dlg_data->widgets_data[0],
			 dlg_data->x + DIALOG_LB, &y, w, NULL, AL_LEFT);
	y++;
	dlg_format_text(term,
			label_password, dlg_data->x + DIALOG_LB, &y, w, NULL,
			dialog_text_color, AL_LEFT);
	dlg_format_field(term,
			 &dlg_data->widgets_data[1],
			 dlg_data->x + DIALOG_LB, &y, w, NULL, AL_LEFT);
	y++;
	dlg_format_buttons(term,
			   &dlg_data->widgets_data[2], 2,
			   dlg_data->x + DIALOG_LB, &y, w, NULL, AL_CENTER);
}

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
	unsigned char sticker[MAX_STR_LEN];

	if (!a || a->blocked) return;
	a->blocked = 1;

	snprintf(sticker, sizeof(sticker),
		_("Authentication required for %s at %s", term),
		a->realm, a->url);

#define AUTH_WIDGETS_COUNT 4
	dlg = calloc_dialog(AUTH_WIDGETS_COUNT, strlen(sticker) + 1);
	if (!dlg) return;

	dlg->title = _("HTTP Authentication", term);
	dlg->fn = auth_layout;

	dlg->udata = (char *)dlg + sizeof_dialog(AUTH_WIDGETS_COUNT, 0);
	strcpy(dlg->udata, sticker);

	dlg->udata2 = a;
	dlg->refresh_data = ses;

	add_dlg_field(dlg, 0, 0, NULL, HTTP_AUTH_USER_MAXLEN, a->user, NULL);
	add_dlg_field_pass(dlg, 0, 0, NULL, HTTP_AUTH_PASSWORD_MAXLEN, a->password);

	add_dlg_button(dlg, B_ENTER, auth_ok, _("OK", term), NULL);
	add_dlg_button(dlg, B_ESC, auth_cancel, _("Cancel", term), NULL);

	add_dlg_end(dlg, AUTH_WIDGETS_COUNT);

	dlg_data = do_dialog(term, dlg, getml(dlg, NULL));
	/* When there's some username, but no password, automagically jump at
	 * the password. */
	if (a->user[0] && !a->password[0])
		dlg_data->selected = 1;
}
