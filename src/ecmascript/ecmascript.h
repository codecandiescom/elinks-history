/* $Id: ecmascript.h,v 1.7 2004/09/26 09:56:55 pasky Exp $ */

#ifndef EL__ECMASCRIPT_ECMASCRIPT_H
#define EL__ECMASCRIPT_ECMASCRIPT_H

/* This is a trivial ECMAScript driver. All your base are belong to pasky. */
/* In the future you will get DOM, a complete ECMAScript interface and free
 * plasm displays for everyone. */

#include "util/ttime.h"
#include "modules/module.h"

struct string;
struct uri;
struct view_state;

struct ecmascript_interpreter {
	struct view_state *vs;
	void *backend_data;
	time_t exec_start;
};

/* Why is the interpreter bound to {struct view_state} instead of {struct
 * document}? That's easy, because the script won't raid just inside of the
 * document, but it will also want to generate pop-up boxes, adjust form
 * contents (which is doc_view-specific) etc. Of course the cons are that we
 * need to wait with any javascript code execution until we get bound to the
 * view_state through document_view - that means we are going to re-render the
 * document if it contains a <script> area full of document.write()s. And why
 * not bound the interpreter to {struct document_view} then? Because it is
 * reset for each rerendering, and it sucks to do all the magic to preserve the
 * interpreter over the rerenderings (we tried). */

struct ecmascript_interpreter *ecmascript_get_interpreter(struct view_state*vs);
void ecmascript_put_interpreter(struct ecmascript_interpreter *interpreter);

void ecmascript_reset_state(struct view_state *vs);

void ecmascript_eval(struct ecmascript_interpreter *interpreter, struct string *code);
unsigned char *ecmascript_eval_stringback(struct ecmascript_interpreter *interpreter, struct string *code);

/* Takes line with the syntax javascript:<ecmascript code>. Activated when user
 * follows a link with this synstax. */
void ecmascript_protocol_handler(struct session *ses, struct uri *uri);

extern struct module ecmascript_module;

#endif
