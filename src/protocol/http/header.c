/* Parser of HTTP headers */
/* $Id: header.c,v 1.10 2002/12/21 19:58:20 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "protocol/http/header.h"
#include "util/conv.h"
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

unsigned char *
parse_http_header_param(unsigned char *x, unsigned char *e)
{
	int le = strlen(e);
	int lp;
	unsigned char *y = x;

again:
	y = strchr(y, ';');
	if (!y) return NULL;

	while (*y && (*y == ';' || *y <= ' ')) y++;
	if (strlen(y) < le) return NULL;

	if (strncasecmp(y, e, le)) goto again;

	y += le;

	while (*y && (*y <= ' ' || *y == '=')) y++;
	if (!*y) return stracpy("");

	lp = 0;
	while (y[lp] >= ' ' && y[lp] != ';') lp++;

	return memacpy(y, lp);
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
