/* HTTP response codes */
/* $Id: codes.c,v 1.9 2003/06/21 13:15:52 pasky Exp $ */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* Needed for asprintf() */
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "protocol/http/codes.h"
#include "util/snprintf.h"


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


unsigned char *
http_error_document(int code)
{
	unsigned char *str = NULL;
	unsigned char *codestr = http_code_to_string(code);

	if (!codestr) codestr = "Unknown error";

	if (asprintf(&str, 
"<html>\n"
" <head>\n"
"  <title>HTTP error %03d</title>\n"
" </head>\n"
" <body>\n"
"  <h1 align=\"left\">HTTP error %03d: %s</h1>\n"
"  \n"
"  <center><hr /></center>\n"
"  \n"
"  <p>An error occured on the server while fetching the document you\n"
"  requested. Moreover, the server did not send back any explanation of what\n"
"  happenned whatsoever - it would be nice if you notified the web server\n"
"  administrator about this, as it is not a nice behaviour from the web\n"
"  server at all and it frequently indicates some much deeper problem with\n"
"  the web server software.</p>\n"
"  \n"
"  <p>I have no idea about what is wrong, sorry. Please contact the web\n"
"  server administrator if you believe that this error should not occur.</p>\n"
"  \n"
"  <p align=\"right\">Have a nice day.</p>\n"
" </body>\n"
"</html>\n", code, code, codestr) < 0)
		return NULL;

	return str;
}
