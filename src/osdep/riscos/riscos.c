/* RISC OS system-specific routines. */
/* $Id: riscos.c,v 1.1 2003/10/27 02:08:40 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef RISCOS

#include "elinks.h"

#include "osdep/os_depx.h"

int
is_xterm(void)
{
       return 1;
}

#endif
