/* $Id: win32.h,v 1.5 2004/08/14 23:29:36 jonas Exp $ */

#ifndef EL__OSDEP_WIN32_WIN32_H
#define EL__OSDEP_WIN32_WIN32_H

#ifdef CONFIG_WIN32

struct terminal;

void open_in_new_win32(struct terminal *term, unsigned char *exe_name,
		       unsigned char *param);

#endif

#endif
