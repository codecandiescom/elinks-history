/* Information about current document and current link */
/* $Id: document.c,v 1.52 2003/07/22 15:36:58 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include "elinks.h"

#include "bfu/msgbox.h"
#include "dialogs/document.h"
#include "document/cache.h"
#include "document/html/renderer.h"
#include "globhist/globhist.h"
#include "intl/gettext/libintl.h"
#include "protocol/http/header.h"
#include "protocol/uri.h"
#include "sched/location.h"
#include "sched/session.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/link.h"
#include "viewer/text/view.h"

void
nowhere_box(struct terminal *term, unsigned char *title)
{
	assert(term);
	if_assert_failed return;

	if (!title || !*title)
		title = N_("Info");

	msg_box(term, NULL, 0,
		title, AL_LEFT,
		N_("You are nowhere!"),
		NULL, 1,
		N_("OK"), NULL, B_ENTER | B_ESC);
}

/* Location info. message box. */
static void
loc_msg(struct terminal *term, struct location *location,
	struct document_view *frame)
{
#ifdef GLOBHIST
	struct global_history_item *historyitem;
#endif
	struct cache_entry *ce;
	unsigned char *a;
	struct string msg;

	if (!location) {
		nowhere_box(term, NULL);
		return;
	}

	if (!init_string(&msg)) return;

	add_to_string(&msg, _("URL", term));
	add_to_string(&msg, ": ");

	/* Add the uri with password and post info stripped */
	add_string_uri_to_string(&msg, location->vs.url,
				 ~(URI_PASSWORD | URI_POST));

	/* We don't preserve this in url. */
	if (location->vs.goto_position) {
		add_char_to_string(&msg, '#');
		add_to_string(&msg, location->vs.goto_position);
	}

#if 0
	/* strip_url_password() takes care about this now */
	if (strchr(location->vs.url, POST_CHAR)) {
		add_bytes_to_string(&msg, location->vs.url,
				 (unsigned char *) strchr(location->vs.url,
							  POST_CHAR)
				 - (unsigned char *) location->vs.url);

	} else {
		add_to_string(&msg, location->vs.url);
	}
#endif

	add_char_to_string(&msg, '\n');

	if (frame && frame->document->title) {
		add_format_to_string(&msg, "%s: %s", _("Title", term),
				     frame->document->title);
	}

	add_char_to_string(&msg, '\n');

	if (!get_cache_entry(location->vs.url, &ce)) {
		add_format_to_string(&msg, "\n%s: %d",
				     _("Size", term), ce->length);

		if (ce->incomplete) {
			add_format_to_string(&msg, "(%s)", _("incomplete", term));
		}	

		add_format_to_string(&msg, "\n%s: %s", _("Codepage", term),
				get_cp_name(location->vs.view->document->cp));

		if (location->vs.view->document->cp_status == CP_STATUS_ASSUMED) {
			add_format_to_string(&msg, "(%s)", _("assumed", term));
		} else if (location->vs.view->document->cp_status == CP_STATUS_IGNORED) {
			add_format_to_string(&msg, "(%s)",
					_("ignoring server setting", term));
		}

		a = parse_http_header(ce->head, "Server", NULL);
		if (a) {
			add_format_to_string(&msg, "\n%s: %s",
					     _("Server", term), a);
			mem_free(a);
		}

		if (ce->ssl_info) {
			add_format_to_string(&msg, "\n%s: %s",
					     _("SSL Cipher", term),
					     ce->ssl_info);
		}
		if (ce->encoding_info) {
			add_format_to_string(&msg, "\n%s: %s",
					     _("Encoding", term),
					     ce->encoding_info);
		}

		a = parse_http_header(ce->head, "Date", NULL);
		if (a) {
			add_format_to_string(&msg, "\n%s: %s",
					     _("Date", term), a);
			mem_free(a);
		}

		if (ce->last_modified) {
			add_format_to_string(&msg, "\n%s: %s",
					     _("Last modified", term),
					     ce->last_modified);
		}

	}

#ifdef GLOBHIST
	add_format_to_string(&msg, "\n%s: ", _("Last visit time", term));
	historyitem = get_global_history_item(location->vs.url);
	if (historyitem) {
		/* Stupid ctime() adds a newline, and we don't want that, so we
		 * use add_bytes_to_str. -- Miciah */
		a = ctime(&historyitem->last_visit);
		add_bytes_to_string(&msg, a, strlen(a) - 1);
	} else {
		add_to_string(&msg, _("Unknown", term));
	}
#endif

	if (frame) {
		add_char_to_string(&msg, '\n');
		a = print_current_link_do(frame, term);
		if (a) {
			add_format_to_string(&msg, "\n%s: %s",
					     _("Link", term), a);
			mem_free(a);
		}

		a = print_current_link_title_do(frame, term);
		if (a) {
			add_format_to_string(&msg, "\n%s: %s",
					     _("Link title", term), a);
			mem_free(a);
		}
	}

	msg_box(term, getml(msg.source, NULL), 0,
		N_("Info"), AL_LEFT,
		msg.source,
		NULL, 1,
		N_("OK"), NULL, B_ENTER | B_ESC);
}

void
state_msg(struct session *ses)
{
	if (!have_location(ses))
		loc_msg(ses->tab->term, NULL, NULL);
	else
		loc_msg(ses->tab->term, cur_loc(ses), current_frame(ses));
}

/* Headers info. message box. */
void
head_msg(struct session *ses)
{
	struct cache_entry *ce;

	if (!have_location(ses)) {
		nowhere_box(ses->tab->term, N_("Header info"));
		return;
	}

	if (find_in_cache(cur_loc(ses)->vs.url, &ce) && ce->head) {
		unsigned char *headers = stracpy(ce->head);

		if (!headers) return;

		/* If the headers string starts by a newline, it means that it
		 * is artificially generated, usually to make ELinks-generated
		 * documents (ie. file:// directory listings) text/html. */
		if (*headers && *headers != '\r')  {
			int i = 0, j = 0;
			/* Sanitize headers string. */
			/* XXX: Do we need to check length and limit
			 * it to something reasonable ? */

			while (ce->head[i]) {
				/* Check for control chars. */
				if (ce->head[i] < ' '
				    && ce->head[i] != '\n') {
					/* Ignore '\r' but replace
					 * others control chars with
					 * a visible char. */
					if (ce->head[i] != '\r') {
						 headers[j] = '*';
						 j++;
					}
				} else {
					headers[j] = ce->head[i];
					j++;
				}
				i++;
			}

			/* Ensure null termination. */
			headers[j] = '\0';

			/* Remove all ending '\n' if any. */
			while (j && headers[--j] == '\n')
			headers[j] = '\0';


			if (*headers)
				/* Headers info message box. */
				msg_box(ses->tab->term, getml(headers, NULL), 0,
					N_("Header info"), AL_LEFT,
					headers,
					NULL, 1,
					N_("OK"), NULL, B_ENTER | B_ESC);

			return;
		}

		mem_free(headers);
	}

	msg_box(ses->tab->term, NULL, 0,
		N_("Header info"), AL_LEFT,
		N_("No header info."),
		NULL, 1,
		N_("OK"), NULL, B_ENTER | B_ESC);

}
