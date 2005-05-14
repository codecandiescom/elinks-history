/* Pseudo about: protocol implementation */
/* $Id: about.c,v 1.12 2005/05/14 22:19:26 miciah Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "cache/cache.h"
#include "protocol/about.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "sched/connection.h"
#include "util/string.h"


void
about_protocol_handler(struct connection *conn)
{
	struct cache_entry *cached = get_cache_entry(conn->uri);

	/* Only do this the first time */
	if (cached && !cached->content_type) {
		int len = 0;

#ifndef CONFIG_SMALL
		if (!conn->uri->data) {
		} else if (!strcmp(conn->uri->data, "bloat")) {
			static const unsigned char *str =
				"<html><body>"
				"<p>Bloat? What bloat?</p>"
				"</body></html>";
			len = strlen(str);
			add_fragment(cached, 0, str, len);
		} else if (!strcmp(conn->uri->data, "links")) {
			static const unsigned char *str =
				"<html><body><pre>"
				"/*                 D~~)w  */\n"
				"/*                /    |  */\n"
				"/*             /'m_O   /  */\n"
				"/*           /'.\"//~~~~\\  */\n"
				"/*           `\\/`\\`--') , */\n"
				"/*             /  `~~~  | */\n"
				"/*            |         | */\n"
				"/*            |         , */\n"
				"/*            `_'p~~~~~/  */\n"
				"/*              .  ||_|   */\n"
				"/*          `  .  _|| |   */\n"
				"/*           `, ((____|   */\n"
				"</pre></body></html>";
			len = strlen(str);
			add_fragment(cached, 0, str, len);
		} else if (!strcmp(conn->uri->data, "mozilla")) {
			static const unsigned char *str =
				"<html><body text=\"white\" bgcolor=\"red\">"
				"<p align=\"center\">And the <em>beste</em> shall meet "
				"a <em>being</em> and the being shall wear signs "
				"of EL and the signs shall have colour of enke. "
				"And the being shall be of <em>Good Nature</em>. "
				"From on high the beste hath looked down upon the being "
				"and the being was <em>smal</em> compared to it. "
				"Yet <em>faster</em> and <em>leaner</em> it hath been "
				"and it would come through doors closed to the beste. "
				"And there was truly great <em>respect</em> "
				"twix the beste and the <em>smal being</em> "
				"and they bothe have <em>served</em> to naciouns. "
				"Yet only the <em>true believers</em> "
				"would choose betwene them and the followers "
				"of <em>mammon</em> stayed <em>blinded</em> to bothe.</p>"
				"<p align=\"right\">from <em>The Book of Mozilla</em> "
				"(Apocryphon of ELeasar), 12:24</p>"
				"</body></html>";
			len = strlen(str);
			add_fragment(cached, 0, str, len);
		} else if (!strcmp(conn->uri->data, "fear")) {
			static const unsigned char *str =
				"<html><body text=\"yellow\">"
				"<p>I must not fear. Fear is the mind-killer. "
				"Fear is the little-death that brings total obliteration. "
				"I will face my fear. "
				"I will permit it to pass over me and through me. "
				"And when it has gone past I will turn the inner eye "
					"to see its path. "
				"Where the fear has gone there will be nothing. "
				"Only I will remain.</p>"
				"</body></html>";
			len = strlen(str);
			add_fragment(cached, 0, str, len);
		}
#endif

		normalize_cache_entry(cached, len);

		/* Set content to known type */
		mem_free_set(&cached->content_type, stracpy("text/html"));
	}

	conn->cached = cached;
	abort_connection(conn, S_OK);
}
