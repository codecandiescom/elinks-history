/* $Id: header.h,v 1.1 2004/05/21 11:38:05 jonas Exp $ */

#ifndef EL__PROTOCOL_HTTP_HEADER_H
#define EL__PROTOCOL_HTTP_HEADER_H

unsigned char *parse_http_header(unsigned char *, unsigned char *, unsigned char **);
unsigned char *parse_http_header_param(unsigned char *, unsigned char *);
unsigned char *get_http_header_param(unsigned char *, unsigned char *);

#endif
