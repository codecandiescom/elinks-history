/* Options dialogs */
/* $Id: dialogs.c,v 1.10 2002/12/08 20:00:39 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/align.h"
#include "bfu/button.h"
#include "bfu/dialog.h"
#include "bfu/inpfield.h"
#include "bfu/listbox.h"
#include "bfu/msgbox.h"
#include "bfu/text.h"
#include "config/conf.h"
#include "config/dialogs.h"
#include "config/options.h"
#include "config/opttypes.h"
#include "dialogs/hierbox.h"
#include "document/session.h"
#include "intl/language.h"
#include "lowlevel/kbd.h"
#include "lowlevel/terminal.h"
#include "util/error.h"
#include "util/lists.h"
#include "util/memory.h"

/* The location of the box in the options manager */
#define	OP_BOX_IND		4


void
write_config_error(struct terminal *term, unsigned char *config_file, int ret)
{
	msg_box(term, NULL,
		TEXT(T_CONFIG_ERROR), AL_CENTER | AL_EXTD_TEXT,
		TEXT(T_UNABLE_TO_WRITE_TO_CONFIG_FILE), "\n",
		config_file, ": ", strerror(ret), NULL,
		NULL, 1,
		TEXT(T_CANCEL), NULL, B_ENTER | B_ESC);
}

/****************************************************************************
  Bookmark manager stuff.
****************************************************************************/

/* Creates the box display (holds everything EXCEPT the actual rendering
 * data) */
static struct listbox_data *
option_dlg_box_build()
{
	struct listbox_data *box;

	/* Deleted in abort */
	box = mem_calloc(1, sizeof(struct listbox_data));
	if (!box) return NULL;

	box->items = &root_option_box_items;
	add_to_list(option_boxes, box);

	return box;
}


/* Cleans up after the option dialog */
static void
option_dialog_abort_handler(struct dialog_data *dlg)
{
	struct listbox_data *box;

	box = (struct listbox_data *) dlg->dlg->items[OP_BOX_IND].data;

	del_from_list(box);
	/* Delete the box structure */
	mem_free(box);
}


static void
done_info_button(void *vhop)
{
#if 0
	struct option *option = vhop;

#endif
}

static int
push_info_button(struct dialog_data *dlg,
		struct widget_data *some_useless_info_button)
{
	struct terminal *term = dlg->win->term;
	struct option *option;
	struct listbox_data *box;

	box = (struct listbox_data *) dlg->dlg->items[OP_BOX_IND].data;

	/* Show history item info */
	if (!box->sel) return 0;
	option = box->sel->udata;
	if (!option) return 0;

	if (option_types[option->type].write) {
		unsigned char *value = init_str();
		int val_len = 0;

		option_types[option->type].write(option, &value, &val_len);

		msg_box(term, getml(value, NULL),
			TEXT(T_INFO), AL_LEFT | AL_EXTD_TEXT,
			TEXT(T_NNAME), ": ", option->name, "\n",
			TEXT(T_TYPE), ": ", option_types[option->type].name, "\n",
			TEXT(T_VALUE), ": ", value, "\n",
			TEXT(T_DESCRIPTION), ": ", option->desc, NULL,
			option, 1,
			TEXT(T_OK), done_info_button, B_ESC | B_ENTER);
	} else {
		msg_box(term, NULL,
			TEXT(T_INFO), AL_LEFT | AL_EXTD_TEXT,
			TEXT(T_NNAME), ": ", option->name, "\n",
			TEXT(T_TYPE), ": ", option_types[option->type].name, "\n",
			TEXT(T_DESCRIPTION), ": ", option->desc, NULL,
			option, 1,
			TEXT(T_OK), done_info_button, B_ESC | B_ENTER);
	}

	return 0;
}


int
check_valid_option(struct dialog_data *dlg, struct widget_data *di)
{
	struct terminal *term = dlg->win->term;
	struct option *option = dlg->dlg->udata;
	unsigned char *value = di->cdata;
	unsigned char *chinon;

	commandline = 1;
	chinon = option_types[option->type].read(option, &value);
	if (chinon) {
		if (option_types[option->type].set &&
		    option_types[option->type].set(option, chinon)) {
			commandline = 0;
			mem_free(chinon);
			return 0;
		}
		mem_free(chinon);
	}
	commandline = 0;

	msg_box(term, NULL,
		TEXT(T_ERROR), AL_LEFT,
		TEXT(T_BAD_OPTION_VALUE),
		NULL, 1,
		TEXT(T_CANCEL), NULL, B_ESC | B_ENTER);
	return 1;
}

static void
layout_edit_dialog(struct dialog_data *dlg)
{
	int max = 0, min = 0;
	int w, rw;
	int y = -1;
	struct terminal *term = dlg->win->term;
	int dialog_text_color = get_bfu_color(term, "dialog.text");
	struct option *option = dlg->dlg->udata;
	unsigned char *name, *type, *value, *desc;

	/* TODO: Memory allocation errors handling; any good idea what exactly
	 * should we do? --pasky */
	name = straconcat(_(TEXT(T_INFO), term), ": ", option->name, NULL);
	type = straconcat(_(TEXT(T_TYPE), term), ": ", option_types[option->type].name, NULL);
	value= straconcat(_(TEXT(T_VALUE), term), ": ", NULL);
	desc = straconcat(_(TEXT(T_DESCRIPTION), term), ": ", option->desc, NULL);
	add_to_ml(&dlg->ml, name, type, value, desc, NULL);

	max_text_width(term, name, &max);
	min_text_width(term, name, &min);
	max_text_width(term, type, &max);
	min_text_width(term, type, &min);
	max_text_width(term, value, &max);
	min_text_width(term, value, &min);
	max_text_width(term, desc, &max);
	min_text_width(term, desc, &min);
	max_buttons_width(term, dlg->items + 1, 2, &max);
	min_buttons_width(term, dlg->items + 1, 2, &min);
	w = term->x * 9 / 10 - 2 * DIALOG_LB;

	if (w > max) w = max;
	if (w < min) w = min;
	if (w > term->x - 2 * DIALOG_LB) w = term->x - 2 * DIALOG_LB;
	if (w < 1) w = 1;

	w = rw = 50;

	dlg_format_text(NULL, term, name, 0, &y,
			w, &rw, dialog_text_color, AL_LEFT);
	dlg_format_text(NULL, term, type, 0, &y,
			w, &rw, dialog_text_color, AL_LEFT);
	dlg_format_text(NULL, term, value, 0, &y,
			w, &rw, dialog_text_color, AL_LEFT);
	y++;
	dlg_format_text(NULL, term, desc, 0, &y,
			w, &rw, dialog_text_color, AL_LEFT);
	y++;
	dlg_format_buttons(NULL, term, dlg->items + 1, 2, 0,
			   &y, w, &rw, AL_CENTER);
	w = rw;
	dlg->xw = w + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);
	draw_dlg(dlg);
	y = dlg->y + DIALOG_TB;
	dlg_format_text(term, term, name, dlg->x + DIALOG_LB,
			&y, w, NULL, dialog_text_color, AL_LEFT);
	dlg_format_text(term, term, type, dlg->x + DIALOG_LB,
			&y, w, NULL, dialog_text_color, AL_LEFT);

	/* XXX: We want the field on the same line. Could this be a problem
	 * with extremely thin terminals? --pasky */
	rw = 0;
	dlg_format_text(term, term, value, dlg->x + DIALOG_LB,
			&y, w, &rw, dialog_text_color, AL_LEFT);
	y--;
	dlg_format_field(NULL, term, &dlg->items[0], rw + dlg->x + DIALOG_LB,
			 &y, w - rw, NULL, AL_LEFT);

	y++;
	dlg_format_text(term, term, desc, dlg->x + DIALOG_LB,
			&y, w, NULL, dialog_text_color, AL_LEFT);
	y++;
	dlg_format_buttons(term, term, &dlg->items[1], 2, dlg->x + DIALOG_LB,
			   &y, w, NULL, AL_CENTER);
}

static void
build_edit_dialog(struct terminal *term, struct option *option)
{
#define EDIT_DIALOG_FIELDS_NB 3
	struct dialog *d;
	unsigned char *value;
	unsigned char *tvalue = init_str();
	int tval_len = 0;

	if (!tvalue) return;

	commandline = 1;
	option_types[option->type].write(option, &tvalue, &tval_len);
	commandline = 0;

	/* Create the dialog */
	d = mem_calloc(1, sizeof(struct dialog)
			  + (EDIT_DIALOG_FIELDS_NB + 1)
			    * sizeof(struct widget)
			  + MAX_STR_LEN);
	if (!d) {
		mem_free(tvalue);
		return;
	}

	d->title = TEXT(T_EDIT);
	d->fn = layout_edit_dialog;
	d->udata = option;

	value = (unsigned char *) &d->items[EDIT_DIALOG_FIELDS_NB + 1];
	safe_strncpy(value, tvalue, MAX_STR_LEN);
	mem_free(tvalue);

	/* FIXME: Compute some meaningful maximal width. --pasky */
	d->items[0].type = D_FIELD;
	d->items[0].dlen = MAX_STR_LEN;
	d->items[0].data = value;
	d->items[0].fn = check_valid_option;

	d->items[1].type = D_BUTTON;
	d->items[1].gid = B_ENTER;
	d->items[1].fn = ok_dialog;
	d->items[1].text = TEXT(T_OK);

	d->items[2].type = D_BUTTON;
	d->items[2].gid = B_ESC;
	d->items[2].text = TEXT(T_CANCEL);
	d->items[2].fn = cancel_dialog;

	d->items[EDIT_DIALOG_FIELDS_NB].type = D_END;

	do_dialog(term, d, getml(d, NULL));
#undef EDIT_DIALOG_FIELDS_NB
}

static int
push_edit_button(struct dialog_data *dlg,
		struct widget_data *some_useless_info_button)
{
	struct terminal *term = dlg->win->term;
	struct option *option;
	struct listbox_data *box;

	box = (struct listbox_data *) dlg->dlg->items[OP_BOX_IND].data;

	/* Show history item info */
	if (!box->sel) return 0;
	option = box->sel->udata;
	if (!option) return 0;

	if (!option_types[option->type].write ||
	    !option_types[option->type].read ||
	    !option_types[option->type].set) {
		msg_box(term, NULL,
			TEXT(T_EDIT), AL_LEFT,
			TEXT(T_CANNOT_EDIT_OPTION),
			NULL, 1,
			TEXT(T_CANCEL), NULL, B_ESC | B_ENTER);
		return 0;
	}

	build_edit_dialog(term, option);

	return 0;
}


static int
push_save_button(struct dialog_data *dlg,
		struct widget_data *some_useless_info_button)
{
	write_config(dlg->win->term);
	return 0;
}


/* Builds the "Options manager" dialog */
void
menu_options_manager(struct terminal *term, void *fcp, struct session *ses)
{
	struct dialog *d;

	/* Create the dialog */
	d = mem_calloc(1, sizeof(struct dialog)
			  + (OP_BOX_IND + 2) * sizeof(struct widget)
			  + sizeof(struct option) + 2 * MAX_STR_LEN);
	if (!d) return;

	d->title = TEXT(T_OPTIONS_MANAGER);
	d->fn = layout_hierbox_browser;
	d->handle_event = hierbox_dialog_event_handler;
	d->abort = option_dialog_abort_handler;
	d->udata = ses;

	d->items[0].type = D_BUTTON;
	d->items[0].gid = B_ENTER;
	d->items[0].fn = push_info_button;
	d->items[0].udata = ses;
	d->items[0].text = TEXT(T_INFO);

	d->items[1].type = D_BUTTON;
	d->items[1].gid = B_ENTER;
	d->items[1].fn = push_edit_button;
	d->items[1].udata = ses;
	d->items[1].text = TEXT(T_EDIT);

	d->items[2].type = D_BUTTON;
	d->items[2].gid = B_ENTER;
	d->items[2].fn = push_save_button;
	d->items[2].udata = ses;
	d->items[2].text = TEXT(T_SAVE);

	d->items[3].type = D_BUTTON;
	d->items[3].gid = B_ESC;
	d->items[3].fn = cancel_dialog;
	d->items[3].text = TEXT(T_CLOSE);

	d->items[OP_BOX_IND].type = D_BOX;
	d->items[OP_BOX_IND].gid = 12;
	d->items[OP_BOX_IND].data = (void *) option_dlg_box_build();

	d->items[OP_BOX_IND + 1].type = D_END;
	do_dialog(term, d, getml(d, NULL));
}
