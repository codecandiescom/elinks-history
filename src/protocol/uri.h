/* $Id: uri.h,v 1.88 2004/04/07 19:34:51 jonas Exp $ */

#ifndef EL__PROTOCOL_URI_H
#define EL__PROTOCOL_URI_H

#include "util/object.h"

struct string;

#define POST_CHAR 1

/* The uri structure is used to store the start position and length of commonly
 * used uri fields. It is initialized by parse_uri(). It is possible that the
 * start of a field is set but that the length is zero so instead of testing
 * *uri-><fieldname> always use uri-><fieldname>len. */
/* XXX: Lots of places in the code assume that the string members point into
 * the same string. That means if you need to use a NUL terminated uri field
 * either temporary modify the string, allocated a copy or change the function
 * used to take a length parameter (in the reverse precedence - modifying the
 * string should not be done since you never know what kind of memory actually
 * contains the string --pasky). */
/* TODO: We should probably add path+query members instead of data. */
/* TODO: Use struct string fields for the {struct uri} members? --pasky */

struct uri {
	/* The start of the uri (and thus start of the protocol string). */
	unsigned char *protocol_str;

	/* The internal type of protocol. Can _never_ be PROTOCOL_UNKNOWN. */
	int protocol; /* enum protocol */

	unsigned char *user;
	unsigned char *password;
	unsigned char *host;
	unsigned char *port;
	/* @data can contain both the path and query uri fields.
	 * It can never be NULL but can have zero length. */
	unsigned char *data;
	unsigned char *post;

	/* @protocollen should only be usable if @protocol is either
	 * PROTOCOL_USER or an uri string should be composed. */
	int protocollen:16;
	int userlen:16;
	int passwordlen:16;
	int hostlen:16;
	int portlen:16;
	int datalen:16;

	/* @post can contain some special encoded form data, used internally
	 * to make form data handling more efficient. The data is marked by
	 * POST_CHAR in the uri string. */

	/* Usage count object. */
	struct object object;
};

enum uri_errno {
	URI_ERRNO_OK,			/* Parsing went well */
	URI_ERRNO_EMPTY,		/* The URI string was empty */
	URI_ERRNO_INVALID_PROTOCOL,	/* No protocol was found */
	URI_ERRNO_NO_SLASHES,		/* Slashes after protocol missing */
	URI_ERRNO_NO_HOST_SLASH,	/* Slash after host missing */
	URI_ERRNO_IPV6_SECURITY,	/* IPv6 security bug detected */
	URI_ERRNO_INVALID_PORT,		/* Port number is bad */
	URI_ERRNO_INVALID_PORT_RANGE,	/* Port number is not within 0-65535 */
};

/* Initializes the members of the uri struct, as they are encountered.
 * If an uri component is recognized both it's length and starting point is
 * set. */
/* Returns what error was encountered or URI_ERRNO_OK if parsing went well. */
enum uri_errno parse_uri(struct uri *uri, unsigned char *uristring);


/* Returns the raw zero-terminated URI string the (struct uri) is associated
 * with. Thus, chances are high that it is the original URI received, not any
 * cheap reconstruction. */
#define struri(uri) ((uri)->protocol_str)


enum uri_component {
	URI_PROTOCOL		= (1 << 0),
	URI_USER		= (1 << 1),
	URI_PASSWORD		= (1 << 2),
	URI_HOST		= (1 << 3),
	URI_PORT		= (1 << 4),
	URI_DATA		= (1 << 5),
	URI_POST		= (1 << 6),
};


/* A small URI struct cache to increase reusability. */
/* XXX: Now there are a few rules to abide.
 *
 * Any URI string that should be registered in the cache has to have lowercased
 * both the protocol and hostname parts. This is strictly checked and will
 * otherwise cause an assertion failure.
 *
 * However this will not be a problem if you either first call join_urls()
 * which you want to do anyway to resolve relative references or use the
 * get_translated_uri() interface.
 *
 * The remaining support for RFC 2391 section 3.1 is done through get_protocol()
 * and get_user_program() which will treat upper case letters
 * as equivalent to lower case in protocol names. */

/* Register a new URI in the cache. If @length is -1 strlen(@string) is used as
 * the length. */
struct uri *get_uri(unsigned char *string, int length);

/* Dereference an URI from the cache */
void done_uri(struct uri *uri);

/* Take a reference of an URI already registered in the cache. */
static inline struct uri *
get_uri_reference(struct uri *uri)
{
	object_lock(uri);
	return uri;
}

#define get_proxied_uri(uri)					\
	(((uri)->protocol == PROTOCOL_PROXY)			\
	? get_uri((uri)->data, -1) : get_uri_reference(uri))

#define get_proxy_uri(uri)					\
	(((uri)->protocol != PROTOCOL_PROXY)			\
	? get_proxy(uri) : get_uri_reference(uri))

/* Resolves an URI relative to a current working directory (CWD) and possibly
 * extracts the fragment. It is possible to just use it to extract fragment
 * and get the resulting URI from the cache.
 * @uristring	is the URI to resolve or translate.
 * @cwd		if non NULL @uristring will be translated using this CWD.
 * @fragment	if non NULL fragment will be extracted and and allocated to the
 *		@fragment pointer. */
struct uri *get_translated_uri(unsigned char *uristring, unsigned char *cwd,
				unsigned char **fragment);

/* These functions recreate the URI string part by part. */
/* The @components bitmask describes the set of URI components used for
 * construction of the URI string. */

/* Adds the components to an already initialized string. */
struct string *add_uri_to_string(struct string *string, struct uri *uri, enum uri_component components);

/* Takes an uri string, parses it and adds the desired components. Useful if
 * there is no struct uri around. */
struct string *add_string_uri_to_string(struct string *string, unsigned char *uristring, enum uri_component components);

/* Extracts strictly the filename part (the crap between path and query) and
 * adds it to the @string. Note that there are cases where the string will be
 * empty ("") (ie. http://example.com/?crash=elinks). */
struct string *add_uri_filename_to_string(struct string *string, struct uri *uri);

/* Returns the new URI string or NULL upon an error. */
unsigned char *get_uri_string(struct uri *uri, enum uri_component components);

/* Returns either the uri's port number if available or the protocol's
 * default port. It is zarro for user protocols. */
int get_uri_port(struct uri *uri);


void encode_uri_string(struct string *, unsigned char *);
void decode_uri_string(unsigned char *);


/* Returns allocated string containing the biggest possible extension.
 * If url is 'jabadaba.1.foo.gz' the returned extension is '1.foo.gz' */
unsigned char *get_extension_from_uri(struct uri *uri);


unsigned char *join_urls(unsigned char *, unsigned char *);

/* Return position if end of string @s matches a known tld or -1 if not.
 * If @slen < 0, then string length will be obtained by a strlen() call,
 * else @slen is used as @s length. */
int end_with_known_tld(unsigned char *s, int slen);

/* Returns the length of url, without post data. */
int get_no_post_url_length(unsigned char *url);

/* Return an allocated string containing url without postdata.
 * If @url_len is non-NULL, then *url_len is set to length of new string. */
unsigned char *get_no_post_url(unsigned char *url, int *url_len);


static inline int
end_of_dir(unsigned char c)
{
	return c == POST_CHAR || c == '#' || c == ';' || c == '?';
}

#endif
