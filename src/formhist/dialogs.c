/* Form history related dialogs */
/* $Id: dialogs.c,v 1.6 2003/11/25 01:07:31 jonas Exp $ */

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
#include "sched/session.h"
#include "terminal/kbd.h"
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

	if (listbox_info == LISTBOX_URI)
		return stracpy(formhist_data->url);

	if (!init_string(&info)) return NULL;

	add_to_string(&info, _("URL", term));
	add_to_string(&info, ": ");
	add_to_string(&info, formhist_data->url);
	add_char_to_string(&info, '\n');

	foreach (sv, *formhist_data->submit) {
		add_char_to_string(&info, '\n');
		add_to_string(&info, sv->name);
		add_to_string(&info, " = ");
		if (sv->value && *sv->value) {
			if (sv->type != FC_PASSWORD)
				add_to_string(&info, sv->value);
			else
				add_to_string(&info, "********");
		}
		add_to_string(&info, " (");
		add_to_string(&info, form_type2str(sv->type));
		add_char_to_string(&info, ')');
	}

	return info.source;
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
	can_delete_formhist_data,
	delete_formhist_data,
};

static int
push_save_button(struct dialog_data *dlg_data, struct widget_data *button)
{
	save_saved_forms();
	return 0;
}

static INIT_LIST_HEAD(formhist_data_box_items);

static struct hierbox_browser_button formhist_buttons[] = {
	{ N_("Login"),		push_hierbox_goto_button	},
	{ N_("Info"),		push_hierbox_info_button	},
	{ N_("Delete"),		push_hierbox_delete_button	},
	{ N_("Clear"),		push_hierbox_clear_button	},
	{ N_("Save"),		push_save_button		},
};

struct hierbox_browser formhist_browser = {
	N_("Form history manager"),
	formhist_buttons,
	HIERBOX_BROWSER_BUTTONS_SIZE(formhist_buttons),

	{ D_LIST_HEAD(formhist_browser.boxes) },
	&formhist_data_box_items,
	{ D_LIST_HEAD(formhist_browser.dialogs) },
	&formhist_listbox_ops,
};

void
menu_formhist_manager(struct terminal *term, void *fcp, struct session *ses)
{
	load_saved_forms();

	hierbox_browser(&formhist_browser, ses);
}
