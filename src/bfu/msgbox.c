/* Prefabricated message box implementation. */
/* $Id: msgbox.c,v 1.46 2003/06/27 20:42:37 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>

#include "elinks.h"

#include "bfu/align.h"
#include "bfu/dialog.h"
#include "bfu/button.h"
#include "bfu/msgbox.h"
#include "bfu/text.h"
#include "intl/gettext/libintl.h"
#include "terminal/terminal.h"
#include "util/memlist.h"
#include "util/memory.h"
#include "util/snprintf.h"
#include "util/string.h"


static void
msg_box_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = 0;
	unsigned char *text = dlg->dlg->udata;
	int dialog_text_color = get_bfu_color(term, "dialog.text");

	text_width(term, text, &min, &max);
	buttons_width(term, dlg->items, dlg->n, &min, &max);

	w = term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max) w = max;
	if (w < min) w = min;
	if (w > term->x - 2 * DIALOG_LB) w = term->x - 2 * DIALOG_LB;
	if (w < 1) w = 1;

	rw = 0;
	dlg_format_text(NULL, term, text, 0, &y, w, &rw, dialog_text_color,
			dlg->dlg->align);

	y++;
	dlg_format_buttons(NULL, term, dlg->items, dlg->n, 0, &y, w, &rw,
			   AL_CENTER);

	w = rw;
	dlg->xw = rw + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);

	draw_dlg(dlg);

	y = dlg->y + DIALOG_TB + 1;
	dlg_format_text(term, term, text, dlg->x + DIALOG_LB, &y, w, NULL,
			dialog_text_color, dlg->dlg->align);

	y++;
	dlg_format_buttons(term, term, dlg->items, dlg->n, dlg->x + DIALOG_LB,
			   &y, w, NULL, AL_CENTER);
}

static int
msg_box_button(struct dialog_data *dlg, struct widget_data *di)
{
	void (*fn)(void *) = (void (*)(void *)) di->item->udata;
	void *data = dlg->dlg->udata2;

	if (fn) fn(data);
	cancel_dialog(dlg, di);

	return 0;
}

void
msg_box(struct terminal *term, struct memory_list *ml, enum msgbox_flags flags,
	unsigned char *title, enum format_align align,
	unsigned char *text, void *udata, int buttons, ...)
{
	struct dialog *dlg;
	va_list ap;
	int button;

	/* Check if the info string is valid. */
	if (!text || buttons < 0) return;

	/* Use the @flags to determine whether @text should be free()d. */
	if (flags & MSGBOX_FREE_TEXT)
		add_one_to_ml(&ml, text);

	/* Use the @flags to determine whether strings should be l18n'd. */
	if (!(flags & MSGBOX_NO_INTL)) {
		title = _(title, term);
		if (!(flags & MSGBOX_FREE_TEXT)) text = _(text, term);
		/* Button labels will be gettextized as will they be extracted
		 * from @ap. */
	}

	dlg = mem_calloc(1, sizeof(struct dialog) +
			    (buttons + 1) * sizeof(struct widget));
	if (!dlg) {
		freeml(ml);
		return;
	}

	add_one_to_ml(&ml, dlg);

	dlg->title = title;
	dlg->fn = msg_box_fn;
	dlg->udata = text;
	dlg->udata2 = udata;
	dlg->align = align;

	va_start(ap, buttons);

	for (button = 0; button < buttons; button++) {
		unsigned char *label;
		void (*fn)(void *);
		int button_flags;

		label = va_arg(ap, unsigned char *);
		fn = va_arg(ap, void *);
		button_flags = va_arg(ap, int);

		if (!label) {
			/* Skip this button. */
			button--;
			buttons--;
			continue;
		}

		if (!(flags & MSGBOX_NO_INTL))
			label = _(label, term);

		dlg->items[button].type = D_BUTTON;
		dlg->items[button].gid = button_flags;
		dlg->items[button].fn = msg_box_button;
		dlg->items[button].dlen = 0;
		dlg->items[button].text = label;
		dlg->items[button].udata = fn;
	}

	va_end(ap);

	dlg->items[button].type = D_END;

	do_dialog(term, dlg, ml);
}


static inline unsigned char *
msg_text_do(unsigned char *format, va_list ap)
{
	unsigned char *info;
	int infolen;
	va_list ap2;

	VA_COPY(ap2, ap);

	infolen = vsnprintf(NULL, 0, format, ap2);
	info = mem_alloc(infolen + 1);
	if (info) {
		if (vsnprintf((char *) info, infolen + 1, format, ap) != infolen) {
			mem_free(info);
			info = NULL;
		} else {
			/* Wear safety boots */
			info[infolen] = '\0';
		}
	}

	return info;
}

unsigned char *
msg_text_ni(unsigned char *format, ...)
{
	unsigned char *info;
	va_list ap;

	va_start(ap, format);
	info = msg_text_do(format, ap);
	va_end(ap);

	return info;
}

unsigned char *
msg_text(struct terminal *term, unsigned char *format, ...)
{
	unsigned char *info;
	va_list ap;

	va_start(ap, format);
	info = msg_text_do(_(format, term), ap);
	va_end(ap);

	return info;
}
