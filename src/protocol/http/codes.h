/* $Id: codes.h,v 1.3 2003/06/21 14:30:05 pasky Exp $ */

#ifndef EL__PROTOCOL_HTTP_CODES_H
#define EL__PROTOCOL_HTTP_CODES_H

/* HTTP response codes device. */

unsigned char *http_code_to_string(int code);
unsigned char *http_error_document(int code);

#endif /* EL__PROTOCOL_HTTP_CODES_H */
