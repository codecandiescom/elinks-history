/* Prefabricated message box implementation. */
/* $Id: msgbox.c,v 1.25 2003/06/07 00:33:33 jonas Exp $ */

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
#include "util/string.h"


static void
msg_box_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = 0;
	unsigned char **ptr;
	unsigned char *text = init_str();
	int textl = 0;
	int dialog_text_color = get_bfu_color(term, "dialog.text");

	if (!text) return;

	for (ptr = dlg->dlg->udata; *ptr; ptr++)
		add_to_str(&text, &textl, _(*ptr, term));

	max_text_width(term, text, &max);
	min_text_width(term, text, &min);
	max_buttons_width(term, dlg->items, dlg->n, &max);
	min_buttons_width(term, dlg->items, dlg->n, &min);

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

	mem_free(text);
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

/* The '...' means:
 *
 * ( text1, [text2, ..., textN, NULL,]
 *   udata, M,
 *   label1, handler1, flags1,
 *   ...,
 *   labelM, handlerM, flagsM )
 *
 * If !(align & AL_EXTD_TEXT), only one text is accepted, if you'll give it
 * AL_EXTD_TEXT, more texts are accepted, terminated by NULL.
 *
 * When labelX == NULL, the entire record is skipped.
 *
 * Handler takes one (void *), and udata is passed as it.
 *
 * You should always align it in a similiar way. */
void
msg_box(struct terminal *term, struct memory_list *ml,
	unsigned char *title, enum format_align align,
	...)
{
	unsigned char **info = NULL;
	int button;
	int buttons;
	struct dialog *dlg;
	void *udata;
	va_list ap;

	va_start(ap, align);

	if (align & AL_EXTD_TEXT) {
		unsigned char *text = "";
		int info_n = 0;

		while (text) {
			text = va_arg(ap, unsigned char *);

			info = mem_realloc(info, (info_n + 1)
						 * sizeof(unsigned char *));
			if (!info) {
				va_end(ap);
				if (ml) freeml(ml);
				return;
			}

			info[info_n] = text;
			info_n++;
		}

	} else {
		/* I had to decide between evil gotos and code duplication. */
		unsigned char *text = va_arg(ap, unsigned char *);
		unsigned char **info_;

		info_ = mem_realloc(info, 2 * sizeof(unsigned char *));
		if (!info_) {
			va_end(ap);
			if (ml) freeml(ml);
			return;
	   	}

		info = info_;
		info[0] = text;
		info[1] = NULL;
	}

	udata = va_arg(ap, void *);
	buttons = va_arg(ap, int);

	dlg = mem_calloc(1, sizeof(struct dialog) +
			    (buttons + 1) * sizeof(struct widget));
	if (!dlg) {
		va_end(ap);
		mem_free(info);
		if (ml) freeml(ml);
		return;
	}

	dlg->title = title;
	dlg->fn = msg_box_fn;
	dlg->udata = info;
	dlg->udata2 = udata;
	dlg->align = align;

	for (button = 0; button < buttons; button++) {
		unsigned char *label;
		void (*fn)(void *);
		int flags;

		label = va_arg(ap, unsigned char *);
		fn = va_arg(ap, void *);
		flags = va_arg(ap, int);

		if (!label) {
			/* Skip this button. */
			button--;
			buttons--;
			continue;
		}

		dlg->items[button].type = D_BUTTON;
		dlg->items[button].gid = flags;
		dlg->items[button].fn = msg_box_button;
		dlg->items[button].dlen = 0;
		dlg->items[button].text = label;
		dlg->items[button].udata = fn;
	}

	va_end(ap);

	dlg->items[button].type = D_END;
	add_to_ml(&ml, dlg, info, NULL);
	do_dialog(term, dlg, ml);
}

#if 0

void
msg_box(struct terminal *term, struct memory_list *ml,
	unsigned char *title, enum format_align align,
	unsigned char *text, void *udata, int buttons, ...)
{
	unsigned char **info = NULL;
	int button;
	struct dialog *dlg;
	va_list ap;

	/* Check if the info string is valid */
	if (!text) return;

	/* What's up with this hack? Allocate a pointer to a pointer ;) */
	info = mem_alloc(2 * sizeof(unsigned char *));
	if (!info) {
		freeml(ml);
		if (align & AL_EXTD_TEXT)
			mem_free(text);
		return;
	}

	info[0] = text;
	info[1] = NULL;

	dlg = mem_calloc(1, sizeof(struct dialog) +
			    (buttons + 1) * sizeof(struct widget));
	if (!dlg) {
		mem_free(info);
		freeml(ml);
		if (align & AL_EXTD_TEXT)
			mem_free(text);
		return;
	}

	dlg->title = title;
	dlg->fn = msg_box_fn;
	dlg->udata = info;
	dlg->udata2 = udata;
	dlg->align = align;

	va_start(ap, buttons);

	for (button = 0; button < buttons; button++) {
		unsigned char *label;
		void (*fn)(void *);
		int flags;

		label = va_arg(ap, unsigned char *);
		fn = va_arg(ap, void *);
		flags = va_arg(ap, int);

		if (!label) {
			/* Skip this button. */
			button--;
			buttons--;
			continue;
		}

		dlg->items[button].type = D_BUTTON;
		dlg->items[button].gid = flags;
		dlg->items[button].fn = msg_box_button;
		dlg->items[button].dlen = 0;
		dlg->items[button].text = label;
		dlg->items[button].udata = fn;
	}

	va_end(ap);

	dlg->items[button].type = D_END;

	/* Use the align string to determine wether @text should be free()d */
	if (align & AL_EXTD_TEXT)
		add_to_ml(&ml, dlg, text, info, NULL);
	else
		add_to_ml(&ml, dlg, info, NULL);

	do_dialog(term, dlg, ml);
}

/* Copied from util/snprint.c */
#ifndef VA_COPY
#ifdef HAVE_VA_COPY
#define VA_COPY(dest, src) __va_copy(dest, src)
#else
#define VA_COPY(dest, src) (dest) = (src)
#endif
#endif

unsigned char *
msg_text(unsigned char *format, ...)
{
	unsigned char *info;
	int infolen;
	va_list ap;
	va_list ap2;

	va_start(ap, format);
	VA_COPY(ap2, ap);

	infolen = vsnprintf(NULL, 0, format, ap2);
	info = mem_alloc(infolen + 1);

	if (vsnprintf((char *)info, infolen, format, ap) != infolen) {
		mem_free(info);
		info = NULL;
	} else {
		/* Wear safety boots */
		info[infolen - 1] = '\0';
	}

	va_end(ap);

	return info;
}

#endif
