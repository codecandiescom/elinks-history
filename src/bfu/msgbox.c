/* Prefabricated message box implementation. */
/* $Id: msgbox.c,v 1.85 2003/11/28 00:17:34 jonas Exp $ */

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


struct dialog_data *
msg_box(struct terminal *term, struct memory_list *ml, enum msgbox_flags flags,
	unsigned char *title, enum format_align align,
	unsigned char *text, void *udata, int buttons, ...)
{
	struct dialog *dlg;
	va_list ap;

	/* Check if the info string is valid. */
	if (!text || buttons < 0) return NULL;

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

	dlg = calloc_dialog(buttons + 1, 0);
	if (!dlg) {
		freeml(ml);
		return NULL;
	}

	add_one_to_ml(&ml, dlg);

	dlg->title = title;
	dlg->layouter = generic_dialog_layouter;
	dlg->layout.padding_top = 1;
	dlg->udata2 = udata;

	add_dlg_text(dlg, text, align, 0);

	va_start(ap, buttons);

	while (dlg->widgets_size < buttons + 1) {
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

		add_dlg_ok_button(dlg, bflags, label, fn, udata);
	}

	va_end(ap);

	add_dlg_end(dlg, buttons + 1);

	return do_dialog(term, dlg, ml);
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

static void
abort_refreshed_msg_box_handler(struct dialog_data *dlg_data)
{
	if (dlg_data->dlg->udata != dlg_data->dlg->widgets->text)
		mem_free(dlg_data->dlg->widgets->text);
}

static enum dlg_refresh_code
refresh_msg_box(struct dialog_data *dlg_data, void *data)
{
	unsigned char *(*get_info)(struct terminal *, void *) = data;
	void *msg_data = dlg_data->dlg->udata2;
	unsigned char *info = get_info(dlg_data->win->term, msg_data);

	if (!info) return REFRESH_CANCEL;

	abort_refreshed_msg_box_handler(dlg_data);

	dlg_data->dlg->widgets->text = info;
	return REFRESH_DIALOG;
}

void
refreshed_msg_box(struct terminal *term, enum msgbox_flags flags,
		  unsigned char *title, enum format_align align,
		  unsigned char *(get_info)(struct terminal *, void *),
		  void *data)
{
	struct dialog_data *dlg_data;
	unsigned char *info = get_info(term, data);

	if (!info) return;

	dlg_data = msg_box(term, NULL, flags | MSGBOX_FREE_TEXT,
			   title, align,
			   info,
			   data, 1,
			   N_("OK"), NULL, B_ENTER | B_ESC);

	if (dlg_data) {
		/* Save the original text to check up on it when the dialog
		 * is freed. */
		dlg_data->dlg->udata = info;
		dlg_data->dlg->abort = abort_refreshed_msg_box_handler;
		refresh_dialog(dlg_data, refresh_msg_box, get_info);
	}
}
