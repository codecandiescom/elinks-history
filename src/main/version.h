/* $Id: version.h,v 1.5 2003/06/18 01:55:24 jonas Exp $ */

#ifndef EL__UTIL_VERSION_H
#define EL__UTIL_VERSION_H

struct terminal;

unsigned char *get_version(void);
unsigned char *get_dyn_full_version(struct terminal *term, int more);
void init_static_version(void);

extern unsigned char full_static_version[];

#endif /* EL__UTIL_VERSION_H */
