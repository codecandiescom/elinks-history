/* Information about current document and current link */
/* $Id: document.c,v 1.2 2002/05/04 08:14:44 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#include <links.h>

#include <bfu/bfu.h>
#include <document/cache.h>
#include <document/history.h>
#include <document/globhist.h>
#include <document/location.h>
#include <document/session.h>
#include <document/view.h>
#include <document/html/renderer.h>
#include <intl/language.h>
#include <lowlevel/terminal.h>
#include <protocol/http/header.h>
#include <protocol/url.h>


void loc_msg(struct terminal *term, struct location *location,
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
	strl = 0;

	add_to_str(&str, &strl, _(TEXT(T_URL), term));
	add_to_str(&str, &strl, ": ");

	url = strip_url_password(location->vs.url);
	if (url) {
		add_to_str(&str, &strl, url);
		mem_free(url);
	}

#if 0
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

		if ((a = parse_http_header(ce->head, "Server", NULL))) {
			add_to_str(&str, &strl, "\n");
			add_to_str(&str, &strl, _(TEXT(T_SERVER), term));
			add_to_str(&str, &strl, ": ");
			add_to_str(&str, &strl, a);
			mem_free(a);
		}

		if ((a = parse_http_header(ce->head, "Date", NULL))) {
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

	if ((a = print_current_link_do(frame, term))) {
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

void state_msg(struct session *ses)
{
	if (!have_location(ses)) loc_msg(ses->term, NULL, NULL);
	else loc_msg(ses->term, cur_loc(ses), current_frame(ses));
}

void head_msg(struct session *ses)
{
	struct cache_entry *ce;
	unsigned char *s, *ss;
	int len;

	if (!have_location(ses)) {
		msg_box(ses->term, NULL,
			TEXT(T_HEADER_INFO), AL_LEFT,
			TEXT(T_YOU_ARE_NOWHERE),
			NULL, 1,
			TEXT(T_OK), NULL, B_ENTER | B_ESC);
		return;
	}
	if (find_in_cache(cur_loc(ses)->vs.url, &ce)) {
		if (ce->head) ss = s = stracpy(ce->head);
		else s = ss = stracpy("");
		len = strlen(s) - 1;
		if (len > 0) {
			while ((ss = strstr(s, "\r\n"))) memmove(ss, ss + 1, strlen(ss));
			while (*s && s[strlen(s) - 1] == '\n') s[strlen(s) - 1] = 0;
		}
		msg_box(ses->term, getml(s, NULL),
			TEXT(T_HEADER_INFO), AL_LEFT,
			s,
			NULL, 1,
			TEXT(T_OK), NULL, B_ENTER | B_ESC);
	}
}
