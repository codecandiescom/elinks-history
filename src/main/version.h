/* $Id: version.h,v 1.3 2003/05/20 22:09:16 zas Exp $ */

#ifndef EL__VERSION_H
#define EL__VERSION_H

unsigned char *get_version();
unsigned char *get_dyn_full_version(struct terminal *term, int more);
void init_static_version();

extern unsigned char full_static_version[];

#endif /* EL__VERSION_H */
