/* $Id: spidermonkey.h,v 1.1 2004/09/22 15:22:34 pasky Exp $ */

#ifndef EL__ECMASCRIPT_SPIDERMONKEY_H
#define EL__ECMASCRIPT_SPIDERMONKEY_H

struct ecmascript_interpreter;

void spidermonkey_init();
void spidermonkey_done();

void *spidermonkey_get_interpreter(struct ecmascript_interpreter *interpreter);
void spidermonkey_put_interpreter(struct ecmascript_interpreter *interpreter);

#endif
