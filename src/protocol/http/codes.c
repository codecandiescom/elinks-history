/* HTTP response codes */
/* $Id: codes.c,v 1.3 2003/06/21 12:29:29 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "protocol/http/codes.h"


struct http_code {
	int num;
	unsigned char *str;
};

/* Source: http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html */
static struct http_code http_code[] = {
	{ 100, "Continue" },
	{ 101, "Switching Protocols" },
	{ 200, "OK" },
	{ 201, "Created" },
	{ 202, "Accepted" },
	{ 203, "Non-Authoritative Information" },
	{ 204, "No Content" },
	{ 205, "Reset Content" },
	{ 206, "Partial Content" },
	{ 300, "Multiple Choices" },
	{ 301, "Moved Permanently" },
	{ 302, "Found" },
	{ 303, "See Other" },
	{ 304, "Not Modified" },
	{ 305, "Use Proxy" },
	{ 306, "(Unused)" },
	{ 307, "Temporary Redirect" },
	{ 400, "Bad Request" },
	{ 401, "Unauthorized" },
	{ 402, "Payment Required" },
	{ 403, "Forbidden" },
	{ 404, "Not Found" },
	{ 405, "Method Not Allowed" },
	{ 406, "Not Acceptable" },
	{ 407, "Proxy Authentication Required" },
	{ 408, "Request Timeout" },
	{ 409, "Conflict" },
	{ 410, "Gone" },
	{ 411, "Length Required" },
	{ 412, "Precondition Failed" },
	{ 413, "Request Entity Too Large" },
	{ 414, "Request-URI Too Long" },
	{ 415, "Unsupported Media Type" },
	{ 416, "Requested Range Not Satisfiable" },
	{ 417, "Expectation Failed" },
	{ 500, "Internal Server Error" },
	{ 501, "Not Implemented" },
	{ 502, "Bad Gateway" },
	{ 503, "Service Unavailable" },
	{ 504, "Gateway Timeout" },
	{ 505, "HTTP Version Not Supported" },
};

#define count(T) (sizeof(T)/sizeof(*(T)))

unsigned char *
http_code_to_string(int code)
{
	int start = 0;
	int end = count(http_code) - 1; /* can be negative. */

	/* Dichotomic search is used there. */
	while (start <= end) {
		int middle = (start + end) >> 1;

		if (http_code[middle].num == code)
			return http_code[middle].str;
		else if (http_code[middle].num > code)
			end = middle - 1;
		else
			start = middle + 1;
	}

	return NULL;
}
