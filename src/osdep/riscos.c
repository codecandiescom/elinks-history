/* RISC OS system-specific routines. */
/* $Id: riscos.c,v 1.2 2003/10/27 02:21:48 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#ifdef RISCOS


int
is_xterm(void)
{
       return 1;
}

#endif
