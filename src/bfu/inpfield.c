/* Input field widget implementation. */
/* $Id: inpfield.c,v 1.4 2002/07/05 20:42:13 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "links.h"

#include "bfu/align.h"
#include "bfu/colors.h"
#include "bfu/dialog.h"
#include "bfu/button.h"
#include "bfu/inpfield.h"
#include "bfu/inphist.h"
#include "bfu/msgbox.h"
#include "bfu/text.h"
#include "intl/language.h"
#include "lowlevel/terminal.h"
#include "util/memlist.h"
#include "util/memory.h"


/* check_number() */
int check_number(struct dialog_data *dlg, struct widget_data *di)
{
	unsigned char *end;
	long l = strtol(di->cdata, (char **)&end, 10);

	if (!*di->cdata || *end) {
		msg_box(dlg->win->term, NULL,
			TEXT(T_BAD_NUMBER), AL_CENTER,
			TEXT(T_NUMBER_EXPECTED),
			NULL, 1,
			TEXT(T_CANCEL),	NULL, B_ENTER | B_ESC);
		return 1;
	}

	if (l < di->item->gid || l > di->item->gnum) {
		msg_box(dlg->win->term, NULL,
			TEXT(T_BAD_NUMBER), AL_CENTER,
			TEXT(T_NUMBER_OUT_OF_RANGE),
			NULL, 1,
			TEXT(T_CANCEL),	NULL, B_ENTER | B_ESC);
		return 1;
	}

	return 0;
}

/* check_nonempty() */
int check_nonempty(struct dialog_data *dlg, struct widget_data *di)
{
	unsigned char *p;

	for (p = di->cdata; *p; p++)
		if (*p > ' ')
			return 0;

	msg_box(dlg->win->term, NULL,
		TEXT(T_BAD_STRING), AL_CENTER,
		TEXT(T_EMPTY_STRING_NOT_ALLOWED),
		NULL, 1,
		TEXT(T_CANCEL),	NULL, B_ENTER | B_ESC);

	return 1;
}


/* dlg_format_field() */
void dlg_format_field(struct terminal *term, struct terminal *t2,
		      struct widget_data *item,
		      int x, int *y, int w, int *rw, enum format_align align)
{
	item->x = x;
	item->y = *y;
	item->l = w;

	if (rw && item->l > *rw) {
		*rw = item->l;
		if (*rw > w) *rw = w;
	}
	(*y)++;
}


/* input_field_cancel() */
int input_field_cancel(struct dialog_data *dlg, struct widget_data *di)
{
	void (*fn)(void *) = di->item->udata;
	void *data = dlg->dlg->udata2;

	if (fn) fn(data);
	cancel_dialog(dlg, di);

	return 0;
}

/* input_field_ok() */
int input_field_ok(struct dialog_data *dlg, struct widget_data *di)
{
	void (*fn)(void *, unsigned char *) = di->item->udata;
	void *data = dlg->dlg->udata2;
	unsigned char *text = dlg->items->cdata;

	if (check_dialog(dlg)) return 1;

	add_to_input_history(dlg->dlg->items->history, text, 1);

	if (fn) fn(data, text);
	ok_dialog(dlg, di);
	return 0;
}

/* input_field_fn() */
void input_field_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = -1;

	max_text_width(term, dlg->dlg->udata, &max);
	min_text_width(term, dlg->dlg->udata, &min);
	max_buttons_width(term, dlg->items + 1, 2, &max);
	min_buttons_width(term, dlg->items + 1, 2, &min);

	if (max < dlg->dlg->items->dlen) max = dlg->dlg->items->dlen;

	w = term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max) w = max;
	if (w < min) w = min;

	rw = 0; /* !!! FIXME: input field */
	dlg_format_text(NULL, term, dlg->dlg->udata, 0, &y, w, &rw,
			COLOR_DIALOG_TEXT, AL_LEFT);
	dlg_format_field(NULL, term, dlg->items, 0, &y, w, &rw,
			 AL_LEFT);

	y++;
	dlg_format_buttons(NULL, term, dlg->items + 1, 2, 0, &y, w, &rw,
			   AL_CENTER);

	w = rw;
	dlg->xw = rw + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);

	draw_dlg(dlg);

	y = dlg->y + DIALOG_TB;
	dlg_format_text(term, term, dlg->dlg->udata, dlg->x + DIALOG_LB,
			&y, w, NULL, COLOR_DIALOG_TEXT, AL_LEFT);
	dlg_format_field(term, term, dlg->items, dlg->x + DIALOG_LB,
			 &y, w, NULL, AL_LEFT);

	y++;
	dlg_format_buttons(term, term, dlg->items + 1, 2, dlg->x + DIALOG_LB,
			   &y, w, NULL, AL_CENTER);
}

/* input_field() */
void input_field(struct terminal *term, struct memory_list *ml,
		 unsigned char *title,
		 unsigned char *text,
		 unsigned char *okbutton,
		 unsigned char *cancelbutton,
		 void *data, struct input_history *history, int l,
		 unsigned char *def, int min, int max,
		 int (*check)(struct dialog_data *, struct widget_data *),
		 void (*fn)(void *, unsigned char *),
		 void (*cancelfn)(void *))
{
	struct dialog *dlg;
	unsigned char *field;

#define SIZEOF_DIALOG (sizeof(struct dialog) + 4 * sizeof(struct widget))

	dlg = mem_alloc(SIZEOF_DIALOG + l);
	if (!dlg) return;

	memset(dlg, 0, SIZEOF_DIALOG + l);
	field = (unsigned char *) dlg + SIZEOF_DIALOG;
	*field = 0;

#undef SIZEOF_DIALOG

	if (def) {
		if (strlen(def) + 1 > l)
			memcpy(field, def, l - 1);
		else
			strcpy(field, def);
	}

	dlg->title = title;
	dlg->fn = input_field_fn;
	dlg->udata = text;
	dlg->udata2 = data;

	dlg->items[0].type = D_FIELD;
	dlg->items[0].gid = min;
	dlg->items[0].gnum = max;
	dlg->items[0].fn = check;
	dlg->items[0].history = history;
	dlg->items[0].dlen = l;
	dlg->items[0].data = field;

	dlg->items[1].type = D_BUTTON;
	dlg->items[1].gid = B_ENTER;
	dlg->items[1].fn = input_field_ok;
	dlg->items[1].dlen = 0;
	dlg->items[1].text = okbutton;
	dlg->items[1].udata = fn;

	dlg->items[2].type = D_BUTTON;
	dlg->items[2].gid = B_ESC;
	dlg->items[2].fn = input_field_cancel;
	dlg->items[2].dlen = 0;
	dlg->items[2].text = cancelbutton;
	dlg->items[2].udata = cancelfn;

	dlg->items[3].type = D_END;

	add_to_ml(&ml, dlg, NULL);
	do_dialog(term, dlg, ml);
}


static inline void
display_field_do(struct widget_data *di, struct dialog_data *dlg, int sel,
		 int hide)
{
	struct terminal *term = dlg->win->term;

	if (di->vpos + di->l <= di->cpos)
		di->vpos = di->cpos - di->l + 1;
	if (di->vpos > di->cpos)
		di->vpos = di->cpos;
	if (di->vpos < 0)
		di->vpos = 0;

	fill_area(term, di->x, di->y, di->l, 1,
			COLOR_DIALOG_FIELD);
	{
		int len = strlen(di->cdata + di->vpos);

		if (!hide) {
			print_text(term, di->x, di->y,
					len <= di->l ? len : di->l,
					di->cdata + di->vpos,
					COLOR_DIALOG_FIELD_TEXT);
		} else {
			fill_area(term, di->x, di->y,
					len <= di->l ? len : di->l, 1,
					COLOR_DIALOG_FIELD_TEXT | '*');
		}
	}
	if (sel) {
		int x = di->x + di->cpos - di->vpos;

		set_cursor(term, x, di->y, x, di->y);
		set_window_ptr(dlg->win, di->x, di->y);
	}
}

void
display_field(struct widget_data *di, struct dialog_data *dlg, int sel)
{
	display_field_do(di, dlg, sel, 0);
}

void
display_field_pass(struct widget_data *di, struct dialog_data *dlg, int sel)
{
	display_field_do(di, dlg, sel, 1);
}

struct widget_ops field_ops = {
	display_field,
};

struct widget_ops field_pass_ops = {
	display_field_pass,
};
