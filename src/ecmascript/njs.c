/* The njs ECMAScript backend. */
/* $Id: njs.c,v 1.1 2004/09/21 22:11:43 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <js.h>
#include <stdlib.h>

#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/njs.h"


void *
njs_get_interpreter(struct ecmascript_interpreter *interpreter)
{
	JSInterpPtr jsint;
	JSInterpOptions jsopts;

	assert(interpreter);

	jsint = js_create_interp(NULL);
	if (!jsint)
		return NULL;
	interpreter->backend_data = jsint;

	js_get_options(jsint, &jsopts);
	jsopts.secure_builtin_file = 1;
	jsopts.secure_builtin_system = 1;
	js_set_options(jsint, &jsopts);

	return jsint;
}

void
njs_put_interpreter(struct ecmascript_interpreter *interpreter)
{
	JSInterpPtr jsint;

	assert(interpreter);
	jsint = interpreter->backend_data;
	js_destroy_interp(jsint);
}
