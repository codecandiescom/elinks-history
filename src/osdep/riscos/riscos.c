/* RISC OS system-specific routines. */
/* $Id: riscos.c,v 1.4 2003/10/28 00:17:49 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "osdep/system.h"

#ifdef RISCOS

#include "elinks.h"

#include "osdep/riscos/riscos.h"
#include "osdep/osdep.h"


int
is_xterm(void)
{
       return 1;
}

#endif
