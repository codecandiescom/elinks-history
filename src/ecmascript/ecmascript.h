/* $Id: ecmascript.h,v 1.6 2004/09/25 00:59:27 pasky Exp $ */

#ifndef EL__ECMASCRIPT_ECMASCRIPT_H
#define EL__ECMASCRIPT_ECMASCRIPT_H

/* This is a trivial ECMAScript driver. All your base are belong to pasky. */
/* In the future you will get DOM, a complete ECMAScript interface and free
 * plasm displays for everyone. */

#include "util/ttime.h"
#include "modules/module.h"

struct document_view;
struct string;
struct uri;
struct view_state;

struct ecmascript_interpreter {
	struct document_view *doc_view;
	void *backend_data;
	time_t exec_start;
};

/* Why is the interpreter bound to {struct document_view} instead of
 * {struct document}? That's easy, because the script won't raid just inside
 * of the document, but it will also want to generate pop-up boxes, adjust
 * form contents (which is doc_view-specific) etc. Of course the cons are that
 * we need to wait with any javascript code execution until we get
 * document_view - that means we are going to re-render the document if it
 * contains a <script> area full of document.write()s. */
struct ecmascript_interpreter *ecmascript_get_interpreter(struct document_view *doc_view);
void ecmascript_put_interpreter(struct ecmascript_interpreter *interpreter);

void ecmascript_cleanup_state(struct document_view *doc_view, struct view_state *vs);

void ecmascript_eval(struct ecmascript_interpreter *interpreter, struct string *code);
unsigned char *ecmascript_eval_stringback(struct ecmascript_interpreter *interpreter, struct string *code);

/* Takes line with the syntax javascript:<ecmascript code>. Activated when user
 * follows a link with this synstax. */
void ecmascript_protocol_handler(struct session *ses, struct uri *uri);

extern struct module ecmascript_module;

#endif
