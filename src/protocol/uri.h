/* $Id: uri.h,v 1.4 2003/07/08 02:25:40 jonas Exp $ */

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
int parse_uri(struct uri *uri);

/* Returns either the uri's port number if available or the protocol's
 * default port. For user protocols this is 0. */
int get_uri_port(struct uri *uri);

#endif
