/* $Id: uri.h,v 1.8 2003/07/12 20:21:15 jonas Exp $ */

#ifndef EL__PROTOCOL_URI_H
#define EL__PROTOCOL_URI_H

#ifndef POST_CHAR
#define POST_CHAR 1
#endif

/* This is only some temporary look while doing the uri parsing cleanup. */
struct uri {
	unsigned char *protocol;
	int protocollen;

	unsigned char *user;
	int userlen;

	unsigned char *password;
	int passwordlen;

	unsigned char *host;
	int hostlen;

	unsigned char *port;
	int portlen;

	unsigned char *data;
	int datalen;

	unsigned char *post;
};

/* Expects that uri->protocol contains the uri string. */
/* Returns the length of the parsed uri or 0 if some error was found. */
int parse_uri(struct uri *uri, unsigned char *uristring);

/* Returns either the uri's port number if available or the protocol's
 * default port. For user protocols this is 0. */
int get_uri_port(struct uri *uri);

static inline int
end_of_dir(unsigned char c)
{
	return c == POST_CHAR || c == '#' || c == ';' || c == '?';
}

/* Returns a valid host URL (for http authentification) or NULL. */
unsigned char *get_uri_string(struct uri *uri);

void encode_uri_string(unsigned char *, unsigned char **, int *);
void decode_uri_string(unsigned char *);

#endif
