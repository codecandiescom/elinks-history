/* HTTP Auth dialog stuff */
/* $Id: auth.c,v 1.32 2003/06/27 20:42:38 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "bfu/button.h"
#include "bfu/inpfield.h"
#include "bfu/text.h"
#include "dialogs/auth.h"
#include "intl/gettext/libintl.h"
#include "terminal/terminal.h"
#include "protocol/http/auth.h"
#include "sched/session.h"
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
	((struct http_auth_basic *)dlg->dlg->udata2)->blocked = 0;
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

/* FIXME: This should be exported properly. --pasky */
extern struct list_head http_auth_basic_list;

void
do_auth_dialog(struct session *ses)
{
	struct dialog *d;
	struct dialog_data *dd;
	struct terminal *term = ses->tab->term;
	struct http_auth_basic *a = NULL;
	unsigned char sticker[MAX_STR_LEN];

	if (!list_empty(http_auth_basic_list)
	    && !((struct http_auth_basic *) http_auth_basic_list.next)->valid)
		  a = (struct http_auth_basic *) http_auth_basic_list.next;

	if (!a || a->blocked) return;
	a->valid = 1;
	a->blocked = 1;
	if (!a->uid) {
		a->uid = mem_alloc(MAX_UID_LEN);
		if (!a->uid) {
			del_auth_entry(a);
			return;
		}
		*a->uid = 0;
	}
	if (!a->passwd) {
		a->passwd = mem_alloc(MAX_PASSWD_LEN);
		if (!a->passwd) {
			del_auth_entry(a);
			return;
		}
		*a->passwd = 0;
	}

	snprintf(sticker, MAX_STR_LEN,
		_("Authentication required for %s at %s", term),
		a->realm, a->url);

#define DLG_SIZE sizeof(struct dialog) + 5 * sizeof(struct widget)

	d = mem_calloc(1, DLG_SIZE + strlen(sticker) + 1);
	if (!d) {
		if (a->uid) {
			mem_free(a->uid);
			a->uid = NULL;
		}
		if (a->passwd) {
			mem_free(a->passwd);
			a->passwd = NULL;
		}
		return;
	}

	d->title = _("HTTP Authentication", term);
	d->fn = auth_layout;

	d->udata = (char *)d + DLG_SIZE;
	strcpy(d->udata, sticker);

#undef DLG_SIZE

	d->udata2 = a;
	d->refresh_data = ses;

	d->items[0].type = D_FIELD;
	d->items[0].dlen = MAX_UID_LEN;
	d->items[0].data = a->uid;

	d->items[1].type = D_FIELD_PASS;
	d->items[1].dlen = MAX_PASSWD_LEN;
	d->items[1].data = a->passwd;

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
	if (a->uid[0] && !a->passwd[0])
		dd->selected = 1;
	a->blocked = 0;
}
