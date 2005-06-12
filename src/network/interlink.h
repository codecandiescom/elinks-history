/* $Id: interlink.h,v 1.8 2005/06/12 22:22:45 jonas Exp $ */

#ifndef EL__NETWORL_INTERLINK_H
#define EL__NETWORK_INTERLINK_H

#ifdef CONFIG_INTERLINK
int init_interlink(void);
void done_interlink(void);
#else
#define init_interlink() (-1)
#define done_interlink()
#endif

#endif
