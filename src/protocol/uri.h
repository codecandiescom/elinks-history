/* $Id: uri.h,v 1.1 2003/07/01 15:22:39 jonas Exp $ */

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

#if 0
/* Expects that uri->protocol contains the uri string. */
/* Returns the length of the parsed uri or 0 if some error was found. */
int parse_uri(struct uri *uri);
#endif

int parse_uri(unsigned char *url, int *prlen,
		unsigned char **user, int *uslen,
		unsigned char **pass, int *palen,
		unsigned char **host, int *holen,
		unsigned char **port, int *polen,
		unsigned char **data, int *dalen,
		unsigned char **post);

#endif
