/* Parser of HTTP headers */
/* $Id: header.c,v 1.11 2004/07/04 13:06:05 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "protocol/header.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"

unsigned char *
parse_header(unsigned char *head, unsigned char *item, unsigned char **ptr)
{
	unsigned char *pos;

	if (!head) return NULL;

	for (pos = head; *pos; pos++) {
		/* Go for a newline. */
		if (*pos != '\n') continue;

		pos++;

		/* Start of line now. */

		{
			unsigned char *itempos;

			for (itempos = item; *itempos && *pos; itempos++, pos++)
				if (toupper(*itempos) != toupper(*pos))
					goto cont;
		}

		if (!*pos) break;

		if (pos[0] == ':') {
			unsigned char *value, *valend;

			/* Strip ':' and leading whitespace */
			do pos++; while (pos[0] == ' ');

			/* Find the end of line/string */
			for (valend = pos; *valend >= ' '; valend++);

			/* Strip trailing whitespace */
			while (valend > pos && valend[-1] == ' ') valend--;

			value = memacpy(pos, valend - pos);
			if (!value) goto cont;

			if (ptr) *ptr = pos;
			return value;
		}

cont:
		/* We could've hit a newline at this point, so keep the chance
		 * to check for it in the next iteration. */
		pos--;
	}

	return NULL;
}

/* Extract the value of name part of the value of attribute content.
 * Ie. @name = "charset" and @str = "text/html; charset=iso-8859-1"
 * will return allocated string containing "iso-8859-1".
 * It supposes that separator is ';' and ignore first element in the
 * list. (ie. '1' is ignored in "1; URL=xxx") */
unsigned char *
parse_header_param(unsigned char *str, unsigned char *name)
{
	unsigned char *p = str;
	int namelen, plen = 0;

	assert(str && name && *name);
	if_assert_failed return NULL;

	/* Returns now if string @str is empty. */
	if (!*p) return NULL;

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

	/* Trim ending spaces */
	while (plen > 0 && p[plen - 1] == ' ') plen--;

	return memacpy(p, plen);
}

/* Parse string param="value", return value as new string or NULL if any
 * error. */
unsigned char *
get_header_param(unsigned char *e, unsigned char *name)
{
	unsigned char *n, *start;

again:
	while (*e && toupper(*e++) != toupper(*name));
	if (!*e) return NULL;

	n = name + 1;
	while (*n && toupper(*e) == toupper(*n)) e++, n++;
	if (*n) goto again;

	skip_space(e);
	if (*e++ != '=') return NULL;

	skip_space(e);
	start = e;

	if (!isquote(*e)) {
		skip_nonspace(e);
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
	if (n) {
		int i = 0;

		while (start < e) {
			n[i++] = (*start < ' ') ? '.' : *start;
			start++;
		}
		n[i] = '\0';
	}

	return n;
}
