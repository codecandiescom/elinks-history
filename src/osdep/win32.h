/* $Id: win32.h,v 1.3 2003/10/27 23:40:24 pasky Exp $ */

#ifndef EL__OSDEP_WIN32_H
#define EL__OSDEP_WIN32_H

#ifdef WIN32

struct terminal;

void open_in_new_win32(struct terminal *term, unsigned char *exe_name,
		       unsigned char *param);

#endif

#endif
