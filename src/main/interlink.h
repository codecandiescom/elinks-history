/* $Id: interlink.h,v 1.9 2005/06/13 00:52:14 jonas Exp $ */

#ifndef EL__MAIN_INTERLINK_H
#define EL__MAIN_INTERLINK_H

#ifdef CONFIG_INTERLINK
int init_interlink(void);
void done_interlink(void);
#else
#define init_interlink() (-1)
#define done_interlink()
#endif

#endif
