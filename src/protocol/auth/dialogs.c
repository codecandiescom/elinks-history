/* HTTP Auth dialog stuff */
/* $Id: dialogs.c,v 1.12 2002/12/07 09:37:19 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "links.h"

#include "bfu/dialog.h"
#include "bfu/button.h"
#include "bfu/inpfield.h"
#include "bfu/text.h"
#include "dialogs/auth.h"
#include "document/session.h"
#include "intl/language.h"
#include "lowlevel/terminal.h"
#include "protocol/http/auth.h"
#include "util/memory.h"


void
auth_layout(struct dialog_data *dlg)
{
        struct terminal *term = dlg->win->term;
        int max = 0, min = 0;
        int w, rw;
        int y = -1;
	int dialog_text_color = get_bfu_color(term, "dialog.text");

	max_text_width(term, TEXT(T_USERID), &max);
        min_text_width(term, TEXT(T_USERID), &min);
        max_text_width(term, TEXT(T_PASSWORD), &max);
        min_text_width(term, TEXT(T_PASSWORD), &min);
        max_buttons_width(term, dlg->items + 2, 2,  &max);
        min_buttons_width(term, dlg->items + 2, 2,  &min);
        w = dlg->win->term->x * 9 / 10 - 2 * DIALOG_LB;
        if (w < min) w = min;
        if (w > dlg->win->term->x - 2 * DIALOG_LB) w = dlg->win->term->x - 2 * DIALOG_LB;
        if (w < 1) w = 1;
        rw = 0;
        if (dlg->dlg->udata) {
                dlg_format_text(NULL, term,
				dlg->dlg->udata, 0, &y, w, &rw,
				dialog_text_color, AL_LEFT);
                y++;
        }

        dlg_format_text(NULL, term,
			TEXT(T_USERID), 0, &y, w, &rw,
			dialog_text_color, AL_LEFT);
        y += 2;
        dlg_format_text(NULL, term,
			TEXT(T_PASSWORD), 0, &y, w, &rw,
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
			TEXT(T_USERID), dlg->x + DIALOG_LB, &y, w, NULL,
			dialog_text_color, AL_LEFT);
        dlg_format_field(term, term,
			 &dlg->items[0],
			 dlg->x + DIALOG_LB, &y, w, NULL, AL_LEFT);
        y++;
        dlg_format_text(term, term,
			TEXT(T_PASSWORD), dlg->x + DIALOG_LB, &y, w, NULL,
			dialog_text_color, AL_LEFT);
        dlg_format_field(term, term,
			 &dlg->items[1],
			 dlg->x + DIALOG_LB, &y, w, NULL, AL_LEFT);
        y++;
        dlg_format_buttons(term, term,
			   &dlg->items[2], 2,
			   dlg->x + DIALOG_LB, &y, w, NULL, AL_CENTER);
}

int
auth_ok(struct dialog_data *dlg, struct widget_data *di)
{
        ((struct http_auth_basic *)dlg->dlg->udata2)->blocked = 0;
        reload(dlg->dlg->refresh_data, -1);
        return ok_dialog(dlg, di);
}

int
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
        /* TODO: complete rewrite */
        struct dialog *d;
	struct dialog_data *dd;
        struct terminal *term = ses->term;
        struct http_auth_basic *a = NULL;

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
        d = mem_alloc(sizeof(struct dialog) + 5 * sizeof(struct widget)
                      + strlen(_(TEXT(T_ENTER_USERNAME), term))
                      + (a->realm ? strlen(a->realm) : 0)
                      + strlen(_(TEXT(T_AT), term)) + strlen(a->url) + 1);
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
        memset(d, 0, sizeof(struct dialog) + 5 * sizeof(struct widget));
        d->title = TEXT(T_AUTHEN);
        d->fn = auth_layout;

        d->udata = (char *)d + sizeof(struct dialog) + 5 * sizeof(struct widget);
        strcpy(d->udata, _(TEXT(T_ENTER_USERNAME), term));
        if (a->realm) strcat(d->udata, a->realm);
        strcat(d->udata, _(TEXT(T_AT), term));
        strcat(d->udata, a->url);

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
        d->items[2].text = TEXT(T_OK);
        d->items[3].type = D_BUTTON;
        d->items[3].gid = B_ESC;
        d->items[3].fn = auth_cancel;
        d->items[3].text = TEXT(T_CANCEL);

        d->items[4].type = D_END;
	dd = do_dialog(term, d, getml(d, NULL));
	/* When there's some username, but no password, automagically jump at
	 * the password. */
	if (a->uid[0] && !a->passwd[0])
		dd->selected = 1;
        a->blocked = 0;
}
