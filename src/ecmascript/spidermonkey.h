/* $Id: spidermonkey.h,v 1.3 2004/09/23 16:23:27 pasky Exp $ */

#ifndef EL__ECMASCRIPT_SPIDERMONKEY_H
#define EL__ECMASCRIPT_SPIDERMONKEY_H

struct ecmascript_interpreter;
struct string;

void spidermonkey_init();
void spidermonkey_done();

void *spidermonkey_get_interpreter(struct ecmascript_interpreter *interpreter);
void spidermonkey_put_interpreter(struct ecmascript_interpreter *interpreter);

void spidermonkey_eval(struct ecmascript_interpreter *interpreter, struct string *code);
unsigned char *spidermonkey_eval_stringback(struct ecmascript_interpreter *interpreter, struct string *code);

#endif
