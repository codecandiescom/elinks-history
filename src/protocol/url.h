/* $Id: url.h,v 1.12 2003/01/05 16:48:16 pasky Exp $ */

#ifndef EL__PROTOCOL_URL_H
#define EL__PROTOCOL_URL_H

#include "sched/sched.h"
#include "sched/session.h"

#define POST_CHAR 1

static inline int end_of_dir(unsigned char c)
{
	return c == POST_CHAR || c == '#' || c == ';' || c == '?';
}

unsigned char *get_protocol_name(unsigned char *);
unsigned char *get_host_name(unsigned char *);
unsigned char *get_host_and_pass(unsigned char *, int);
unsigned char *get_user_name(unsigned char *);
unsigned char *get_pass(unsigned char *);
unsigned char *get_port_str(unsigned char *);
int get_port(unsigned char *);
void (*get_protocol_handle(unsigned char *))(struct connection *);
void (*get_external_protocol_function(unsigned char *))(struct session *, unsigned char *);
unsigned char *get_url_data(unsigned char *);
unsigned char *strip_url_password(unsigned char *);
unsigned char *join_urls(unsigned char *, unsigned char *);
unsigned char *translate_url(unsigned char *, unsigned char *);
unsigned char *extract_position(unsigned char *);
unsigned char *extract_proxy(unsigned char *);
void get_filename_from_url(unsigned char *, unsigned char **, int *);
void get_filenamepart_from_url(unsigned char *, unsigned char **, int *);

void encode_url_string(unsigned char *, unsigned char **, int *);
void decode_url_string(unsigned char *);

#endif
