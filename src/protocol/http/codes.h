/* HTTP response codes
 * source: http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
 */
/* $Id: codes.h,v 1.2 2003/06/21 12:56:16 pasky Exp $ */

#ifndef EL__PROTOCOL_HTTP_CODES_H
#define EL__PROTOCOL_HTTP_CODES_H

unsigned char *http_code_to_string(int code);
unsigned char *http_error_document(int code);

#endif /* EL__PROTOCOL_HTTP_CODES_H */
