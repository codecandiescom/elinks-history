/* Information about current document and current link */
/* $Id: document.c,v 1.5 2002/05/17 17:41:28 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#include "links.h"

#include "bfu/bfu.h"
#include "document/cache.h"
#include "document/history.h"
#include "document/globhist.h"
#include "document/location.h"
#include "document/session.h"
#include "document/view.h"
#include "document/html/renderer.h"
#include "intl/language.h"
#include "lowlevel/terminal.h"
#include "protocol/http/header.h"
#include "protocol/url.h"

/* Location info. message box. */
void
loc_msg(struct terminal *term, struct location *location,
	struct f_data_c *frame)
{
	struct global_history_item *historyitem;
	struct cache_entry *ce;
	unsigned char *a;
	unsigned char *url;
	unsigned char *str;
	int strl;

	if (!location) {
		msg_box(term, NULL,
			TEXT(T_INFO), AL_LEFT,
			TEXT(T_YOU_ARE_NOWHERE),
			NULL, 1,
			TEXT(T_OK), NULL, B_ENTER | B_ESC);
		return;
	}

	str = init_str();
	if (!str) return;
	strl = 0;

	add_to_str(&str, &strl, _(TEXT(T_URL), term));
	add_to_str(&str, &strl, ": ");

	url = strip_url_password(location->vs.url);
	if (url) {
		add_to_str(&str, &strl, url);
		mem_free(url);
	}

	/* We don't preserve this in url. */
	if (location->vs.goto_position) {
		add_to_str(&str, &strl, "#");
		add_to_str(&str, &strl, location->vs.goto_position);
	}

#if 0
	/* strip_url_password() takes care about this now */
	if (strchr(location->vs.url, POST_CHAR)) {
		add_bytes_to_str(&str, &strl, location->vs.url,
				 (unsigned char *) strchr(location->vs.url,
							  POST_CHAR)
				 - (unsigned char *) location->vs.url);

	} else {
		add_to_str(&str, &strl, location->vs.url);
	}
#endif

	add_to_str(&str, &strl, "\n");

	add_to_str(&str, &strl, _(TEXT(T_TITLE), term));
	add_to_str(&str, &strl, ": ");
	add_to_str(&str, &strl, frame->f_data->title);
	add_to_str(&str, &strl, "\n");

	add_to_str(&str, &strl, _(TEXT(T_LAST_VISIT_TIME), term));
	add_to_str(&str, &strl, ": ");
	historyitem = get_global_history_item(location->vs.url, NULL, 0);
	if (historyitem) {
		/* Stupid ctime() adds a newline, and we don't want that, so we
		 * use add_bytes_to_str. -- Miciah */
		a = ctime(&historyitem->last_visit);
		add_bytes_to_str(&str, &strl, a, strlen(a) - 1);
	} else {
		add_to_str(&str, &strl, _(TEXT(T_UNKNOWN), term));
	}

	if (!get_cache_entry(location->vs.url, &ce)) {
		add_to_str(&str, &strl, "\n");
		add_to_str(&str, &strl, _(TEXT(T_SIZE), term));
		add_to_str(&str, &strl, ": ");
		add_num_to_str(&str, &strl, ce->length);

		if (ce->incomplete) {
			add_to_str(&str, &strl, " (");
			add_to_str(&str, &strl, _(TEXT(T_INCOMPLETE), term));
			add_to_str(&str, &strl, ")");
		}

		add_to_str(&str, &strl, "\n");
		add_to_str(&str, &strl, _(TEXT(T_CODEPAGE), term));
		add_to_str(&str, &strl, ": ");
		add_to_str(&str, &strl, get_cp_name(location->vs.f->f_data->cp));

		if (location->vs.f->f_data->ass == 1) {
			add_to_str(&str, &strl, " (");
			add_to_str(&str, &strl, _(TEXT(T_ASSUMED), term));
			add_to_str(&str, &strl, ")");
		}

		if (location->vs.f->f_data->ass == 2) {
			add_to_str(&str, &strl, " (");
			add_to_str(&str, &strl, _(TEXT(T_IGNORING_SERVER_SETTING), term));
			add_to_str(&str, &strl, ")");
		}

		a = parse_http_header(ce->head, "Server", NULL);
		if (a) {
			add_to_str(&str, &strl, "\n");
			add_to_str(&str, &strl, _(TEXT(T_SERVER), term));
			add_to_str(&str, &strl, ": ");
			add_to_str(&str, &strl, a);
			mem_free(a);
		}

		a = parse_http_header(ce->head, "Date", NULL);
		if (a) {
			add_to_str(&str, &strl, "\n");
			add_to_str(&str, &strl, _(TEXT(T_DATE), term));
			add_to_str(&str, &strl, ": ");
			add_to_str(&str, &strl, a);
			mem_free(a);
		}

		if (ce->last_modified) {
			add_to_str(&str, &strl, "\n");
			add_to_str(&str, &strl, _(TEXT(T_LAST_MODIFIED), term));
			add_to_str(&str, &strl, ": ");
			add_to_str(&str, &strl, ce->last_modified);
		}
#ifdef HAVE_SSL
		if (ce->ssl_info) {
			add_to_str(&str, &strl, "\n");
			add_to_str(&str, &strl, "SSL cipher: ");
			add_to_str(&str, &strl, ce->ssl_info);
		}
#endif
	}

	a = print_current_link_do(frame, term);
	if (a) {
		add_to_str(&str, &strl, "\n\n");
		add_to_str(&str, &strl, _(TEXT(T_LINK), term));
		add_to_str(&str, &strl, ": ");
		add_to_str(&str, &strl, a);
		mem_free(a);
	}

	msg_box(term, getml(str, NULL),
		TEXT(T_INFO), AL_LEFT,
		str,
		NULL, 1,
		TEXT(T_OK), NULL, B_ENTER | B_ESC);
}

void
state_msg(struct session *ses)
{
	if (!have_location(ses))
		loc_msg(ses->term, NULL, NULL);
	else
		loc_msg(ses->term, cur_loc(ses), current_frame(ses));
}

/* Headers info. message box. */
void
head_msg(struct session *ses)
{
	struct cache_entry *ce;

	if (!have_location(ses)) {
		msg_box(ses->term, NULL,
			TEXT(T_HEADER_INFO), AL_LEFT,
			TEXT(T_YOU_ARE_NOWHERE),
			NULL, 1,
			TEXT(T_OK), NULL, B_ENTER | B_ESC);
		return;
	}

	if (find_in_cache(cur_loc(ses)->vs.url, &ce)) {
		unsigned char *headers;

		if (ce->head) {
			headers = stracpy(ce->head);
			if (!headers) return;

			if (*headers)  {
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
			}

		} else {
			headers = stracpy("");
			if (!headers) return;
		}

		/* Headers info message box. */
		msg_box(ses->term, getml(headers, NULL),
			TEXT(T_HEADER_INFO), AL_LEFT,
			headers,
			NULL, 1,
			TEXT(T_OK), NULL, B_ENTER | B_ESC);
	}
}
