/* Info dialogs */
/* $Id: info.c,v 1.96 2004/04/11 23:06:23 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "bfu/msgbox.h"
#include "dialogs/info.h"
#include "config/kbdbind.h"
#include "config/options.h"
#include "cache/cache.h"
#include "document/html/renderer.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/select.h"
#include "modules/version.h"
#include "sched/connection.h"
#include "sched/session.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#ifdef LEAK_DEBUG
#include "util/memdebug.h"
#endif
#include "util/memlist.h"
#include "util/memory.h"
#include "util/string.h"

void
menu_about(struct terminal *term, void *d, struct session *ses)
{
	msg_box(term, NULL, MSGBOX_FREE_TEXT,
		N_("About"), AL_CENTER,
		get_dyn_full_version(term, 1),
		NULL, 1,
		N_("OK"), NULL, B_ENTER | B_ESC);
}

struct keys_toggle_info {
	struct terminal *term;
	int toggle;
};

static void
push_toggle_keys_display_button(void *data)
{
	struct keys_toggle_info *info = data;

	menu_keys(info->term, (void *) !info->toggle, NULL);
}

void
menu_keys(struct terminal *term, void *d, struct session *ses)
{
	/* We scale by main mapping because it has the most actions */
	int actions[MAIN_ACTIONS] = {
		ACT_MAIN_MENU,
		ACT_MAIN_QUIT,
		ACT_MAIN_DOWN,
		ACT_MAIN_UP,
		ACT_MAIN_SCROLL_DOWN,
		ACT_MAIN_SCROLL_UP,
		ACT_MAIN_SCROLL_LEFT,
		ACT_MAIN_SCROLL_RIGHT,
		ACT_MAIN_BACK,
		ACT_MAIN_ENTER,
		ACT_MAIN_GOTO_URL,
		ACT_MAIN_GOTO_URL_CURRENT,
		ACT_MAIN_DOCUMENT_INFO,
		ACT_MAIN_HEADER_INFO,
		ACT_MAIN_SEARCH,
		ACT_MAIN_SEARCH_BACK,
		ACT_MAIN_FIND_NEXT,
		ACT_MAIN_FIND_NEXT_BACK,
		ACT_MAIN_DOWNLOAD,
		ACT_MAIN_TOGGLE_HTML_PLAIN,

		ACT_MAIN_NONE,
	};
	struct string keys;
	struct keys_toggle_info *info;

	info = mem_calloc(1, sizeof(struct keys_toggle_info));

	if (!info || !init_string(&keys)) {
		if (info) mem_free(info);
		return;
	}

	info->term = term;
	info->toggle = (int) d;

	if (info->toggle) {
		int action;
		enum keymap map;

		for (action = 0; action < MAIN_ACTIONS - 1; action++) {
			actions[action] = action + 1;
		}

		for (map = 0; map < KM_MAX; map++) {
			add_actions_to_string(&keys, actions, map, term);
			if (map + 1 < KM_MAX)
				add_to_string(&keys, "\n\n");

			/* Just a little reminder that the following code takes
			 * the easy way. */
			assert(MAIN_ACTIONS > EDIT_ACTIONS);
			assert(EDIT_ACTIONS > MENU_ACTIONS);

			if (map == KM_MAIN) {
				actions[EDIT_ACTIONS] = ACT_EDIT_NONE;
			} else if (map == KM_EDIT) {
				actions[MENU_ACTIONS] = ACT_MENU_NONE;
			}
		}
	} else {
		add_actions_to_string(&keys, (int *) actions, KM_MAIN, term);
	}

	msg_box(term, getml(info, NULL), MSGBOX_FREE_TEXT | MSGBOX_SCROLLABLE,
		N_("Keys"), AL_LEFT,
		keys.source,
		info, 2,
		N_("OK"), NULL, B_ENTER | B_ESC,
		N_("Toggle display"), push_toggle_keys_display_button, B_ENTER);
}

void
menu_copying(struct terminal *term, void *d, struct session *ses)
{
	msg_box(term, NULL, MSGBOX_FREE_TEXT,
		N_("Copying"), AL_CENTER,
		msg_text(term, N_("ELinks %s\n"
			"\n"
			"(C) 1999 - 2002 Mikulas Patocka\n"
			"(C) 2001 - 2004 Petr Baudis and others\n"
			"\n"
			"This program is free software; you can redistribute it "
			"and/or modify it under the terms of the GNU General Public "
			"License as published by the Free Software Foundation, "
			"specifically version 2 of the License."),
			VERSION_STRING),
		NULL, 1,
		N_("OK"), NULL, B_ENTER | B_ESC);
}


static unsigned char *
get_ressource_info(struct terminal *term, void *data)
{
	return msg_text(term, N_("Resources: %d handles, %d timers.\n"
		"Connections: %d connections, %d connecting, %d "
		"transferring, %d keepalive.\n"
		"Memory cache: %d bytes, %d files, %d locked, %d "
		"loading.\n"
		"Formatted document cache: %d documents, %d locked."),
		select_info(INFO_FILES), select_info(INFO_TIMERS),
		connect_info(INFO_FILES), connect_info(INFO_CONNECTING),
		connect_info(INFO_TRANSFER), connect_info(INFO_KEEP),
		cache_info(INFO_BYTES), cache_info(INFO_FILES),
		cache_info(INFO_LOCKED), cache_info(INFO_LOADING),
		formatted_info(INFO_FILES), formatted_info(INFO_LOCKED));
}

void
resource_info(struct terminal *term)
{
	refreshed_msg_box(term, 0, N_("Resources"), AL_LEFT,
			  get_ressource_info, NULL);
}

#ifdef LEAK_DEBUG

static unsigned char *
get_memory_info(struct terminal *term, void *data)
{
	return msg_text(term, N_("%ld bytes of memory allocated."), mem_amount);
}

void
memory_inf(struct terminal *term, void *d, struct session *ses)
{
	refreshed_msg_box(term, 0, N_("Memory info"), AL_LEFT,
			  get_memory_info, NULL);
}

#endif
