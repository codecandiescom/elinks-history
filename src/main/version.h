/* $Id: version.h,v 1.4 2003/06/08 11:38:41 pasky Exp $ */

#ifndef EL__UTIL_VERSION_H
#define EL__UTIL_VERSION_H

struct terminal;

unsigned char *get_version();
unsigned char *get_dyn_full_version(struct terminal *term, int more);
void init_static_version();

extern unsigned char full_static_version[];

#endif /* EL__UTIL_VERSION_H */
