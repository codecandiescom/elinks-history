/* Options dialogs */
/* $Id: dialogs.c,v 1.31 2003/01/02 23:59:52 pasky Exp $ */

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
#include "config/kbdbind.h"
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


void
write_config_error(struct terminal *term, unsigned char *config_file, int ret)
{
	msg_box(term, NULL,
		N_(T_CONFIG_ERROR), AL_CENTER | AL_EXTD_TEXT,
		N_(T_UNABLE_TO_WRITE_TO_CONFIG_FILE), "\n",
		config_file, ": ", ret == 2222 ? "Secure open failed" : strerror(ret), NULL,
		NULL, 1,
		N_(T_CANCEL), NULL, B_ENTER | B_ESC);
}



/****************************************************************************
  Option manager stuff.
****************************************************************************/

/* The location of the box in the options manager */
#define	OP_BOX_IND		6

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
	if (!box->sel || !box->sel->udata) return 0;
	option = box->sel->udata;

	if (option_types[option->type].write) {
		unsigned char *value = init_str();
		int val_len = 0;

		if (!value) return 0;

		option_types[option->type].write(option, &value, &val_len);

		msg_box(term, getml(value, NULL),
			N_(T_INFO), AL_LEFT | AL_EXTD_TEXT,
			N_(T_NNAME), ": ", option->name, "\n",
			N_(T_TYPE), ": ", option_types[option->type].name, "\n",
			N_(T_VALUE), ": ", value, "\n\n",
			N_(T_DESCRIPTION), ": \n", option->desc, NULL,
			option, 1,
			N_(T_OK), done_info_button, B_ESC | B_ENTER);
	} else {
		msg_box(term, NULL,
			N_(T_INFO), AL_LEFT | AL_EXTD_TEXT,
			N_(T_NNAME), ": ", option->name, "\n",
			N_(T_TYPE), ": ", option_types[option->type].name, "\n\n",
			N_(T_DESCRIPTION), ": \n", option->desc, NULL,
			option, 1,
			N_(T_OK), done_info_button, B_ESC | B_ENTER);
	}

	return 0;
}


static int
check_valid_option(struct dialog_data *dlg, struct widget_data *di)
{
	struct terminal *term = dlg->win->term;
	struct option *option = dlg->dlg->udata;
	struct session *ses = dlg->dlg->udata2;
	unsigned char *value = di->cdata;
	unsigned char *chinon;

	commandline = 1;
	chinon = option_types[option->type].read(option, &value);
	if (chinon) {
		if (option_types[option->type].set &&
		    option_types[option->type].set(option, chinon)) {
			struct option *current = option;

			option->flags |= OPT_TOUCHED;

			/* Notify everyone out there! */

			/* This boolean thing can look a little weird - it
			 * basically says that we should proceed when there's
			 * no change_hook or there's one and its return value
			 * was zero. */
			while (current && (!current->change_hook ||
				!current->change_hook(ses, current, option))) {
				if (current->box_item &&
				    current->box_item->root)
					current = current->box_item->root->udata;
				else
					break;
			}

			commandline = 0;
			mem_free(chinon);
			return 0;
		}
		mem_free(chinon);
	}
	commandline = 0;

	msg_box(term, NULL,
		N_(T_ERROR), AL_LEFT,
		N_(T_BAD_OPTION_VALUE),
		NULL, 1,
		N_(T_CANCEL), NULL, B_ESC | B_ENTER);
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

	name = straconcat(GT(N_(T_NNAME), term), ": ", option->name, NULL);
	type = straconcat(GT(N_(T_TYPE), term), ": ",
			  GT(option_types[option->type].name, term), NULL);
	value= straconcat(GT(N_(T_VALUE), term), ": ", NULL);
	desc = straconcat(GT(N_(T_DESCRIPTION), term), ": \n", option->desc,
			  NULL);

	if (name && type && value && desc)
		add_to_ml(&dlg->ml, name, type, value, desc, NULL);
	else {
		if (name) mem_free(name);
		if (type) mem_free(type);
		if (value) mem_free(value);
		if (desc) mem_free(desc);
		return;
	}

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

	rw = 0;

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
build_edit_dialog(struct terminal *term, struct session *ses,
		  struct option *option)
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

	d->title = N_(T_EDIT);
	d->fn = layout_edit_dialog;
	d->udata = option;
	d->udata2 = ses;

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
	d->items[1].text = N_(T_OK);

	d->items[2].type = D_BUTTON;
	d->items[2].gid = B_ESC;
	d->items[2].text = N_(T_CANCEL);
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
	if (!box->sel || !box->sel->udata) return 0;
	option = box->sel->udata;

	if (!option_types[option->type].write ||
	    !option_types[option->type].read ||
	    !option_types[option->type].set) {
		msg_box(term, NULL,
			N_(T_EDIT), AL_LEFT,
			N_(T_CANNOT_EDIT_OPTION),
			NULL, 1,
			N_(T_CANCEL), NULL, B_ESC | B_ENTER);
		return 0;
	}

	build_edit_dialog(term, dlg->dlg->udata, option);

	return 0;
}


static void
add_option_to_tree(void *data, unsigned char *name)
{
	struct option *option = data;

	/* get_opt_rec() will do all the work for ourselves... ;-) */
	get_opt_rec(option, name);
	/* TODO: If the return value is NULL, we should pop up a msgbox. */
}

static int
push_add_button(struct dialog_data *dlg,
		struct widget_data *some_useless_info_button)
{
	struct terminal *term = dlg->win->term;
	struct listbox_data *box = (void *) dlg->dlg->items[OP_BOX_IND].data;
	struct option *option;

	if (!box->sel || !box->sel->udata) {

invalid_option:
		msg_box(term, NULL,
			N_(T_ADD_OPTION), AL_CENTER,
			N_(T_CANNOT_ADD_OPTION_HERE),
			NULL, 1,
			N_(T_CANCEL), NULL, B_ESC | B_ENTER);
		return 0;
	}

	option = box->sel->udata;
	if (option->flags != OPT_AUTOCREATE) {
		if (box->sel->root) option = box->sel->root->udata;
		if (!option || option->flags != OPT_AUTOCREATE)
			goto invalid_option;
	}

	input_field(term, NULL, N_(T_ADD_OPTION), N_(T_NNAME),
		N_(T_OK), N_(T_CANCEL), option, NULL,
		MAX_STR_LEN, "", 0, 0, NULL,
		add_option_to_tree, NULL);
	return 0;
}


/* FIXME: Races here, we need to lock the entry..? --pasky */

static void
really_delete_option(void *data)
{
	struct option *option = data;
	struct listbox_data *box;

	foreach (box, *option->box_item->box) {
		if (box->sel == option->box_item) {
			box->sel = traverse_listbox_items_list(option->box_item, -1,
					1, NULL, NULL);
			if (option->box_item == box->sel)
				box->sel = traverse_listbox_items_list(option->box_item, 1,
						1, NULL, NULL);
			if (option->box_item == box->sel)
				box->sel = NULL;
		}

		if (box->top == option->box_item) {
			box->top = traverse_listbox_items_list(option->box_item, -1,
					1, NULL, NULL);
			if (option->box_item == box->top)
				box->top = traverse_listbox_items_list(option->box_item, 1,
						1, NULL, NULL);
			if (option->box_item == box->top)
				box->top = NULL;
		}
	}

	delete_option(option);
}

static int
push_del_button(struct dialog_data *dlg,
		struct widget_data *some_useless_info_button)
{
	struct terminal *term = dlg->win->term;
	struct listbox_data *box = (void *) dlg->dlg->items[OP_BOX_IND].data;
	struct option *option;

	if (!box->sel || !box->sel->udata) {

invalid_option:
		msg_box(term, NULL,
			N_(T_DELETE_OPTION), AL_CENTER,
			N_(T_CANNOT_DELETE_OPTION_HERE),
			NULL, 1,
			N_(T_CANCEL), NULL, B_ESC | B_ENTER);
		return 0;
	}

	option = box->sel->udata;
	if (!box->sel->root ||
	    ((struct option *) box->sel->root->udata)->flags != OPT_AUTOCREATE) {
		goto invalid_option;
	}

	msg_box(term, NULL,
		N_(T_DELETE_OPTION), AL_CENTER | AL_EXTD_TEXT,
		N_(T_REALLY_DELETE_OPTION), " \"", option->name, "\" ?", NULL,
		option, 2,
		N_(T_OK), really_delete_option, B_ENTER,
		N_(T_CANCEL), NULL, B_ESC);

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

	d->title = N_(T_OPTIONS_MANAGER);
	d->fn = layout_hierbox_browser;
	d->handle_event = hierbox_dialog_event_handler;
	d->abort = option_dialog_abort_handler;
	d->udata = ses;

	d->items[0].type = D_BUTTON;
	d->items[0].gid = B_ENTER;
	d->items[0].fn = push_info_button;
	d->items[0].udata = ses;
	d->items[0].text = N_(T_INFO);

	d->items[1].type = D_BUTTON;
	d->items[1].gid = B_ENTER;
	d->items[1].fn = push_edit_button;
	d->items[1].udata = ses;
	d->items[1].text = N_(T_EDIT);

	d->items[2].type = D_BUTTON;
	d->items[2].gid = B_ENTER;
	d->items[2].fn = push_add_button;
	d->items[2].udata = ses;
	d->items[2].text = N_(T_ADD);

	d->items[3].type = D_BUTTON;
	d->items[3].gid = B_ENTER;
	d->items[3].fn = push_del_button;
	d->items[3].udata = ses;
	d->items[3].text = N_(T_DELETE);

	d->items[4].type = D_BUTTON;
	d->items[4].gid = B_ENTER;
	d->items[4].fn = push_save_button;
	d->items[4].udata = ses;
	d->items[4].text = N_(T_SAVE);

	d->items[5].type = D_BUTTON;
	d->items[5].gid = B_ESC;
	d->items[5].fn = cancel_dialog;
	d->items[5].text = N_(T_CLOSE);

	d->items[OP_BOX_IND].type = D_BOX;
	d->items[OP_BOX_IND].gid = 12;
	d->items[OP_BOX_IND].data = (void *) option_dlg_box_build();

	d->items[OP_BOX_IND + 1].type = D_END;
	do_dialog(term, d, getml(d, NULL));
}



/****************************************************************************
  Keybinding manager stuff.
****************************************************************************/

/* The location of the box in the keybinding manager */
#define	KB_BOX_IND		5

/* Creates the box display (holds everything EXCEPT the actual rendering
 * data) */
static struct listbox_data *
kbdbind_dlg_box_build()
{
	struct listbox_data *box;

	/* Deleted in abort */
	box = mem_calloc(1, sizeof(struct listbox_data));
	if (!box) return NULL;

	box->items = &kbdbind_box_items;
	add_to_list(kbdbind_boxes, box);

	return box;
}


/* Cleans up after the keybinding dialog */
static void
kbdbind_dialog_abort_handler(struct dialog_data *dlg)
{
	struct listbox_data *box;

	box = (struct listbox_data *) dlg->dlg->items[KB_BOX_IND].data;

	del_from_list(box);
	/* Delete the box structure */
	mem_free(box);
}


struct kbdbind_add_hop {
	struct terminal *term;
	int action, keymap;
};

static void
really_add_keybinding(void *data, unsigned char *keystroke)
{
	struct kbdbind_add_hop *hop = data;
	long key, meta;

	/* TODO: This should maybe rather happen in a validation function? */
	if (parse_keystroke(keystroke, &key, &meta) < 0) {
		msg_box(hop->term, NULL,
			N_(T_ADD_KEYBINDING), AL_CENTER,
			N_(T_INVALID_KEYSTROKE),
			NULL, 1,
			N_(T_CANCEL), NULL, B_ESC | B_ENTER);
		return;
	}

	add_keybinding(hop->keymap, hop->action, key, meta, 0);
}

static int
push_kbdbind_add_button(struct dialog_data *dlg,
		struct widget_data *some_useless_info_button)
{
	struct terminal *term = dlg->win->term;
	struct listbox_data *box = (void *) dlg->dlg->items[KB_BOX_IND].data;
	struct listbox_item *item = box->sel;
	struct kbdbind_add_hop *hop;
	unsigned char *text;

	if (!item || !item->depth) {
		msg_box(term, NULL,
			N_(T_ADD_KEYBINDING), AL_CENTER,
			N_(T_NEED_TO_SELECT_KEYMAP),
			NULL, 1,
			N_(T_CANCEL), NULL, B_ESC | B_ENTER);
		return 0;
	}

	hop = mem_calloc(1, sizeof(struct kbdbind_add_hop));
	if (!hop) return 0;
	hop->term = term;

	if (item->depth == 2)
		item = item->root;
	hop->keymap = (int) item->udata;
	hop->action = (int) item->root->udata;

	text = straconcat(GT(N_(T_ACTION), term), ": ", write_action(hop->action), "\n",
			  GT(N_(T_KKEYMAP),term), ": ", write_keymap(hop->keymap), "\n",
			  "\n", GT(N_(T_KEYSTROKE_HELP), term), "\n\n",
			  GT(N_(T_KEYSTROKE), term), NULL);
	if (!text) {
		mem_free(hop);
		return 0;
	}

	input_field(term, getml(text, hop, NULL), N_(T_ADD_KEYBINDING), text,
		N_(T_OK), N_(T_CANCEL), hop, NULL,
		MAX_STR_LEN, "", 0, 0, NULL,
		really_add_keybinding, NULL);
	return 0;
}


static int
push_kbdbind_toggle_display_button(struct dialog_data *dlg,
		struct widget_data *some_useless_info_button)
{
	toggle_display_action_listboxes();
	clear_dialog(dlg, some_useless_info_button);
	return 0;
}


/* FIXME: Races here, we need to lock the entry..? --pasky */

static void
really_delete_keybinding(void *data)
{
	struct keybinding *keybinding = data;
	struct listbox_data *box;

	foreach (box, *keybinding->box_item->box) {
		if (box->sel == keybinding->box_item) {
			box->sel = traverse_listbox_items_list(keybinding->box_item, -1,
					1, NULL, NULL);
			if (keybinding->box_item == box->sel)
				box->sel = traverse_listbox_items_list(keybinding->box_item, 1,
						1, NULL, NULL);
			if (keybinding->box_item == box->sel)
				box->sel = NULL;
		}

		if (box->top == keybinding->box_item) {
			box->top = traverse_listbox_items_list(keybinding->box_item, -1,
					1, NULL, NULL);
			if (keybinding->box_item == box->top)
				box->top = traverse_listbox_items_list(keybinding->box_item, 1,
						1, NULL, NULL);
			if (keybinding->box_item == box->top)
				box->top = NULL;
		}
	}

	free_keybinding(keybinding);
}

static int
push_kbdbind_del_button(struct dialog_data *dlg,
		struct widget_data *some_useless_info_button)
{
	struct terminal *term = dlg->win->term;
	struct listbox_data *box = (void *) dlg->dlg->items[KB_BOX_IND].data;
	struct keybinding *keybinding;

	if (!box->sel || box->sel->depth < 2) {
		msg_box(term, NULL,
			N_(T_DELETE_KEYBINDING), AL_CENTER,
			N_(T_NOT_A_KEYBINDING),
			NULL, 1,
			N_(T_CANCEL), NULL, B_ESC | B_ENTER);
		return 0;
	}

	keybinding = box->sel->udata;

	msg_box(term, NULL,
		N_(T_DELETE_KEYBINDING), AL_CENTER | AL_EXTD_TEXT,
		N_(T_REALLY_DELETE_KEYBINDING), " \"", box->sel->text, "\" (",
		N_(T_AACTION), " \"", write_action(keybinding->action), "\", ",
		N_(T_KEYMAP), " \"", write_keymap(keybinding->keymap), "\") ?", NULL,
		keybinding, 2,
		N_(T_OK), really_delete_keybinding, B_ENTER,
		N_(T_CANCEL), NULL, B_ESC);

	return 0;
}


static int
push_kbdbind_save_button(struct dialog_data *dlg,
		struct widget_data *some_useless_info_button)
{
	write_config(dlg->win->term);
	return 0;
}

/* Builds the "Keybinding manager" dialog */
void
menu_keybinding_manager(struct terminal *term, void *fcp, struct session *ses)
{
	struct dialog *d;

	/* Create the dialog */
	d = mem_calloc(1, sizeof(struct dialog)
			  + (KB_BOX_IND + 2) * sizeof(struct widget)
			  + sizeof(struct option) + 2 * MAX_STR_LEN);
	if (!d) return;

	d->title = N_(T_KEYBINDING_MANAGER);
	d->fn = layout_hierbox_browser;
	d->handle_event = hierbox_dialog_event_handler;
	d->abort = kbdbind_dialog_abort_handler;
	d->udata = ses;

	d->items[0].type = D_BUTTON;
	d->items[0].gid = B_ENTER;
	d->items[0].fn = push_kbdbind_add_button;
	d->items[0].udata = ses;
	d->items[0].text = N_(T_ADD);

	d->items[1].type = D_BUTTON;
	d->items[1].gid = B_ENTER;
	d->items[1].fn = push_kbdbind_del_button;
	d->items[1].udata = ses;
	d->items[1].text = N_(T_DELETE);

	d->items[2].type = D_BUTTON;
	d->items[2].gid = B_ENTER;
	d->items[2].fn = push_kbdbind_toggle_display_button;
	d->items[2].udata = ses;
	d->items[2].text = N_(T_TOGGLE_DISPLAY);

	d->items[3].type = D_BUTTON;
	d->items[3].gid = B_ENTER;
	d->items[3].fn = push_kbdbind_save_button;
	d->items[3].udata = ses;
	d->items[3].text = N_(T_SAVE);

	d->items[4].type = D_BUTTON;
	d->items[4].gid = B_ESC;
	d->items[4].fn = cancel_dialog;
	d->items[4].text = N_(T_CLOSE);

	d->items[KB_BOX_IND].type = D_BOX;
	d->items[KB_BOX_IND].gid = 12;
	d->items[KB_BOX_IND].data = (void *) kbdbind_dlg_box_build();

	d->items[KB_BOX_IND + 1].type = D_END;
	do_dialog(term, d, getml(d, NULL));
}
