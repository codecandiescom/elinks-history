/* Options dialogs */
/* $Id: dialogs.c,v 1.66 2003/07/31 16:56:12 jonas Exp $ */

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
#include "intl/gettext/libintl.h"
#include "sched/session.h"
#include "terminal/kbd.h"
#include "terminal/terminal.h"
#include "util/error.h"
#include "util/lists.h"
#include "util/memory.h"


void
write_config_error(struct terminal *term, struct memory_list *ml,
		   unsigned char *config_file, unsigned char *strerr)
{
	msg_box(term, ml, MSGBOX_FREE_TEXT,
		N_("Config error"), AL_CENTER,
		msg_text(term, N_("Unable to write to config file %s: %s"),
			config_file, strerr),
		NULL, 1,
		N_("Cancel"), NULL, B_ENTER | B_ESC);
}



/****************************************************************************
  Option manager stuff.
****************************************************************************/

/* The location of the box in the options manager */
#define	OP_BOX_IND		6

/* Creates the box display (holds everything EXCEPT the actual rendering
 * data) */
static struct listbox_data *
option_dlg_box_build(void)
{
	struct listbox_data *box;

	/* Deleted in abort */
	box = mem_calloc(1, sizeof(struct listbox_data));
	if (!box) return NULL;

	box->items = &config_option_box_items;
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
		struct string value;
		
		if (!init_string(&value)) return 0;

		option_types[option->type].write(option, &value);

		msg_box(term, getml(value.source, NULL), MSGBOX_FREE_TEXT,
			N_("Info"), AL_LEFT,
			msg_text(term, N_("Name: %s\n"
				"Type: %s\n"
				"Value: %s\n\n"
				"Description:\n%s"),
				option->name, option_types[option->type].name,
				value.source, _(option->desc ? option->desc
						      : (unsigned char *) "N/A",
					 term)),
			option, 1,
			N_("OK"), done_info_button, B_ESC | B_ENTER);
	} else {
		msg_box(term, NULL, MSGBOX_FREE_TEXT,
			N_("Info"), AL_LEFT,
			msg_text(term, N_("Name: %s\n"
				"Type: %s\n"
				"Description:\n%s"),
				option->name, option_types[option->type].name,
				_(option->desc  ? option->desc
						: (unsigned char *) "N/A", term)),
			option, 1,
			N_("OK"), done_info_button, B_ESC | B_ENTER);
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

	msg_box(term, NULL, 0,
		N_("Error"), AL_LEFT,
		N_("Bad option value."),
		NULL, 1,
		N_("Cancel"), NULL, B_ESC | B_ENTER);
	return 1;
}

static void
layout_edit_dialog(struct dialog_data *dlg)
{
	int max = 0, min = 0;
	int w, rw;
	int y = -1;
	struct terminal *term = dlg->win->term;
	unsigned char dialog_text_color = get_bfu_color(term, "dialog.text");
	struct option *option = dlg->dlg->udata;
	unsigned char *name, *type, *value, *desc;

	name = straconcat(_("Name", term), ": ", option->name, NULL);
	type = straconcat(_("Type", term), ": ",
			  _(option_types[option->type].name, term), NULL);
	value= straconcat(_("Value", term), ": ", NULL);
	desc = straconcat(_("Description", term), ": \n",
			  _(option->desc ? option->desc
				  	 : (unsigned char *) "N/A", term),
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

	text_width(term, name, &min, &max);
	text_width(term, type, &min, &max);
	text_width(term, value, &min, &max);
	text_width(term, desc, &min, &max);
	buttons_width(term, dlg->items + 1, 2, &min, &max);

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
	struct string tvalue;

	if (!init_string(&tvalue)) return;

	commandline = 1;
	option_types[option->type].write(option, &tvalue);
	commandline = 0;

	/* Create the dialog */
	d = mem_calloc(1, sizeof(struct dialog)
			  + (EDIT_DIALOG_FIELDS_NB + 1)
			    * sizeof(struct widget)
			  + MAX_STR_LEN);
	if (!d) {
		done_string(&tvalue);
		return;
	}

	d->title = _("Edit", term);
	d->fn = layout_edit_dialog;
	d->udata = option;
	d->udata2 = ses;

	value = (unsigned char *) &d->items[EDIT_DIALOG_FIELDS_NB + 1];
	safe_strncpy(value, tvalue.source, MAX_STR_LEN);
	done_string(&tvalue);

	/* FIXME: Compute some meaningful maximal width. --pasky */
	d->items[0].type = D_FIELD;
	d->items[0].dlen = MAX_STR_LEN;
	d->items[0].data = value;
	d->items[0].fn = check_valid_option;

	d->items[1].type = D_BUTTON;
	d->items[1].gid = B_ENTER;
	d->items[1].fn = ok_dialog;
	d->items[1].text = _("OK", term);

	d->items[2].type = D_BUTTON;
	d->items[2].gid = B_ESC;
	d->items[2].text = _("Cancel", term);
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
		msg_box(term, NULL, 0,
			N_("Edit"), AL_LEFT,
			N_("This option cannot be edited. This means that "
			   "this is some special option like a folder - try "
			   "to press a space in order to see its contents."),
			NULL, 1,
			N_("Cancel"), NULL, B_ESC | B_ENTER);
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
		msg_box(term, NULL, 0,
			N_("Add option"), AL_CENTER,
			N_("Cannot add an option here."),
			NULL, 1,
			N_("Cancel"), NULL, B_ESC | B_ENTER);
		return 0;
	}

	option = box->sel->udata;
	if (!(option->flags & OPT_AUTOCREATE)) {
		if (box->sel->root) option = box->sel->root->udata;
		if (!option || !(option->flags & OPT_AUTOCREATE))
			goto invalid_option;
	}

	input_field(term, NULL, 1, N_("Add option"), N_("Name"),
		N_("OK"), N_("Cancel"), option, NULL,
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
		msg_box(term, NULL, 0,
			N_("Delete option"), AL_CENTER,
			N_("Cannot delete this option."),
			NULL, 1,
			N_("Cancel"), NULL, B_ESC | B_ENTER);
		return 0;
	}

	option = box->sel->udata;
	if (!box->sel->root ||
	    !(((struct option *) box->sel->root->udata)->flags & OPT_AUTOCREATE)) {
		goto invalid_option;
	}

	msg_box(term, NULL, MSGBOX_FREE_TEXT,
		N_("Delete option"), AL_CENTER,
		msg_text(term, N_("Really delete the option \"%s\"?"),
			option->name),
		option, 2,
		N_("OK"), really_delete_option, B_ENTER,
		N_("Cancel"), NULL, B_ESC);

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

	d->title = _("Options manager", term);
	d->fn = layout_hierbox_browser;
	d->handle_event = hierbox_dialog_event_handler;
	d->abort = option_dialog_abort_handler;
	d->udata = ses;

	d->items[0].type = D_BUTTON;
	d->items[0].gid = B_ENTER;
	d->items[0].fn = push_info_button;
	d->items[0].udata = ses;
	d->items[0].text = _("Info", term);

	d->items[1].type = D_BUTTON;
	d->items[1].gid = B_ENTER;
	d->items[1].fn = push_edit_button;
	d->items[1].udata = ses;
	d->items[1].text = _("Edit", term);

	d->items[2].type = D_BUTTON;
	d->items[2].gid = B_ENTER;
	d->items[2].fn = push_add_button;
	d->items[2].udata = ses;
	d->items[2].text = _("Add", term);

	d->items[3].type = D_BUTTON;
	d->items[3].gid = B_ENTER;
	d->items[3].fn = push_del_button;
	d->items[3].udata = ses;
	d->items[3].text = _("Delete", term);

	d->items[4].type = D_BUTTON;
	d->items[4].gid = B_ENTER;
	d->items[4].fn = push_save_button;
	d->items[4].udata = ses;
	d->items[4].text = _("Save", term);

	d->items[5].type = D_BUTTON;
	d->items[5].gid = B_ESC;
	d->items[5].fn = cancel_dialog;
	d->items[5].text = _("Close", term);

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
kbdbind_dlg_box_build(void)
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
		msg_box(hop->term, NULL, 0,
			N_("Add keybinding"), AL_CENTER,
			N_("Invalid keystroke."),
			NULL, 1,
			N_("Cancel"), NULL, B_ESC | B_ENTER);
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
		msg_box(term, NULL, 0,
			N_("Add keybinding"), AL_CENTER,
			N_("Need to select a keymap."),
			NULL, 1,
			N_("Cancel"), NULL, B_ESC | B_ENTER);
		return 0;
	}

	hop = mem_calloc(1, sizeof(struct kbdbind_add_hop));
	if (!hop) return 0;
	hop->term = term;

	if (item->depth == 2)
		item = item->root;
	hop->keymap = (int) item->udata;
	hop->action = (int) item->root->udata;

	text = straconcat(_("Action", term), ": ", write_action(hop->action), "\n",
			  _("Keymap", term), ": ", write_keymap(hop->keymap), "\n",
			  "\n", _("Keystroke should be written in the format: "
				  "[Prefix-]Key\nPrefix: Shift, Ctrl, Alt\n"
				  "Key: a,b,c,...,1,2,3,...,Space,Up,PageDown,"
				  "Tab,Enter,Insert,F5,...", term), "\n\n",
			  _("Keystroke", term), NULL);
	if (!text) {
		mem_free(hop);
		return 0;
	}

	input_field(term, getml(text, hop, NULL), 0,
		_("Add keybinding", term), text,
		_("OK", term), _("Cancel", term), hop, NULL,
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
		msg_box(term, NULL, 0,
			N_("Delete keybinding"), AL_CENTER,
			N_("This item is not a keybinding. Try to press a space"
			   " in order to get to the keybindings themselves."),
			NULL, 1,
			N_("Cancel"), NULL, B_ESC | B_ENTER);
		return 0;
	}

	keybinding = box->sel->udata;

	msg_box(term, NULL, MSGBOX_FREE_TEXT,
		N_("Delete keybinding"), AL_CENTER,
		msg_text(term, N_("Really delete the keybinding \"%s\" "
			"(action \"%s\", keymap \"%s\")?"),
			box->sel->text, write_action(keybinding->action),
			write_keymap(keybinding->keymap)),
		keybinding, 2,
		N_("OK"), really_delete_keybinding, B_ENTER,
		N_("Cancel"), NULL, B_ESC);

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

	d->title = _("Keybinding manager", term);
	d->fn = layout_hierbox_browser;
	d->handle_event = hierbox_dialog_event_handler;
	d->abort = kbdbind_dialog_abort_handler;
	d->udata = ses;

	d->items[0].type = D_BUTTON;
	d->items[0].gid = B_ENTER;
	d->items[0].fn = push_kbdbind_add_button;
	d->items[0].udata = ses;
	d->items[0].text = _("Add", term);

	d->items[1].type = D_BUTTON;
	d->items[1].gid = B_ENTER;
	d->items[1].fn = push_kbdbind_del_button;
	d->items[1].udata = ses;
	d->items[1].text = _("Delete", term);

	d->items[2].type = D_BUTTON;
	d->items[2].gid = B_ENTER;
	d->items[2].fn = push_kbdbind_toggle_display_button;
	d->items[2].udata = ses;
	d->items[2].text = _("Toggle display", term);

	d->items[3].type = D_BUTTON;
	d->items[3].gid = B_ENTER;
	d->items[3].fn = push_kbdbind_save_button;
	d->items[3].udata = ses;
	d->items[3].text = _("Save", term);

	d->items[4].type = D_BUTTON;
	d->items[4].gid = B_ESC;
	d->items[4].fn = cancel_dialog;
	d->items[4].text = _("Close", term);

	d->items[KB_BOX_IND].type = D_BOX;
	d->items[KB_BOX_IND].gid = 12;
	d->items[KB_BOX_IND].data = (void *) kbdbind_dlg_box_build();

	d->items[KB_BOX_IND + 1].type = D_END;
	do_dialog(term, d, getml(d, NULL));
}
