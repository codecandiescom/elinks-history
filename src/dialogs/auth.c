/* HTTP Auth dialog stuff */
/* $Id: auth.c,v 1.42 2003/07/23 08:37:47 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/button.h"
#include "bfu/dialog.h"
#include "bfu/inpfield.h"
#include "bfu/text.h"
#include "dialogs/auth.h"
#include "intl/gettext/libintl.h"
#include "protocol/http/auth.h"
#include "sched/session.h"
#include "terminal/terminal.h"
#include "util/memory.h"


static void
auth_layout(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = -1;
	int dialog_text_color = get_bfu_color(term, "dialog.text");
	unsigned char *label_login = N_("Login");
	unsigned char *label_password = N_("Password");

	text_width(term, label_login, &min, &max);
	text_width(term, label_password, &min, &max);
	buttons_width(term, dlg->items + 2, 2, &min, &max);

	w = dlg->win->term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w < min) w = min;
	if (w > dlg->win->term->x - 2 * DIALOG_LB)
		w = dlg->win->term->x - 2 * DIALOG_LB;
	if (w < 1) w = 1;
	rw = 0;
	if (dlg->dlg->udata) {
		dlg_format_text(NULL, term,
				dlg->dlg->udata, 0, &y, w, &rw,
				dialog_text_color, AL_LEFT);
		y++;
	}

	dlg_format_text(NULL, term,
			label_login, 0, &y, w, &rw,
			dialog_text_color, AL_LEFT);
	y += 2;
	dlg_format_text(NULL, term,
			label_password, 0, &y, w, &rw,
			dialog_text_color, AL_LEFT);
	y += 2;
	dlg_format_buttons(NULL, term,
			   dlg->items + 2, 2,
			   0, &y, w, &rw, AL_CENTER);
	w = rw;
	dlg->xw = w + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);
	draw_dlg(dlg);
	y = dlg->y + DIALOG_TB;
	if (dlg->dlg->udata) {
		dlg_format_text(term, term,
				dlg->dlg->udata, dlg->x + DIALOG_LB, &y, w, NULL,
				dialog_text_color, AL_LEFT);
		y++;
	}
	dlg_format_text(term, term,
			label_login, dlg->x + DIALOG_LB, &y, w, NULL,
			dialog_text_color, AL_LEFT);
	dlg_format_field(term, term,
			 &dlg->items[0],
			 dlg->x + DIALOG_LB, &y, w, NULL, AL_LEFT);
	y++;
	dlg_format_text(term, term,
			label_password, dlg->x + DIALOG_LB, &y, w, NULL,
			dialog_text_color, AL_LEFT);
	dlg_format_field(term, term,
			 &dlg->items[1],
			 dlg->x + DIALOG_LB, &y, w, NULL, AL_LEFT);
	y++;
	dlg_format_buttons(term, term,
			   &dlg->items[2], 2,
			   dlg->x + DIALOG_LB, &y, w, NULL, AL_CENTER);
}

static int
auth_ok(struct dialog_data *dlg, struct widget_data *di)
{
	struct http_auth_basic *entry = dlg->dlg->udata2;

	entry->blocked = 0;
	entry->valid = auth_entry_has_userinfo(entry);
	reload(dlg->dlg->refresh_data, -1);
	return ok_dialog(dlg, di);
}

static int
auth_cancel(struct dialog_data *dlg, struct widget_data *di)
{
	((struct http_auth_basic *)dlg->dlg->udata2)->blocked = 0;
	del_auth_entry(dlg->dlg->udata2);
	return cancel_dialog(dlg, di);
}

void
do_auth_dialog(struct session *ses)
{
	struct dialog *d;
	struct dialog_data *dd;
	struct terminal *term = ses->tab->term;
	struct http_auth_basic *a = get_invalid_auth_entry();
	unsigned char sticker[MAX_STR_LEN];

	if (!a || a->blocked) return;
	a->blocked = 1;

	snprintf(sticker, sizeof(sticker),
		_("Authentication required for %s at %s\n", term),
		a->realm, a->url);

#define DLG_SIZE sizeof(struct dialog) + 5 * sizeof(struct widget)

	d = mem_calloc(1, DLG_SIZE + strlen(sticker) + 1);
	if (!d) return;

	d->title = _("HTTP Authentication", term);
	d->fn = auth_layout;

	d->udata = (char *)d + DLG_SIZE;
	strcpy(d->udata, sticker);

#undef DLG_SIZE

	d->udata2 = a;
	d->refresh_data = ses;

	d->items[0].type = D_FIELD;
	d->items[0].dlen = HTTP_AUTH_USER_MAXLEN;
	d->items[0].data = a->user;

	d->items[1].type = D_FIELD_PASS;
	d->items[1].dlen = HTTP_AUTH_PASSWORD_MAXLEN;
	d->items[1].data = a->password;

	d->items[2].type = D_BUTTON;
	d->items[2].gid = B_ENTER;
	d->items[2].fn = auth_ok;
	d->items[2].text = _("OK", term);
	d->items[3].type = D_BUTTON;
	d->items[3].gid = B_ESC;
	d->items[3].fn = auth_cancel;
	d->items[3].text = _("Cancel", term);

	d->items[4].type = D_END;
	dd = do_dialog(term, d, getml(d, NULL));
	/* When there's some username, but no password, automagically jump at
	 * the password. */
	if (a->user[0] && !a->password[0])
		dd->selected = 1;
}
