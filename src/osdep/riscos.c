/* RISC OS system-specific routines. */
/* $Id: riscos.c,v 1.3 2003/10/27 02:44:45 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "osdep/system.h"

#ifdef RISCOS

#include "elinks.h"


int
is_xterm(void)
{
       return 1;
}

#endif
