/* The njs ECMAScript backend. */
/* $Id: njs.c,v 1.3 2004/09/22 10:42:56 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <js.h>
#include <stdlib.h>

#include "elinks.h"

#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/njs.h"
#include "protocol/uri.h"


/*** Global methods */


static JSMethodResult
global_decodeURI(void *ctx, JSInterpPtr jsint,
                 int argc, JSType *argv,
                 JSType *result_return, char *error_return)
{
	unsigned char *str;

	if (argc != 1) {
		strncpy(error_return, "Too many arguments.", 1024);
		return JS_ERROR;
	}
	if (argv[0].type != JS_TYPE_STRING) {
		strncpy(error_return, "Invalid argument type.", 1024);
		return JS_ERROR;
	}

	str = malloc(argv[0].u.s->len + 1);
	memcpy(str, argv[0].u.s->data, argv[0].u.s->len + 1);

	decode_uri_string(str);

	result_return->type = JS_TYPE_STRING;
	result_return->u.s->len = strlen(str);
	result_return->u.s->data = str;
	return JS_OK;
}

static JSMethodResult
global_encodeURI_do(void *ctx, JSInterpPtr jsint,
                    int argc, JSType *argv,
                    JSType *result_return, char *error_return,
                    int convert_slashes)
{
	struct string string = NULL_STRING;
	unsigned char *str;

	if (argc != 1) {
		strncpy(error_return, "Too many arguments.", 1024);
		return JS_ERROR;
	}
	if (argv[0].type != JS_TYPE_STRING) {
		strncpy(error_return, "Invalid argument type.", 1024);
		return JS_ERROR;
	}

	init_string(&string);
	str = memacpy(argv[0].u.s->data, argv[0].u.s->len);
	encode_uri_string(&string, str, convert_slashes);
	mem_free(str);

	str = malloc(string.length + 1);
	memcpy(str, string.source, string.length + 1);
	done_string(&string);

	result_return->type = JS_TYPE_STRING;
	result_return->u.s->len = strlen(str);
	result_return->u.s->data = str;
	return JS_OK;
}

static JSMethodResult
global_encodeURI(void *ctx, JSInterpPtr jsint,
                 int argc, JSType *argv,
                 JSType *result_return, char *error_return)
{
	return global_encodeURI_do(ctx, jsint, argc, argv,
	                           result_return, error_return, 0);
}

static JSMethodResult
global_encodeURIComponent(void *ctx, JSInterpPtr jsint,
                          int argc, JSType *argv,
                          JSType *result_return, char *error_return)
{
	return global_encodeURI_do(ctx, jsint, argc, argv,
	                           result_return, error_return, 1);
}




/*** The ELinks interface */

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

	js_create_global_method(jsint, "decodeURI", global_decodeURI, NULL, NULL);
	js_create_global_method(jsint, "decodeURIComponent", global_decodeURI, NULL, NULL);
	js_create_global_method(jsint, "encodeURI", global_encodeURI, NULL, NULL);
	js_create_global_method(jsint, "encodeURIComponent", global_encodeURIComponent, NULL, NULL);

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
