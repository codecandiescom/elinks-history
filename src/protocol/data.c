/* The "data" URI protocol implementation (RFC 2397) */
/* $Id: data.c,v 1.1 2004/08/14 03:32:32 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "cache/cache.h"
#include "protocol/data.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "sched/connection.h"
#include "util/base64.h"
#include "util/string.h"

/* The URLs are of the form:
 *
 *	data:[<mediatype>][;base64],<data>
 *
 * The <mediatype> is an Internet media type specification (with optional
 * parameters.) The appearance of ";base64" means that the data is encoded as
 * base64. Without ";base64", the data (as a sequence of octets) is represented
 * using ASCII encoding for octets inside the range of safe URL characters and
 * using the standard %xx hex encoding of URLs for octets outside that range.
 * If <mediatype> is omitted, it defaults to "text/plain;charset=US-ASCII".  As a
 * shorthand, "text/plain" can be omitted but the charset parameter supplied.
 *
 * The syntax:
 *
 *	dataurl	  := "data:" [ mediatype ] [ ";base64" ] "," data
 *	mediatype := [ type "/" subtype ] *( ";" parameter )
 *	data	  := *urlchar
 *	parameter := attribute "=" value
 *
 * where "urlchar" is imported from [RFC2396], and "type", "subtype",
 * "attribute" and "value" are the corresponding tokens from [RFC2045],
 * represented using URL escaped encoding of [RFC2396] as necessary.
 *
 * Attribute values in [RFC2045] are allowed to be either represented as tokens
 * or as quoted strings. However, within a "data" URL, the "quoted-string"
 * representation would be awkward, since the quote mark is itself not a valid
 * urlchar. For this reason, parameter values should use the URL Escaped
 * encoding instead of quoted string if the parameter values contain any
 * "tspecial".
 *
 * The ";base64" extension is distinguishable from a content-type parameter by
 * the fact that it doesn't have a following "=" sign. */

#define data_has_base64_attribute(typelen, endstr) \
	((typelen) >= sizeof(";base64") - 1 \
	 && !memcmp(";base64", (end) - sizeof(";base64") + 1, sizeof(";base64") - 1))

static unsigned char *
init_data_protocol_header(struct cache_entry *cached,
			  unsigned char *type, int typelen)
{
	unsigned char *head;

	type = memacpy(type, typelen);
	if (!type) return NULL;

	/* Set fake content type */
	head = straconcat("\r\nContent-Type: ",  type, "\r\n", NULL);
	mem_free(type);
	if (!head) return NULL;

	mem_free_set(&cached->head, head);
	return head;
}

static unsigned char *
parse_data_uri_type(struct connection *conn, int *base64)
{
	struct uri *uri = conn->uri;
	unsigned char *end = memchr(uri->data, ',', uri->datalen);
	unsigned char *type;
	int typelen = 0;

	if (end) {
		type	= uri->data;
		typelen	= end - type;

		if (data_has_base64_attribute(typelen, end)) {
			*base64 = 1;
			typelen -= sizeof(";base64") - 1;
		}
	}

	/* Either no ',' was found or the type was missing */
	if (!typelen) {
		type	= "text/plain;charset=US-ASCII";
		typelen	= strlen(type);
	}

	if (!init_data_protocol_header(conn->cached, type, typelen))
		return NULL;

	/* Return char after ',' or complete data part */
	return end ? end + 1 : uri->data;
}

void
data_protocol_handler(struct connection *conn)
{
	struct uri *uri = conn->uri;
	struct cache_entry *cached = get_cache_entry(uri);
	unsigned char *data_start, *data;
	int base64 = 0;

	if (!cached) {
		abort_conn_with_state(conn, S_OUT_OF_MEM);
		return;
	}

	conn->cached = cached;

	data_start = parse_data_uri_type(conn, &base64);
	if (!data_start) {
		abort_conn_with_state(conn, S_OUT_OF_MEM);
		return;
	}

	/* Allocate the data string because URI decoding will possibly modify
	 * it. */
	data = memacpy(data_start, uri->datalen - (data_start - uri->data));
	if (!data) {
		abort_conn_with_state(conn, S_OUT_OF_MEM);
		return;
	}

	if (base64) {
		unsigned char *decoded = base64_encode(data);

		if (!decoded) {
			abort_conn_with_state(conn, S_OUT_OF_MEM);
			return;
		}

		mem_free_set(&data, decoded);
	} else {
		decode_uri_string(data);
	}

	/* This will not release the newly created header */
	delete_entry_content(cached);

	/* Use strlen() to get the correct decoded length */
	add_fragment(cached, conn->from, data, strlen(data));

	mem_free(data);

	cached->incomplete = 0;

	abort_conn_with_state(conn, S_OK);
}