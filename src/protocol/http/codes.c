/* HTTP response codes */
/* $Id: codes.c,v 1.18 2004/02/20 16:03:15 jonas Exp $ */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* Needed for asprintf() */
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "cache/cache.h"
#include "intl/gettext/libintl.h"
#include "protocol/http/codes.h"
#include "sched/connection.h"
#include "sched/session.h"
#include "sched/task.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/snprintf.h"
#include "viewer/text/view.h"


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


/* TODO: Some short intermediate document for the 3xx messages? --pasky */
static unsigned char *
get_http_error_document(struct terminal *term, int code)
{
	unsigned char *codestr = http_code_to_string(code);

	if (!codestr) codestr = "Unknown error";

	/* TODO: l10n this but without all the HTML code. --jonas */
	return asprintfa(
"<html>\n"
" <head>\n"
"  <title>HTTP error %03d</title>\n"
" </head>\n"
" <body>\n"
"  <h1 align=\"left\">HTTP error %03d: %s</h1>\n"
#ifndef ELINKS_SMALL
"  \n"
"  <center><hr /></center>\n"
"  \n"
"  <p>An error occurred on the server while fetching the document you\n"
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
#endif
" </body>\n"
"</html>\n",
			code, code, codestr);
}

struct http_error_info {
	int code;
	unsigned char url[1];
};

static void
show_http_error_document(struct session *ses, void *data)
{
	struct http_error_info *info = data;
	struct terminal *term = ses->tab->term;
	struct cache_entry *cached = find_in_cache(info->url);
	struct cache_entry *cache = cached ? cached : get_cache_entry(info->url);
	unsigned char *str = NULL;

	if (cache) str = get_http_error_document(term, info->code);

	if (str) {
		if (cached) delete_entry_content(cache);
		if (cache->head) mem_free(cache->head);
		cache->head = stracpy("\r\nContent-type: text/html\r\n");
		add_fragment(cache, 0, str, strlen(str));
		mem_free(str);

		draw_formatted(ses, 1);
	}

	mem_free(info);
}


void
http_error_document(struct connection *conn, int code)
{
	struct http_error_info *info;
	int urllen;

	assert(conn && struri(conn->uri));

	urllen = strlen(struri(conn->uri));

	info = mem_calloc(1, sizeof(struct http_error_info) + urllen);
	if (!info) return;

	info->code = code;
	memcpy(info->url, struri(conn->uri), urllen);

	add_questions_entry(show_http_error_document, info);
}
