/* Format conversion functions */
/* $Id: conv.c,v 1.1 2002/03/27 21:30:25 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <links.h>

#include <util/conv.h>


unsigned char hx(int a)
{
	return a >= 10 ? a + 'A' - 10 : a + '0';
}

int unhx(unsigned char a)
{
	if (a >= '0' && a <= '9') return a - '0';
	if (a >= 'A' && a <= 'F') return a - 'A' + 10;
	if (a >= 'a' && a <= 'f') return a - 'a' + 10;
	return -1;
}
