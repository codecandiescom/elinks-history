/* Parser of HTTP headers */
/* $Id: header.c,v 1.11 2003/07/27 17:43:51 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "protocol/http/header.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"

unsigned char *
parse_http_header(unsigned char *head, unsigned char *item,
		  unsigned char **ptr)
{
	unsigned char *i, *f, *g, *h = NULL;

	if (!head) return NULL;

	for (f = head; *f; f++) {
		if (*f != '\n') continue;

		f++;
		for (i = item; *i && *f; i++, f++)
			if (upcase(*i) != upcase(*f)) goto cont;
		if (!*f) break;

		if (f[0] == ':') {
			while (f[1] == ' ') f++;
			for (g = ++f; *g >= ' '; g++);
			while (g > f && g[-1] == ' ') g--;

			if (h) mem_free(h);
			h = mem_alloc(g - f + 1);

			if (h) {
				memcpy(h, f, g - f);
				h[g - f] = '\0';
				if (ptr) {
					*ptr = f;
					break;
				}
				return h;
			}
		}

cont:;
		f--;
	}
	return h;
}

/* Extract the value of name part of the value of attribute content.
 * Ie. @name = "charset" and @str = "text/html; charset=iso-8859-1"
 * will return allocated string containing "iso-8859-1".
 * It supposes that separator is ';' and ignore first element in the
 * list. (ie. '1' is ignored in "1; URL=xxx") */
/* FIXME: rename it, any idea ? --Zas */
unsigned char *
parse_http_header_param(unsigned char *str, unsigned char *name)
{
	register unsigned char *p = str;
	int namelen, plen = 0;
	
	assert(str && *str && name && *name);
	if_assert_failed return NULL;
	
	namelen = strlen(name);
	do {
		p = strchr(p, ';');
		if (!p) return NULL;

		while (*p && (*p == ';' || *p <= ' ')) p++;
		if (strlen(p) < namelen) return NULL;
	} while (strncasecmp(p, name, namelen));

	p += namelen;
	
	while (*p && (*p <= ' ' || *p == '=')) p++;
	if (!*p) return stracpy("");
	
	while (p[plen] >= ' ' && p[plen] != ';') plen++;
	
	return memacpy(p, plen);
}

/* Parse string param="value", return value as new string or NULL if any
 * error. */
unsigned char *
get_http_header_param(unsigned char *e, unsigned char *name)
{
	unsigned char *n, *start;
	int i = 0;

again:
	while (*e && upcase(*e++) != upcase(*name));
	if (!*e) return NULL;

	n = name + 1;
	while (*n && upcase(*e) == upcase(*n)) e++, n++;
	if (*n) goto again;

	while (WHITECHAR(*e)) e++;
	if (*e++ != '=') return NULL;

	while (WHITECHAR(*e)) e++;
	start = e;

	if (!IS_QUOTE(*e)) {
		while (*e && !WHITECHAR(*e)) e++;
	} else {
		unsigned char uu = *e++;

		start++;
		while (*e != uu) {
			if (!*e) return NULL;
			e++;
		}
	}

	while (start < e && *start == ' ') start++;
	while (start < e && *(e - 1) == ' ') e--;
	if (start == e) return NULL;

	n = mem_alloc(e - start + 1);
	if (!n) return NULL;

	while (start < e) {
		if (*start < ' ') n[i] = '.';
		else n[i] = *start;
		i++; start++;
	}
	n[i] = '\0';

	return n;
}
