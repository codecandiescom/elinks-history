/* Form history related dialogs */
/* $Id: dialogs.c,v 1.27 2004/06/14 00:53:47 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/button.h"
#include "bfu/dialog.h"
#include "bfu/hierbox.h"
#include "bfu/listbox.h"
#include "bfu/msgbox.h"
#include "bfu/text.h"
#include "formhist/dialogs.h"
#include "formhist/formhist.h"
#include "dialogs/edit.h"
#include "intl/gettext/libintl.h"
#include "protocol/uri.h"
#include "sched/session.h"
#include "terminal/terminal.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/object.h"
#include "util/string.h"


static void
lock_formhist_data(struct listbox_item *item)
{
	object_lock((struct formhist_data *)item->udata);
}

static void
unlock_formhist_data(struct listbox_item *item)
{
	object_unlock((struct formhist_data *)item->udata);
}

static int
is_formhist_data_used(struct listbox_item *item)
{
	return is_object_used((struct formhist_data *)item->udata);
}

static unsigned char *
get_formhist_data_info(struct listbox_item *item, struct terminal *term,
		       enum listbox_info listbox_info)
{
	struct formhist_data *formhist_data = item->udata;
	struct string info;
	struct submitted_value *sv;

	switch (listbox_info) {
	case LISTBOX_TEXT:
		return stracpy(formhist_data->url);

	case LISTBOX_ALL:
		break;
	}

	if (!init_string(&info)) return NULL;

	add_format_to_string(&info, "%s: %s", _("URL", term), formhist_data->url);
	add_char_to_string(&info, '\n');

	if (formhist_data->dontsave)
		add_to_string(&info, _("Forms are never saved for this URL.", term));
	else
		add_to_string(&info, _("Forms are saved for this URL.", term));

	add_char_to_string(&info, '\n');
	foreach (sv, *formhist_data->submit) {
		add_format_to_string(&info, "\n[%8s] ", form_type2str(sv->type));

		add_to_string(&info, sv->name);
		add_to_string(&info, " = ");
		if (sv->value && *sv->value) {
			if (sv->type != FC_PASSWORD)
				add_to_string(&info, sv->value);
			else
				add_to_string(&info, "********");
		}
	}

	return info.source;
}

static struct uri *
get_formhist_data_uri(struct listbox_item *item)
{
	struct formhist_data *formhist_data = item->udata;

	return get_uri(formhist_data->url, 0);
}

static int
can_delete_formhist_data(struct listbox_item *item)
{
	return 1;
}

static void
delete_formhist_data(struct listbox_item *item, int last)
{
	struct formhist_data *formhist_data = item->udata;

	assert(!is_object_used(formhist_data));

	del_from_list(formhist_data);
	free_form(formhist_data);
}

static struct listbox_ops formhist_listbox_ops = {
	lock_formhist_data,
	unlock_formhist_data,
	is_formhist_data_used,
	get_formhist_data_info,
	get_formhist_data_uri,
	can_delete_formhist_data,
	delete_formhist_data,
	NULL,
};

static int
push_login_button(struct dialog_data *dlg_data,
		  struct widget_data *button)
{
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct formhist_data *formhist_data;
	struct terminal *term = dlg_data->win->term;

	if (!box->sel || !box->sel->udata) return 0;

	formhist_data = box->sel->udata;

	if (formhist_data->dontsave) {
		msg_box(term, NULL, 0, N_("Form not saved"), AL_CENTER,
			N_("No saved information for this URL.\n"
			"If you want to save passwords for this URL, enable "
			"it by the \"Toggle saving\" button."),
			NULL, 1,
			N_("OK"), NULL, B_ESC | B_ENTER);
		return 0;
	}

	push_hierbox_goto_button(dlg_data, button);

	return 0;
}

static int
push_toggle_dontsave_button(struct dialog_data *dlg_data,
			    struct widget_data *button)
{
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct formhist_data *formhist_data;

	if (!box->sel || !box->sel->udata) return 0;

	formhist_data = box->sel->udata;

	formhist_data->dontsave = !formhist_data->dontsave;
	return 0;
}

static int
push_save_button(struct dialog_data *dlg_data, struct widget_data *button)
{
	save_forms_to_file();
	return 0;
}

static struct hierbox_browser_button formhist_buttons[] = {
	{ N_("Login"),		push_login_button,		1 },
	{ N_("Info"),		push_hierbox_info_button,	1 },
	{ N_("Delete"),		push_hierbox_delete_button,	1 },
	{ N_("Toggle saving"),	push_toggle_dontsave_button,	0 },
	{ N_("Clear"),		push_hierbox_clear_button,	1 },
	{ N_("Save"),		push_save_button,		0 },
};

struct_hierbox_browser(
	formhist_browser,
	N_("Form history manager"),
	formhist_buttons,
	&formhist_listbox_ops
);

void
formhist_manager(struct session *ses)
{
	load_forms_from_file();
	hierbox_browser(&formhist_browser, ses);
}
