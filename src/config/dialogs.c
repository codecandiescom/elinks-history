/* Options dialogs */
/* $Id: dialogs.c,v 1.164 2004/04/13 16:36:23 jonas Exp $ */

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
#include "util/object.h"
#include "util/secsave.h"


static void
toggle_success_msgbox(void *dummy)
{
	get_opt_bool("ui.success_msgbox") = !get_opt_bool("ui.success_msgbox");
	get_opt_rec(config_options, "ui.success_msgbox")->flags |= OPT_TOUCHED;
}

void
write_config_dialog(struct terminal *term, unsigned char *config_file,
		    int secsave_error, int stdio_error)
{
	unsigned char *errmsg = NULL;
	unsigned char *strerr;

	if (secsave_error == SS_ERR_NONE && !stdio_error) {
		if (!get_opt_bool("ui.success_msgbox")) return;

		msg_box(term, NULL, MSGBOX_FREE_TEXT,
			N_("Write config success"), AL_CENTER,
			msg_text(term, N_("Options were saved successfully to config file %s."),
				 config_file),
			NULL, 2,
			N_("OK"), NULL, B_ENTER | B_ESC,
			N_("Do not show anymore"), toggle_success_msgbox, 0);
		return;
	}

	switch (secsave_error) {
		case SS_ERR_OPEN_READ:
			strerr = _("Cannot read the file", term);
			break;
		case SS_ERR_STAT:
			strerr = _("Cannot stat the file", term);
			break;
		case SS_ERR_ACCESS:
			strerr = _("Cannot access the file", term);
			break;
		case SS_ERR_MKSTEMP:
			strerr = _("Cannot create temp file", term);
			break;
		case SS_ERR_RENAME:
			strerr = _("Cannot rename the file", term);
			break;
		case SS_ERR_DISABLED:
			strerr = _("File saving disabled by option", term);
			break;
		case SS_ERR_OUT_OF_MEM:
			strerr = _("Out of memory", term);
			break;
		case SS_ERR_OPEN_WRITE:
			strerr = _("Cannot write the file", term);
			break;
		case SS_ERR_NONE: /* Impossible. */
		case SS_ERR_OTHER:
		default:
			strerr = _("Secure file error", term);
			break;
	}

	if (stdio_error > 0)
		errmsg = straconcat(strerr, " (", strerror(stdio_error), ")", NULL);

	msg_box(term, NULL, MSGBOX_FREE_TEXT,
		N_("Write config error"), AL_CENTER,
		msg_text(term, N_("Unable to write to config file %s.\n%s"),
			 config_file, errmsg ? errmsg : strerr),
		NULL, 1,
		N_("OK"), NULL, B_ENTER | B_ESC);

	if (errmsg) mem_free(errmsg);
}



/****************************************************************************
  Option manager stuff.
****************************************************************************/

/* Implementation of the listbox operations */

static void
lock_option(struct listbox_item *item)
{
	object_lock((struct option *)item->udata);
}

static void
unlock_option(struct listbox_item *item)
{
	object_unlock((struct option *)item->udata);
}

static int
is_option_used(struct listbox_item *item)
{
	return is_object_used((struct option *)item->udata);
}

static unsigned char *
get_range_string(struct option *option)
{
	struct string info;

	if (!init_string(&info)) return NULL;

	if (option->type == OPT_BOOL)
		add_to_string(&info, "[0|1]");
	else if (option->type == OPT_INT || option->type == OPT_LONG)
		add_format_to_string(&info, "[%li..%li]", option->min, option->max);

	return info.source;
}

static unsigned char *
get_option_info(struct listbox_item *item, struct terminal *term,
		  enum listbox_info listbox_info)
{
	struct option *option = item->udata;
	unsigned char *desc, *type;
	struct string info;

	if (listbox_info == LISTBOX_URI) return NULL;

	if (!init_string(&info)) return NULL;

	type = _(option_types[option->type].name, term);
	if (option->type == OPT_TREE) {
		type = straconcat(type, " ",
				_("(expand by pressing space)", term), NULL);
	}

	desc = _(option->desc  ? option->desc : (unsigned char *) "N/A", term);

	if (option_types[option->type].write) {
		unsigned char *range;
		struct string value;

		if (!init_string(&value)) {
			done_string(&info);
			return NULL;
		}

		option_types[option->type].write(option, &value);

		add_format_to_string(&info, "%s: %s", _("Name", term), option->name);
		add_format_to_string(&info, "\n%s: %s", _("Type", term), type);
		range = get_range_string(option);
		if (range) {
			if (*range) {
				add_to_string(&info, " ");
				add_to_string(&info, range);
			}
			mem_free(range);
		}
		add_format_to_string(&info, "\n%s: %s", _("Value", term), value.source);

		if (*desc)
			add_format_to_string(&info, "\n\n%s:\n%s", _("Description", term), desc);
		done_string(&value);
	} else {
		add_format_to_string(&info, "%s: %s", _("Name", term), option->name);
		add_format_to_string(&info, "\n%s: %s", _("Type", term), type);
		if (*desc)
			add_format_to_string(&info, "\n\n%s:\n%s", _("Description", term), desc);
	}

	if (option->type == OPT_TREE) {
		mem_free(type);
	}

	return info.source;
}

static int
can_delete_option(struct listbox_item *item)
{
	if (item->root) {
		struct option *parent_option = item->root->udata;

		return parent_option->flags & OPT_AUTOCREATE;
	}

	return 0;
}

static void
delete_option_item(struct listbox_item *item, int last)
{
	struct option *option = item->udata;

	assert(!is_object_used(option));

	mark_option_as_deleted(option);
}

static struct listbox_ops options_listbox_ops = {
	lock_option,
	unlock_option,
	is_option_used,
	get_option_info,
	can_delete_option,
	delete_option_item,
	NULL,
};

/* Button handlers */

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
	unsigned char *value, *name, *desc, *range;
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

	value = get_dialog_offset(dlg, EDIT_WIDGETS_COUNT);
	safe_strncpy(value, tvalue.source, MAX_STR_LEN);
	done_string(&tvalue);

	name = straconcat(_("Name", term), ": ", option->name, "\n",
			  _("Type", term), ": ",
			  _(option_types[option->type].name, term), NULL);
	desc = straconcat(_("Description", term), ": \n",
			  _(option->desc ? option->desc
				  	 : (unsigned char *) "N/A", term),
			  NULL);
	range = get_range_string(option);
	if (range) {
		if (*range) {
			unsigned char *tmp;

			tmp = straconcat(name, " ", range, NULL);
			if (tmp) {
				mem_free(name);
				name = tmp;
			}
		}
		mem_free(range);
	}

	if (!name || !desc) {
		if (name) mem_free(name);
		if (desc) mem_free(desc);
		mem_free(dlg);
		return;
	}

	/* FIXME: Compute some meaningful maximal width. --pasky */
	add_dlg_text(dlg, name, AL_LEFT, 0);
	add_dlg_field(dlg, _("Value", term), 0, 0, check_valid_option, MAX_STR_LEN, value, NULL);
	dlg->widgets[dlg->widgets_size - 1].info.field.float_label = 1;

	add_dlg_text(dlg, desc, AL_LEFT, 0);

	add_dlg_button(dlg, B_ENTER, ok_dialog, _("OK", term), NULL);
	add_dlg_button(dlg, B_ESC, cancel_dialog, _("Cancel", term), NULL);

	add_dlg_end(dlg, EDIT_WIDGETS_COUNT);

	do_dialog(term, dlg, getml(dlg, name, desc, NULL));
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
	struct option *old = get_opt_rec_real(option, name);

	if (old && (old->flags & OPT_DELETED)) delete_option(old);
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


static int
push_save_button(struct dialog_data *dlg_data,
		struct widget_data *some_useless_info_button)
{
	write_config(dlg_data->win->term);

	return 0;
}

#define	OPTION_MANAGER_BUTTONS	5

static struct hierbox_browser_button option_buttons[] = {
	{ N_("Info"),		push_hierbox_info_button,	1 },
	{ N_("Edit"),		push_edit_button,		0 },
	{ N_("Add"),		push_add_button,		0 },
	{ N_("Delete"),		push_hierbox_delete_button,	0 },
	{ N_("Save"),		push_save_button,		0 },
};

struct_hierbox_browser(
	option_browser,
	N_("Option manager"),
	option_buttons,
	&options_listbox_ops
);

/* Builds the "Options manager" dialog */
void
options_manager(struct session *ses)
{
	hierbox_browser(&option_browser, ses);
}


/****************************************************************************
  Keybinding manager stuff.
****************************************************************************/

struct kbdbind_add_hop {
	struct terminal *term;
	int action, keymap;
	long key, meta;
};

struct kbdbind_add_hop *
new_hop_from(struct kbdbind_add_hop *hop) {
	struct kbdbind_add_hop *new_hop = mem_alloc(sizeof(struct kbdbind_add_hop));

	if (new_hop)
		memcpy(new_hop, hop, sizeof(struct kbdbind_add_hop));

	return new_hop;
}

static void
really_really_add_keybinding(void *data)
{
	struct kbdbind_add_hop *hop = data;

	assert(hop);

	add_keybinding(hop->keymap, hop->action, hop->key, hop->meta, 0);
}

static void
really_add_keybinding(void *data, unsigned char *keystroke)
{
	struct kbdbind_add_hop *hop = data;
	int action;

	/* TODO: This should maybe rather happen in a validation function? */
	if (parse_keystroke(keystroke, &hop->key, &hop->meta) < 0) {
		msg_box(hop->term, NULL, 0,
			N_("Add keybinding"), AL_CENTER,
			N_("Invalid keystroke."),
			NULL, 1,
			N_("OK"), NULL, B_ESC | B_ENTER);
		return;
	}

	if (keybinding_exists(hop->keymap, hop->key, hop->meta, &action)
	    && action != ACT_MAIN_NONE) {
		struct kbdbind_add_hop *new_hop;

		/* Same keystroke for same action, just return. */
		if (action == hop->action) return;

		new_hop = new_hop_from(hop);
		if (!new_hop) return; /* out of mem */

		msg_box(new_hop->term, getml(new_hop, NULL), MSGBOX_FREE_TEXT,
			N_("Keystroke already used"), AL_CENTER,
			msg_text(new_hop->term, N_("The keystroke \"%s\" "
			"is currently used for \"%s\".\n"
			"Are you sure you want to replace it?"),
			keystroke, write_action(hop->keymap, action)),
			new_hop, 2,
			N_("Yes"), really_really_add_keybinding, B_ENTER,
			N_("No"), NULL, B_ESC);

		return;
	}

	really_really_add_keybinding((void *) hop);
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
	hop->action = (int) item->udata;
	hop->keymap = (int) item->root->udata;

	text = straconcat(_("Action", term), ": ", write_action(hop->keymap, hop->action), "\n",
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
			box->sel->text, write_action(keybinding->keymap, keybinding->action),
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
	{ N_("Add"),		push_kbdbind_add_button,		0 },
	{ N_("Delete"),		push_kbdbind_del_button,		0 },
	{ N_("Toggle display"),	push_kbdbind_toggle_display_button,	1 },
	{ N_("Save"),		push_kbdbind_save_button,		0 },
};

struct_hierbox_browser(
	keybinding_browser,
	N_("Keybinding manager"),
	keybinding_buttons,
	NULL
);

/* Builds the "Keybinding manager" dialog */
void
keybinding_manager(struct session *ses)
{
	hierbox_browser(&keybinding_browser, ses);
}
