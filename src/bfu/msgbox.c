/* Prefabricated message box implementation. */
/* $Id: msgbox.c,v 1.77 2003/11/06 22:02:51 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "bfu/button.h"
#include "bfu/msgbox.h"
#include "bfu/style.h"
#include "bfu/text.h"
#include "intl/gettext/libintl.h"
#include "terminal/terminal.h"
#include "util/color.h"
#include "util/memlist.h"
#include "util/memory.h"
#include "util/snprintf.h"
#include "util/string.h"


static void
msg_box_layouter(struct dialog_data *dlg_data)
{
	struct terminal *term = dlg_data->win->term;
	int max = 0, min = 0;
	int w = dialog_max_width(term);
	int rw = 0;
	int y = 0;
	unsigned char *text = dlg_data->dlg->udata;
	struct color_pair *text_color = get_bfu_color(term, "dialog.text");

	text_width(term, text, &min, &max);
	buttons_width(dlg_data->widgets_data, dlg_data->n, &min, &max);

	int_bounds(&w, min, max);
	int_bounds(&w, 1, term->width - 2 * DIALOG_LB);

	dlg_format_text(NULL, text, 0, &y, w, &rw, text_color,
			dlg_data->dlg->align);

	y++;
	dlg_format_buttons(NULL, dlg_data->widgets_data, dlg_data->n, 0, &y, w, &rw,
			   AL_CENTER);

	w = rw;

	draw_dialog(dlg_data, w, y, AL_CENTER);

	y = dlg_data->y + DIALOG_TB + 1;
	dlg_format_text(term, text, dlg_data->x + DIALOG_LB, &y, w, NULL,
			text_color, dlg_data->dlg->align);

	y++;
	dlg_format_buttons(term, dlg_data->widgets_data, dlg_data->n, dlg_data->x + DIALOG_LB,
			   &y, w, NULL, AL_CENTER);
}

static int
msg_box_button(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	void (*fn)(void *) = (void (*)(void *)) widget_data->widget->udata;
	void *data = dlg_data->dlg->udata2;

	if (fn) fn(data);
	cancel_dialog(dlg_data, widget_data);

	return 0;
}

void
msg_box(struct terminal *term, struct memory_list *ml, enum msgbox_flags flags,
	unsigned char *title, enum format_align align,
	unsigned char *text, void *udata, int buttons, ...)
{
	struct dialog *dlg;
	va_list ap;

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

	dlg = calloc_dialog(buttons, 0);
	if (!dlg) {
		freeml(ml);
		return;
	}

	add_one_to_ml(&ml, dlg);

	dlg->title = title;
	dlg->layouter = msg_box_layouter;
	dlg->udata = text;
	dlg->udata2 = udata;
	dlg->align = align;

	va_start(ap, buttons);

	while (dlg->widgets_size < buttons) {
		unsigned char *label;
		void (*fn)(void *);
		int bflags;

		label = va_arg(ap, unsigned char *);
		fn = va_arg(ap, void *);
		bflags = va_arg(ap, int);

		if (!label) {
			/* Skip this button. */
			buttons--;
			continue;
		}

		if (!(flags & MSGBOX_NO_INTL))
			label = _(label, term);

		add_dlg_button(dlg, bflags, msg_box_button, label, fn);
	}

	va_end(ap);

	add_dlg_end(dlg, buttons);

	do_dialog(term, dlg, ml);
}

/* Do not inline this function, because with inline
 * elinks segfaults on Cygwin */
static unsigned char *
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
