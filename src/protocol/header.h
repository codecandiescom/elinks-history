/* $Id: header.h,v 1.2 2004/06/28 10:38:02 jonas Exp $ */

#ifndef EL__PROTOCOL_HEADER_H
#define EL__PROTOCOL_HEADER_H

unsigned char *parse_http_header(unsigned char *, unsigned char *, unsigned char **);
unsigned char *parse_http_header_param(unsigned char *, unsigned char *);
unsigned char *get_http_header_param(unsigned char *, unsigned char *);

#endif
