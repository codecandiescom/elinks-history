/* Form history related dialogs */
/* $Id: dialogs.c,v 1.3 2003/11/24 16:42:24 jonas Exp $ */

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

	if (listbox_info == LISTBOX_URI)
		return stracpy(formhist_data->url);

	if (!init_string(&info)) return NULL;

	/* TODO: More info --jonas */
	add_to_string(&info, _("URL", term));
	add_to_string(&info, ": ");
	add_to_string(&info, formhist_data->url);

	return info.source;
}

static void
done_formhist_data_item(struct listbox_item *item, int last)
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
	done_formhist_data_item,
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
	hierbox_browser(&formhist_browser, ses);
}
