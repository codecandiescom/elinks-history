/* $Id: version.h,v 1.1 2003/05/19 14:12:30 zas Exp $ */

#ifndef EL__VERSION_H
#define EL__VERSION_H

unsigned char *get_version();
unsigned char *get_dyn_full_version(struct terminal *term);
void init_static_version();

unsigned char full_static_version[1024];

#endif /* EL__VERSION_H */
