/* Options dialogs */
/* $Id: dialogs.c,v 1.125 2003/11/24 00:33:55 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/button.h"
#include "bfu/dialog.h"
#include "bfu/hierbox.h"
#include "bfu/inpfield.h"
#include "bfu/listbox.h"
#include "bfu/msgbox.h"
#include "bfu/style.h"
#include "bfu/text.h"
#include "config/conf.h"
#include "config/dialogs.h"
#include "config/kbdbind.h"
#include "config/options.h"
#include "config/opttypes.h"
#include "intl/gettext/libintl.h"
#include "sched/session.h"
#include "terminal/kbd.h"
#include "terminal/terminal.h"
#include "util/color.h"
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
		N_("OK"), NULL, B_ENTER | B_ESC);
}



/****************************************************************************
  Option manager stuff.
****************************************************************************/

static void
done_info_button(void *vhop)
{
#if 0
	struct option *option = vhop;

#endif
}

static int
push_info_button(struct dialog_data *dlg_data,
		struct widget_data *some_useless_info_button)
{
	struct terminal *term = dlg_data->win->term;
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct option *option;

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
				option->name, _(option_types[option->type].name, term),
				value.source, _(option->desc ? option->desc
						      : (unsigned char *) "N/A",
					 term)),
			option, 1,
			N_("OK"), done_info_button, B_ESC | B_ENTER);
	} else {
		msg_box(term, NULL, MSGBOX_FREE_TEXT,
			N_("Info"), AL_LEFT,
			msg_text(term, N_("Name: %s\n"
				"Type: %s\n\n"
				"Description:\n%s"),
				option->name, _(option_types[option->type].name, term),
				_(option->desc  ? option->desc
						: (unsigned char *) "N/A", term)),
			option, 1,
			N_("OK"), done_info_button, B_ESC | B_ENTER);
	}

	return 0;
}


static int
check_valid_option(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct terminal *term = dlg_data->win->term;
	struct option *option = dlg_data->dlg->udata;
	struct session *ses = dlg_data->dlg->udata2;
	unsigned char *value = widget_data->cdata;
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
		N_("OK"), NULL, B_ESC | B_ENTER);
	return 1;
}

static void
build_edit_dialog(struct terminal *term, struct session *ses,
		  struct option *option)
{
#define EDIT_WIDGETS_COUNT 5
	struct dialog *dlg;
	unsigned char *value, *label, *name, *desc;
	struct string tvalue;

	if (!init_string(&tvalue)) return;

	commandline = 1;
	option_types[option->type].write(option, &tvalue);
	commandline = 0;

	/* Create the dialog */
	dlg = calloc_dialog(EDIT_WIDGETS_COUNT, MAX_STR_LEN);
	if (!dlg) {
		done_string(&tvalue);
		return;
	}

	dlg->title = _("Edit", term);
	dlg->layouter = generic_dialog_layouter;
	dlg->udata = option;
	dlg->udata2 = ses;

	value = (unsigned char *) &dlg->widgets[EDIT_WIDGETS_COUNT];
	safe_strncpy(value, tvalue.source, MAX_STR_LEN);
	done_string(&tvalue);

	name = straconcat(_("Name", term), ": ", option->name, "\n",
			  _("Type", term), ": ",
			  _(option_types[option->type].name, term), NULL);
	label = straconcat(_("Value", term), ": ", NULL);
	desc = straconcat(_("Description", term), ": \n",
			  _(option->desc ? option->desc
				  	 : (unsigned char *) "N/A", term),
			  NULL);

	if (!name || !desc || !label) {
		if (name) mem_free(name);
		if (desc) mem_free(desc);
		if (label) mem_free(label);
		mem_free(dlg);
		return;
	}

	/* FIXME: Compute some meaningful maximal width. --pasky */
	add_dlg_text(dlg, name, AL_LEFT, 1);
	add_dlg_field(dlg, label, 0, 0, check_valid_option, MAX_STR_LEN, value, NULL);
	dlg->widgets[dlg->widgets_size - 1].info.field.float_label = 1;

	add_dlg_text(dlg, desc, AL_LEFT, 0);

	add_dlg_button(dlg, B_ENTER, ok_dialog, _("OK", term), NULL);
	add_dlg_button(dlg, B_ESC, cancel_dialog, _("Cancel", term), NULL);

	add_dlg_end(dlg, EDIT_WIDGETS_COUNT);

	do_dialog(term, dlg, getml(dlg, label, name, desc, NULL));
#undef EDIT_WIDGETS_COUNT
}

static int
push_edit_button(struct dialog_data *dlg_data,
		 struct widget_data *some_useless_info_button)
{
	struct terminal *term = dlg_data->win->term;
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct option *option;

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
			N_("OK"), NULL, B_ESC | B_ENTER);
		return 0;
	}

	build_edit_dialog(term, dlg_data->dlg->udata, option);

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
push_add_button(struct dialog_data *dlg_data,
		struct widget_data *some_useless_info_button)
{
	struct terminal *term = dlg_data->win->term;
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct option *option;

	if (!box->sel || !box->sel->udata) {

invalid_option:
		msg_box(term, NULL, 0,
			N_("Add option"), AL_CENTER,
			N_("Cannot add an option here."),
			NULL, 1,
			N_("OK"), NULL, B_ESC | B_ENTER);
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

	delete_option(option);
}

static int
push_del_button(struct dialog_data *dlg_data,
		struct widget_data *some_useless_info_button)
{
	struct terminal *term = dlg_data->win->term;
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct option *option;

	if (!box->sel || !box->sel->udata) {

invalid_option:
		msg_box(term, NULL, 0,
			N_("Delete option"), AL_CENTER,
			N_("Cannot delete this option."),
			NULL, 1,
			N_("OK"), NULL, B_ESC | B_ENTER);
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
push_save_button(struct dialog_data *dlg_data,
		struct widget_data *some_useless_info_button)
{
	write_config(dlg_data->win->term);
	return 0;
}

#define	OPTION_MANAGER_BUTTONS	5

static struct hierbox_browser_button option_buttons[] = {
	{ N_("Info"),		push_info_button	},
	{ N_("Edit"),		push_edit_button	},
	{ N_("Add"),		push_add_button		},
	{ N_("Delete"),		push_del_button		},
	{ N_("Save"),		push_save_button	},
};

struct hierbox_browser option_browser = {
	N_("Option manager"),
	option_buttons,
	HIERBOX_BROWSER_BUTTONS_SIZE(option_buttons),

	{ D_LIST_HEAD(option_browser.boxes) },
	NULL,	/* Set in menu_options_manager() */
	{ D_LIST_HEAD(option_browser.dialogs) },
	NULL,
};

/* Builds the "Options manager" dialog */
void
menu_options_manager(struct terminal *term, void *fcp, struct session *ses)
{
	option_browser.items = &config_options->box_item->child;

	hierbox_browser(&option_browser, ses);
}



/****************************************************************************
  Keybinding manager stuff.
****************************************************************************/

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
			N_("OK"), NULL, B_ESC | B_ENTER);
		return;
	}

	add_keybinding(hop->keymap, hop->action, key, meta, 0);
}

static int
push_kbdbind_add_button(struct dialog_data *dlg_data,
		struct widget_data *some_useless_info_button)
{
	struct terminal *term = dlg_data->win->term;
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct listbox_item *item = box->sel;
	struct kbdbind_add_hop *hop;
	unsigned char *text;

	if (!item || !item->depth) {
		msg_box(term, NULL, 0,
			N_("Add keybinding"), AL_CENTER,
			N_("Need to select a keymap."),
			NULL, 1,
			N_("OK"), NULL, B_ESC | B_ENTER);
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
push_kbdbind_toggle_display_button(struct dialog_data *dlg_data,
		struct widget_data *some_useless_info_button)
{
	toggle_display_action_listboxes();
	clear_dialog(dlg_data, some_useless_info_button);
	return 0;
}


/* FIXME: Races here, we need to lock the entry..? --pasky */

static void
really_delete_keybinding(void *data)
{
	struct keybinding *keybinding = data;

	free_keybinding(keybinding);
}

static int
push_kbdbind_del_button(struct dialog_data *dlg_data,
		struct widget_data *some_useless_info_button)
{
	struct terminal *term = dlg_data->win->term;
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct keybinding *keybinding;

	if (!box->sel || box->sel->depth < 2) {
		msg_box(term, NULL, 0,
			N_("Delete keybinding"), AL_CENTER,
			N_("This item is not a keybinding. Try to press a space"
			   " in order to get to the keybindings themselves."),
			NULL, 1,
			N_("OK"), NULL, B_ESC | B_ENTER);
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
push_kbdbind_save_button(struct dialog_data *dlg_data,
		struct widget_data *some_useless_info_button)
{
	write_config(dlg_data->win->term);
	return 0;
}

#define	KEYBINDING_MANAGER_BUTTONS	4

static INIT_LIST_HEAD(keybinding_dialog_list);

static struct hierbox_browser_button keybinding_buttons[] = {
	{ N_("Add"),		push_kbdbind_add_button			},
	{ N_("Delete"),		push_kbdbind_del_button			},
	{ N_("Toggle display"),	push_kbdbind_toggle_display_button	},
	{ N_("Save"),		push_kbdbind_save_button		},
};

struct hierbox_browser keybinding_browser = {
	N_("Keybinding manager"),
	keybinding_buttons,
	HIERBOX_BROWSER_BUTTONS_SIZE(keybinding_buttons),

	{ D_LIST_HEAD(keybinding_browser.boxes) },
	&kbdbind_box_items,
	{ D_LIST_HEAD(keybinding_browser.dialogs) },
	NULL,
};

/* Builds the "Keybinding manager" dialog */
void
menu_keybinding_manager(struct terminal *term, void *fcp, struct session *ses)
{
	hierbox_browser(&keybinding_browser, ses);
}
