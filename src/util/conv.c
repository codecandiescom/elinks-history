/* Format conversion functions */
/* $Id: conv.c,v 1.2 2002/04/27 21:21:20 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>

#include <links.h>

#include <util/conv.h>


unsigned char
hx(int a)
{
	return a >= 10 ? a + 'A' - 10 : a + '0';
}

int
unhx(unsigned char a)
{
	if (a >= '0' && a <= '9') return a - '0';
	if (a >= 'A' && a <= 'F') return a - 'A' + 10;
	if (a >= 'a' && a <= 'f') return a - 'a' + 10;
	return -1;
}

/* Convert chars to html &#xx */
void
add_htmlesc_str(unsigned char **str, int *strl,
		unsigned char *ostr, int ostrl)
{

#define accept_char(x) ((upcase((x)) >= 'A' && upcase((x)) <= 'Z') ||\
		((x) >= '0' && (x) <= '9') || (x) == ' ' ||\
		(x) == '-' || (x) == '_' || (x) == '.' ||\
		(x) == ':' || (x) == ';')

	for (; ostrl; ostrl--, ostr++) {
		if (accept_char(*ostr)) {
			add_chr_to_str(str, strl, *ostr);
		} else {
			add_to_str(str, strl, "&#");
			add_num_to_str(str, strl, (int) *ostr);
			add_chr_to_str(str, strl, ';');
		}
	}

#undef accept_char

}
