/* Gopher access protocol (RFC 1436) */
/* $Id: gopher.c,v 1.11 2004/08/19 14:11:23 jonas Exp $ */

/* Based on version of HTGopher.c in the lynx tree.
 *
 * Author tags:
 *	TBL	Tim Berners-Lee
 * 	FM	Foteos Macrides
 *
 * A little history:
 *	26 Sep 90	Adapted from other accesses (News, HTTP) TBL
 *	29 Nov 91	Downgraded to C, for portable implementation.
 *	10 Mar 96	Foteos Macrides (macrides@sci.wfbr.edu).  Added a
 *			form-based CSO/PH gateway.  Can be invoked via a
 *			"cso://host[:port]/" or "gopher://host:105/2"
 *			URL.	If a gopher URL is used with a query token
 *			('?'), the old ISINDEX procedure will be used
 *			instead of the form-based gateway.
 *	15 Mar 96	Foteos Macrides (macrides@sci.wfbr.edu).  Pass
 *			port 79, gtype 0 gopher URLs to the finger
 *			gateway.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "elinks.h"

#include "cache/cache.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/connect.h"
#include "modules/module.h"
#include "protocol/gopher/gopher.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "sched/connection.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"


/* Gopher entity types */
enum gopher_entity {
	GOPHER_FILE		= '0',
	GOPHER_DIRECTORY	= '1',
	GOPHER_CSO		= '2',
	GOPHER_ERROR		= '3',
	GOPHER_MACBINHEX	= '4',
	GOPHER_PCBINARY		= '5',
	GOPHER_UUENCODED	= '6',
	GOPHER_INDEX		= '7',
	GOPHER_TELNET		= '8',
	GOPHER_BINARY		= '9',
	GOPHER_GIF		= 'g',
	GOPHER_HTML		= 'h', /* HTML */
	GOPHER_CHTML		= 'H', /* HTML */
	GOPHER_MIME		= 'm',
	GOPHER_SOUND		= 's',
	GOPHER_WWW		= 'w', /* W3 address */
	GOPHER_IMAGE		= 'I',
	GOPHER_TN3270		= 'T',
	GOPHER_INFO		= 'i', /* Information or separator line */
	GOPHER_DUPLICATE	= '+',
	GOPHER_PLUS_IMAGE	= ':', /* Addition from Gopher Plus */
	GOPHER_PLUS_MOVIE	= ';',
	GOPHER_PLUS_SOUND	= '<',
	GOPHER_PLUS_PDF		= 'P',
};

/* Default Gopher Node type is directory listing */
#define DEFAULT_GOPHER_ENTITY	GOPHER_DIRECTORY

#define entity_needs_gopher_access(entity)	\
	((entity) != GOPHER_TELNET		\
	 && (entity) != GOPHER_TN3270		\
	 && (entity) != GOPHER_WWW)

struct gopher_entity_info {
	enum gopher_entity type;
	unsigned char *description;
	unsigned char *content_type;
};

/* Lynx uses some hardcoded content types for various entities:
 *
 * - GOPHER_FILE has "text/plain" but really it can be anything including
 *   images so rely on the MIME system being able to determine it by looking at
 *   the extension.
 * - GOPHER_IMAGE and GOPHER_PLUS_IMAGE which Lynx assigns "image/gif" buf it
 *   really can be anything.
 * - GOPHER_BINARY can for example be a tar ball so using
 *   "application/octet-stream" is a bad idea.
 */
static struct gopher_entity_info gopher_entity_info[] = {
	{ GOPHER_BINARY,	"    (BINARY)",	NULL			   },
	{ GOPHER_CHTML,		"     (CHTML)",	"text/html"		   },
	{ GOPHER_CSO,		"       (CSO)",	"text/html"		   },
	{ GOPHER_DIRECTORY,	" (DIRECTORY)",	"text/html"		   },
	{ GOPHER_FILE,		"      (FILE)",	NULL /* "text/plain" */	   },
	{ GOPHER_GIF,		" (GIF IMAGE)",	"image/gif"		   },
	{ GOPHER_HTML,		"      (HTML)",	"text/html"		   },
	{ GOPHER_IMAGE,		"     (IMAGE)",	NULL /* "image/gif" */	   },
	{ GOPHER_INDEX,		"     (INDEX)",	"text/html"		   },
	{ GOPHER_MACBINHEX,	"(BINARY HEX)",	"application/octet-stream" },
	{ GOPHER_MIME,		"      (MIME)",	"application/octet-stream" },
	{ GOPHER_PCBINARY,	"  (PCBINARY)",	"application/octet-stream" },
	{ GOPHER_PLUS_IMAGE,	"    (IMAGE+)",	NULL /* "image/gif" */	   },
	{ GOPHER_PLUS_MOVIE,	"     (MOVIE)",	"video/mpeg"		   },
	{ GOPHER_PLUS_PDF,	"       (PDF)",	"application/pdf"	   },
	{ GOPHER_PLUS_SOUND,	"    (SOUND+)",	"audio/basic"		   },
	{ GOPHER_SOUND,		"     (SOUND)",	"audio/basic"		   },
	{ GOPHER_TELNET,	"    (TELNET)",	NULL			   },
	{ GOPHER_TN3270,	"    (TN3270)",	NULL			   },
	{ GOPHER_UUENCODED,	" (UUENCODED)",	"application/octet-stream" },
	{ GOPHER_WWW,		"(W3 ADDRESS)",	NULL			   },

	{ GOPHER_INFO,		"            ",	NULL			   },
	{ GOPHER_ERROR,		NULL,		NULL			   },
};

static struct gopher_entity_info *
get_gopher_entity_info(enum gopher_entity type)
{
	int entry;

	for (entry = 0; entry < sizeof(gopher_entity_info); entry++)
		if (gopher_entity_info[entry].type == type)
			return &gopher_entity_info[entry];

	return NULL;
}

static unsigned char *
get_gopher_entity_description(enum gopher_entity type)
{
	struct gopher_entity_info *info = get_gopher_entity_info(type);

	return info ? info->description : NULL;
}


struct gopher_connection_info {
	struct gopher_entity_info *entity;

	int commandlen;
	unsigned char command[1];
};

/* De-escape a selector into a command. */
/* The % hex escapes are converted. Otheriwse, the string is copied. */
static void
add_uri_decoded(struct string *command, unsigned char *string, int length,
	       int replace_plus)
{
	int oldlen = command->length;

	assert(string);

	if (!length) return;

	if (replace_plus) {
		/* Remove plus signs 921006 */
		if (!add_string_replace(command, string, length, '+', ' '))
			return;

	} else if (!add_bytes_to_string(command, string, length)) {
		return;
	}

	assert(command->length > oldlen);
	decode_uri_string(command->source + oldlen);

	/* Evil decode_uri_string() modifies the string */
	command->length = strlen(command->source);
}

static enum connection_state init_gopher_index_cache_entry(struct connection *conn);

static enum connection_state
add_gopher_command(struct connection *conn, struct string *command,
		   enum gopher_entity entity,
		   unsigned char *selector, int selectorlen)
{
	unsigned char *query;
	int querylen;

	if (!init_string(command))
		return S_OUT_OF_MEM;

	/* Look for search string */
	query = memchr(selector, '?', selectorlen);

	/* Check if no search is required */
	if (!query || !query[1]) {
		query = NULL;
		querylen = 0;
	} else {
		querylen = selector + selectorlen - query;
		/* Exclude '?' */
		selectorlen -= querylen + 1; 
	}

	switch (entity) {
	case GOPHER_INDEX:
		/* No search required? */
		if (!query) {
			done_string(command);
			return init_gopher_index_cache_entry(conn);
		}

		add_uri_decoded(command, selector, selectorlen, 0);
		add_char_to_string(command, '\t');
		add_uri_decoded(command, query, querylen, 1);
		break;

	case GOPHER_CSO:
		/* No search required */
		if (!query) {
			done_string(command);
			/* Display "cover page" */
#if 0
			return init_gopher_cso_cache_entry(conn);
#endif
			return S_GOPHER_CSO_ERROR;
		}

		add_uri_decoded(command, selector, selectorlen, 0);
		add_to_string(command, "query ");
		add_uri_decoded(command, query, querylen, 1);
		break;

	default:
		/* Not index */
		add_uri_decoded(command, selector, selectorlen, 0);
	}

	add_crlf_to_string(command);

	return S_CONN;
}

static enum connection_state
init_gopher_connection_info(struct connection *conn)
{
	struct gopher_connection_info *gopher;
	enum connection_state state;
	struct string command;
	enum gopher_entity entity = DEFAULT_GOPHER_ENTITY;
	unsigned char *selector = conn->uri->data;
	int selectorlen = conn->uri->datalen;
	size_t size;

	/* Get entity type, and selector string. */
	/* Pick up gopher_entity */
	if (selectorlen > 0) {
		entity = *selector++;
		selectorlen--;
	}

	state = add_gopher_command(conn, &command, entity, selector, selectorlen);
	if (state != S_CONN)
		return state;

	/* Atleast the command should contain \r\n to ask the server
	 * wazzup! */
	assert(command.length >= 2);

	size = sizeof(struct gopher_connection_info) + command.length;
	gopher = mem_calloc(1, size);
	if (!gopher) {
		done_string(&command);
		return S_OUT_OF_MEM;
	}

	gopher->entity = get_gopher_entity_info(entity);
	gopher->commandlen = command.length;

	memcpy(gopher->command, command.source, command.length);
	done_string(&command);

	conn->info = gopher;

	return S_CONN;
}


static void
end_gopher_connection(struct connection *conn, enum connection_state state)
{
	if (state == S_OK && conn->cached) {
#if 0
		/* Seeing dots at all files .. trying to remove the end-marker
		 * ".\r\n" */
		struct gopher_connection_info *gopher = conn->info;

		if (gopher
		    && gopher->entity
		    && gopher->entity->type != GOPHER_DIRECTORY
		    && gopher->entity->type != GOPHER_INDEX
		    && gopher->entity->type != GOPHER_CSO) {
			conn->from -= 3;
		}
#endif
		truncate_entry(conn->cached, conn->from, 1);

		conn->cached->incomplete = 0;
	}

	abort_conn_with_state(conn, state);
}


/* Add a link. The title of the destination is set, as there is no way of
 * knowing what the title is when we arrive.
 *
 *	text	points to the text to be put into the file, 0 terminated.
 * 	addr	points to the hypertext reference address 0 terminated.
 */

static void
add_gopher_link(struct string *buffer, const unsigned char *text,
		   const unsigned char *addr)
{
	add_format_to_string(buffer, "<a href=\"%s\">%s</a>",
			     addr, text);
}

static void
add_gopher_description(struct string *buffer, enum gopher_entity entity)
{
	unsigned char *description = get_gopher_entity_description(entity);

	add_to_string(buffer, "<b>");
	add_to_string(buffer, description);
	add_to_string(buffer, "</b> ");
}

static void
add_gopher_menu_line(struct string *buffer, unsigned char *line)
{
	/* Gopher menu fields */
	unsigned char *name = line;
	unsigned char *selector = NULL;
	unsigned char *host = NULL;
	unsigned char *port = NULL;
	enum gopher_entity entity = *name++;

	if (!entity) {
		add_char_to_string(buffer, '\n');
		return;
	}

	if (*name) {
		selector = strchr(name, ASCII_TAB);
		if (selector) {
			/* Terminate name */
			*selector++ = '\0';

			/* Gopher+ Type=0+ objects can be binary, and will have
			 * 9 or 5 beginning their selector.  Make sure we don't
			 * trash the terminal by treating them as text. - FM */
			if (entity == GOPHER_FILE
			    && (*selector == GOPHER_BINARY ||
				*selector == GOPHER_PCBINARY))
				entity = *selector;
		}

		host = selector ? strchr(selector, ASCII_TAB) : NULL;
		if (host) {
			/* Terminate selector */
			*host++ = '\0';
		}

		port = host ? strchr(host, ASCII_TAB) : NULL;
		if (port) {
			unsigned char *end;
			int portno;

			errno = 0;
			portno = strtol(port + 1, (char **) &end, 10);
			if (errno || !uri_port_is_valid(portno)) {
				port = NULL;

			} else {
				/* Try to wipe out the default gopher port
				 * number from being appended to links. */
				if (portno == 70
				    && entity_needs_gopher_access(entity))
					portno = 0;

				/* If the port number is 0 it means no port
				 * number is needed in which case it can be
				 * wiped out completely. Else append it to the
				 * host string a la W3.  */
				if (portno == 0) {
					*port = '\0';
				} else {
					*port = ':';

					/* Chop port if there is junk after the
					 * number */
					if (!*end) *end = 0;
				}
			}
		}
	}

	/* Nameless files are a separator line */
	if (entity == GOPHER_FILE) {
		int i = strlen(name) - 1;

		while (name[i] == ' ' && i >= 0)
			name[i--] = '\0';

		if (i < 0)
			entity = GOPHER_INFO;
	}

	add_gopher_description(buffer, entity);

	switch (entity) {
	case GOPHER_WWW:
		/* Gopher pointer to W3 */
		if (selector) {
			add_gopher_link(buffer, name, selector);
			break;
		}

		/* Fall through if no selector is defined so the
		 * text is just displayed. */

	case GOPHER_INFO:
		/* Information or separator line */
		add_to_string(buffer, name);
		break;

	default:
	{
		struct string address;
		unsigned char *format = *selector ? "%s://%s@%s/" : "%s://%s%s/";

		/* If port is defined it means that both @selector and @host
		 * was correctly parsed. */
		if (!port || !init_string(&address)) {
			/* Parse error: Bad menu item */
			add_to_string(buffer, name);
			break;
		}

		assert(selector && host);

		if (entity == GOPHER_TELNET) {
			add_format_to_string(&address, format,
					     "telnet", selector, host);

		} else if (entity == GOPHER_TN3270) {
			add_format_to_string(&address, format,
					     "tn3270", selector, host);

		} else {
			add_format_to_string(&address, "gopher://%s/%c",
				host, entity);

			/* Encode selector string */
			encode_uri_string(&address, selector, 0);
		}

		/* Error response from Gopher doesn't deserve to
		 * be a hyperlink. */
		if (strlcmp(address.source, address.length,
			    "gopher://error.host:1/0", -1))
			add_gopher_link(buffer, name, address.source);
		else
			add_to_string(buffer, name);

		done_string(&address);
	}
	}

	add_char_to_string(buffer, '\n');
}


/* Search for line ending \r\n pair */
static unsigned char *
get_gopher_line_end(unsigned char *data, int datalen)
{
	for (; datalen > 1; data++, datalen--)
		if (data[0] == ASCII_CR && data[1] == ASCII_LF)
			return data + 2;

	return NULL;
}

static inline unsigned char *
check_gopher_last_line(unsigned char *line, unsigned char *end)
{
	assert(line < end);

	/* Just to be safe NUL terminate the line */
	end[-2] = 0;

	return line[0] == '.' && !line[1] ? NULL : line;
}

/* Parse a Gopher Menu document */
static enum connection_state
read_gopher_directory_data(struct connection *conn, struct read_buffer *rb)
{
	enum connection_state state = S_TRANS;
	struct string buffer;
	unsigned char *end;

	if (!init_string(&buffer))
		return S_OUT_OF_MEM;

	if (conn->from == 0) {
		unsigned char *where = get_uri_string(conn->uri, URI_PUBLIC);

		add_format_to_string(&buffer,
			"<html>\n"
			"<head>\n"
			"<title>Gopher menu at %s</title>\n"
			"</head>\n"
			"<body>\n"
			"<h1>Gopher menu at %s</h1>\n"
			"<pre>",
			empty_string_or_(where), empty_string_or_(where));

		mem_free_if(where);
	}

	while ((end = get_gopher_line_end(rb->data, rb->len))) {
		unsigned char *line = check_gopher_last_line(rb->data, end);

		/* Break on line with a dot by itself */
		if (!line) {
			state = S_OK;
			break;
		}

		add_gopher_menu_line(&buffer, line);
		conn->received += end - rb->data;
		kill_buffer_data(rb, end - rb->data);
	}

	if (state != S_TRANS)
		add_to_string(&buffer,
			"</pre>\n"
			"</body>\n"
			"</html>\n");

	add_fragment(conn->cached, conn->from, buffer.source, buffer.length);
	conn->from += buffer.length;

	done_string(&buffer);

	return state;
}


static struct cache_entry *
init_gopher_cache_entry(struct connection *conn)
{
	struct gopher_connection_info *gopher = conn->info;
	struct cache_entry *cached = get_cache_entry(conn->uri);

	if (!cached) return NULL;

	conn->cached = cached;

	if (!cached->content_type
	    && gopher
	    && gopher->entity
	    && gopher->entity->content_type) {
		cached->content_type = stracpy(gopher->entity->content_type);
		if (!cached->content_type) return NULL;
	}

	return cached;
}

/* Display a Gopher Index document. */
static enum connection_state
init_gopher_index_cache_entry(struct connection *conn)
{
	unsigned char *where;
	struct string buffer;

	if (!init_gopher_cache_entry(conn)
	    || !init_string(&buffer))
		return S_OUT_OF_MEM;

	where = get_uri_string(conn->uri, URI_PUBLIC);

	add_format_to_string(&buffer,
		"<html>\n"
		"<head>\n"
		"<title>Gopher index at %s</title>\n"
		"<isindex>"
		"</head>\n"
		"<body>\n"
		"<h1>Gopher index at %s</h1>\n"
		"<p>This is a searchable Gopher index.</p>\n"
		"<p>Please enter search keywords.</p>\n",
		empty_string_or_(where), empty_string_or_(where));

	mem_free_if(where);

	/* FIXME: I think this needs a form or something */

	add_fragment(conn->cached, conn->from, buffer.source, buffer.length);
	conn->from += buffer.length;
	done_string(&buffer);

	conn->cached->content_type = stracpy("text/html");

	return conn->cached->content_type ? S_OK : S_OUT_OF_MEM;
}


static void
read_gopher_response_data(struct connection *conn, struct read_buffer *rb)
{
	struct gopher_connection_info *gopher = conn->info;
	enum connection_state state = S_TRANS;

	assert(gopher && gopher->entity);

	set_connection_timeout(conn);

	if (!conn->cached && !init_gopher_cache_entry(conn)) {
		end_gopher_connection(conn, S_OUT_OF_MEM);
		return;
	}

	if (rb->close == 2) {
		/* If the server suddenly closes the connection there can still
		 * be some error message that might be useful. */
		if (rb->len > 0) {
			add_fragment(conn->cached, conn->from, rb->data, rb->len);
			conn->received += rb->len;
			conn->from     += rb->len;
		}

		end_gopher_connection(conn, S_OK);
		return;
	}

	/* Now read the data from the socket */
	switch (gopher->entity->type) {
	case GOPHER_DIRECTORY:
	case GOPHER_INDEX:
		state = read_gopher_directory_data(conn, rb);
		break;

	case GOPHER_CSO:
#if 0
		/* FIXME: Merge CSO support */
		state = read_gopher_cso_data(conn, rb);
#endif
		state = S_GOPHER_CSO_ERROR;
		break;

	case GOPHER_SOUND:
	case GOPHER_PLUS_SOUND:
	case GOPHER_PLUS_MOVIE:
	case GOPHER_PLUS_PDF:
	case GOPHER_MACBINHEX:
	case GOPHER_PCBINARY:
	case GOPHER_UUENCODED:
	case GOPHER_BINARY:
	case GOPHER_FILE:
	case GOPHER_HTML:
	case GOPHER_CHTML:
	case GOPHER_GIF:
	case GOPHER_IMAGE:
	case GOPHER_PLUS_IMAGE:
	default:
		/* Add the received data as a new cache entry fragment and do
		 * the connection data accounting. */
		add_fragment(conn->cached, conn->from, rb->data, rb->len);

		conn->received += rb->len;
		conn->from     += rb->len;

		kill_buffer_data(rb, rb->len);
	}

	if (state != S_TRANS) {
		end_gopher_connection(conn, state);
		return;
	}

	read_from_socket(conn, &conn->socket, rb, read_gopher_response_data);
	set_connection_state(conn, S_TRANS);
}


static void
receive_gopher_response(struct connection *conn)
{
	struct read_buffer *rb = alloc_read_buffer(conn);

	if (!rb) return;

	set_connection_timeout(conn);

	rb->close = 1;
	read_from_socket(conn, &conn->socket, rb, read_gopher_response_data);
}

static void
send_gopher_command(struct connection *conn)
{
	struct gopher_connection_info *gopher = conn->info;

	write_to_socket(conn, &conn->socket,
			gopher->command, gopher->commandlen,
			receive_gopher_response);

	set_connection_state(conn, S_SENT);
}


/* FIXME: No decoding of strange data types as yet. */
void
gopher_protocol_handler(struct connection *conn)
{
	struct uri *uri = conn->uri;
	enum connection_state state = S_CONN;

	switch (get_uri_port(uri)) {
	case 105:
		/* If it's a port 105 GOPHER_CSO gopher_entity with no ISINDEX
		 * token ('?'), use the form-based CSO gateway (otherwise,
		 * return an ISINDEX cover page or do the ISINDEX search).
		 * - FM */
		if (uri->datalen == 1 && *uri->data == GOPHER_CSO) {
			/* FIXME: redirect_cache() */
			end_gopher_connection(conn, S_GOPHER_CSO_ERROR);
		}
		break;

	case 79:
#if 0
		/* This is outcommented because it apparently means that the
		 * finger protocol handler needs to be extended for handling
		 * this the way Lynx does. --jonas */
		/* If it's a port 79/0[/...] URL, use the finger gateway.
		 * - FM */
		if (uri->datalen >= 1 && *uri->data == GOPHER_FILE) {
			/* FIXME: redirect_cache() */
			end_gopher_connection(conn, S_OK);
		}
		break;
#endif
	default:
		state = init_gopher_connection_info(conn);
		if (state != S_CONN) {
			/* FIXME: Handle bad selector ... */
			end_gopher_connection(conn, state);
			return;
		}

		/* Set up a socket to the server for the data */
		set_connection_timeout(conn);
		conn->from = 0;
		make_connection(conn, &conn->socket, send_gopher_command);
	}
}