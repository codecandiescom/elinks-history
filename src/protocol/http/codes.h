/* $Id: codes.h,v 1.4 2004/02/20 16:01:44 jonas Exp $ */

#ifndef EL__PROTOCOL_HTTP_CODES_H
#define EL__PROTOCOL_HTTP_CODES_H

struct connection;

/* HTTP response codes device. */

unsigned char *http_code_to_string(int code);
void http_error_document(struct connection *conn, int code);

#endif /* EL__PROTOCOL_HTTP_CODES_H */
