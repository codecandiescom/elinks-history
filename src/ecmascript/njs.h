/* $Id: njs.h,v 1.1 2004/09/21 22:11:43 pasky Exp $ */

#ifndef EL__ECMASCRIPT_NJS_H
#define EL__ECMASCRIPT_NJS_H

struct ecmascript_interpreter;

void *njs_get_interpreter(struct ecmascript_interpreter *interpreter);
void njs_put_interpreter(struct ecmascript_interpreter *interpreter);

#endif
