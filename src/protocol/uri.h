/* $Id: uri.h,v 1.2 2003/07/01 16:27:10 jonas Exp $ */

#ifndef EL__PROTOCOL_URI_H
#define EL__PROTOCOL_URI_H

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

#endif
