/* OS/2 support fo ELinks. It has pretty different life than rest of ELinks. */
/* $Id: os2.c,v 1.1 2003/10/27 01:05:44 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef OS2

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "elinks.h"

#include "osdep/os_depx.h"



# endif
