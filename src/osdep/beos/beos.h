/* $Id: beos.h,v 1.6 2003/10/27 23:42:32 pasky Exp $ */

#ifndef EL__OSDEP_BEOS_BEOS_H
#define EL__OSDEP_BEOS_BEOS_H

#ifdef BEOS

struct terminal;

void open_in_new_be(struct terminal *term, unsigned char *exe_name,
		    unsigned char *param);

#endif

#endif
