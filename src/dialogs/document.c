/* Information about current document and current link */
/* $Id: document.c,v 1.96 2004/07/02 23:14:22 zas Exp $ */

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
#include "cache/cache.h"
#include "dialogs/document.h"
#include "document/document.h"
#include "document/html/renderer.h"
#include "document/view.h"
#include "globhist/globhist.h"
#include "intl/gettext/libintl.h"
#include "protocol/header.h"
#include "protocol/uri.h"
#include "sched/location.h"
#include "sched/session.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
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
		title, ALIGN_LEFT,
		N_("You are nowhere!"),
		NULL, 1,
		N_("OK"), NULL, B_ENTER | B_ESC);
}

/* Location info. message box. */
void
document_info_dialog(struct session *ses)
{
	struct terminal *term = ses->tab->term;
	struct location *location = cur_loc(ses);
	struct document_view *doc_view;
	struct cache_entry *cached;
	struct string msg;

	if (!location) {
		nowhere_box(term, NULL);
		return;
	}

	doc_view = current_frame(ses);

	if (!init_string(&msg)) return;

	add_to_string(&msg, _("URL", term));
	add_to_string(&msg, ": ");

	/* Add the uri with password and post info stripped */
	add_uri_to_string(&msg, location->vs.uri, URI_PUBLIC);

	add_char_to_string(&msg, '\n');

	if (doc_view && doc_view->document->title) {
		add_format_to_string(&msg, "%s: %s", _("Title", term),
				     doc_view->document->title);
	}

	add_char_to_string(&msg, '\n');

	cached = find_in_cache(location->vs.uri);
	if (cached) {
		unsigned char *a;

		add_format_to_string(&msg, "\n%s: %d",
				     _("Size", term), cached->length);

		if (cached->incomplete) {
			add_format_to_string(&msg, "(%s)", _("incomplete", term));
		}

		if (doc_view) {
			add_format_to_string(&msg, "\n%s: %s", _("Codepage", term),
					get_cp_name(doc_view->document->cp));

			if (doc_view->document->cp_status == CP_STATUS_ASSUMED) {
				add_format_to_string(&msg, " (%s)", _("assumed", term));
			} else if (doc_view->document->cp_status == CP_STATUS_IGNORED) {
				add_format_to_string(&msg, " (%s)",
						_("ignoring server setting", term));
			}
		}

		a = parse_header(cached->head, "Server", NULL);
		if (a) {
			add_format_to_string(&msg, "\n%s: %s",
					     _("Server", term), a);
			mem_free(a);
		}

		if (cached->ssl_info) {
			add_format_to_string(&msg, "\n%s: %s",
					     _("SSL Cipher", term),
					     cached->ssl_info);
		}
		if (cached->encoding_info) {
			add_format_to_string(&msg, "\n%s: %s",
					     _("Encoding", term),
					     cached->encoding_info);
		}

		a = parse_header(cached->head, "Date", NULL);
		if (a) {
			add_format_to_string(&msg, "\n%s: %s",
					     _("Date", term), a);
			mem_free(a);
		}

		if (cached->last_modified) {
			add_format_to_string(&msg, "\n%s: %s",
					     _("Last modified", term),
					     cached->last_modified);
		}

	}

#ifdef CONFIG_GLOBHIST
	{
		unsigned char *last_visit = NULL;
		struct global_history_item *historyitem;

		add_format_to_string(&msg, "\n%s: ",
				     _("Last visit time", term));

		historyitem = get_global_history_item(struri(location->vs.uri));

		if (historyitem) last_visit = ctime(&historyitem->last_visit);

		/* GNU's documentation says that ctime() can return NULL.
		 * The Open Group Base Specifications Issue 6 implies
		 * otherwise, but is ambiguous. Let's be safe. -- Miciah
		 */
		if (last_visit) {
			/* The string returned by ctime() includes a newline,
			 * and we don't want that, so we use add_bytes_to_str.
			 * The string always has exactly 25 characters, so add
			 * 24 bytes: The length of the string, minus one for
			 * the newline. -- Miciah
			 */
			add_bytes_to_string(&msg, last_visit, 24);
		} else {
			add_to_string(&msg, _("Unknown", term));
		}
	}
#endif

	if (doc_view) {
		unsigned char *a = get_current_link_info(ses, doc_view);

		add_char_to_string(&msg, '\n');
		if (a) {
			add_format_to_string(&msg, "\n%s: %s",
					     _("Link", term), a);
			mem_free(a);
		}

		a = get_current_link_title(doc_view);
		if (a) {
			add_format_to_string(&msg, "\n%s: %s",
					     _("Link title", term), a);
			mem_free(a);
		}

#ifdef CONFIG_GLOBHIST
		{
			struct global_history_item *historyitem = NULL;
			struct link *link = get_current_link_in_view(doc_view);

			if (link) {
				historyitem =
					get_global_history_item(link->where);
			}

			if (historyitem) {
				unsigned char *last_visit;

				last_visit = ctime(&historyitem->last_visit);

				if (last_visit)
					add_format_to_string(&msg,
						"\n%s: %.24s",
						_("Link last visit time",
						  term),
						last_visit);

				if (*historyitem->title)
					add_format_to_string(&msg, "\n%s: %s",
						_("Link title (from history)",
						  term),
						historyitem->title);
			}
		}
#endif
	}

	msg_box(term, NULL, MSGBOX_FREE_TEXT | MSGBOX_SCROLLABLE,
		N_("Info"), ALIGN_LEFT,
		msg.source,
		NULL, 1,
		N_("OK"), NULL, B_ENTER | B_ESC);
}

/* Headers info. message box. */
void
protocol_header_dialog(struct session *ses)
{
	struct cache_entry *cached;

	if (!have_location(ses)) {
		nowhere_box(ses->tab->term, N_("Header info"));
		return;
	}

	cached = find_in_cache(cur_loc(ses)->vs.uri);
	if (cached && cached->head) {
		unsigned char *headers = stracpy(cached->head);

		if (!headers) return;

		/* If the headers string starts by a newline, it means that it
		 * is artificially generated, usually to make ELinks-generated
		 * documents (ie. file:// directory listings) text/html. */
		if (*headers && *headers != '\r')  {
			int i = 0, j = 0;
			/* Sanitize headers string. */
			/* XXX: Do we need to check length and limit
			 * it to something reasonable ? */

			while (cached->head[i]) {
				/* Check for control chars. */
				if (cached->head[i] < ' '
				    && cached->head[i] != '\n') {
					/* Ignore '\r' but replace
					 * others control chars with
					 * a visible char. */
					if (cached->head[i] != '\r') {
						 headers[j] = '*';
						 j++;
					}
				} else {
					headers[j] = cached->head[i];
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
				msg_box(ses->tab->term, NULL, MSGBOX_FREE_TEXT | MSGBOX_SCROLLABLE,
					N_("Header info"), ALIGN_LEFT,
					headers,
					NULL, 1,
					N_("OK"), NULL, B_ENTER | B_ESC);

			return;
		}

		mem_free(headers);
	}

	msg_box(ses->tab->term, NULL, 0,
		N_("Header info"), ALIGN_LEFT,
		N_("No header info."),
		NULL, 1,
		N_("OK"), NULL, B_ENTER | B_ESC);

}
