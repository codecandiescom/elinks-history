/* $Id: os2.h,v 1.6 2003/10/27 23:40:24 pasky Exp $ */

#ifndef EL__OSDEP_OS2_H
#define EL__OSDEP_OS2_H

#ifdef OS2

struct terminal;

void open_in_new_vio(struct terminal *term, unsigned char *exe_name,
		     unsigned char *param);
void open_in_new_fullscreen(struct terminal *term, unsigned char *exe_name,
			    unsigned char *param);

#endif

#endif
