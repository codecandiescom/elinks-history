/* Math stuff */
/* $Id: math.c,v 1.1 2004/12/20 23:51:25 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "util/math.h" /* Well, yes. But then again, who knows. */
#include "util/types.h"


/* Computers are useless.  They can only give you answers.
 * -- Pablo Picasso */


/* Give us the best you can. */
/* We use the longlong declaration here so that we get to know when someone
 * changes it to something different that math.h knows - we want them to
 * stay same. */
longlong swap_register_;
