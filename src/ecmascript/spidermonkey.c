/* The SpiderMonkey ECMAScript backend. */
/* $Id: spidermonkey.c,v 1.8 2004/09/22 23:14:51 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* For wild SpiderMonkey installations. */
#ifdef CONFIG_BEOS
#define XP_BEOS
#elif CONFIG_OS2
#define XP_OS2
#elif CONFIG_RISCOS
#error Out of luck, buddy!
#elif CONFIG_UNIX
#define XP_UNIX
#elif CONFIG_WIN32
#define XP_WIN
#endif

#include <jsapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/document.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/spidermonkey.h"
#include "protocol/uri.h"
#include "util/string.h"


/*** Global methods */


/* TODO? Are there any which need to be implemented? */



/*** Classes */

enum prop_type {
	JSPT_UNDEF,
	JSPT_INT,
	JSPT_DOUBLE,
	JSPT_STRING,
	JSPT_ASTRING,
	JSPT_BOOLEAN,
};

#define VALUE_TO_JSVAL_START \
	enum prop_type prop_type; \
	unsigned char *string = NULL; \
 \
	if (!JSVAL_IS_INT(id)) \
		goto bye;

#define VALUE_TO_JSVAL_END \
	value_to_jsval(ctx, vp, prop_type, string); \
 \
bye: \
	return JS_TRUE;

static void
value_to_jsval(JSContext *ctx, jsval *vp, enum prop_type prop_type,
               unsigned char *string)
{
	switch (prop_type) {
	case JSPT_STRING:
	case JSPT_ASTRING:
		if (string) {
			*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(ctx, string));
		} else {
			*vp = JSVAL_NULL;
		}
		if (prop_type == JSPT_ASTRING)
			mem_free(string);
		break;

	case JSPT_UNDEF:
	default:
		*vp = JSVAL_NULL;
		break;
	}
}

#define JSVAL_TO_VALUE_START \
	jsint boolean = 0; \
	jsdouble *number = NULL; \
	unsigned char *string = NULL; \
 \
	/* Prevent "Unused variable" warnings. */ \
	if (!JSVAL_IS_INT(id) || boolean || number || string) \
		goto bye;

#define JSVAL_REQUIRE(vp, type, var) \
	jsval_to_value(ctx, vp, JSTYPE_ ## type, &var);

#define JSVAL_TO_VALUE_END \
bye: \
	return JS_TRUE;

static void
jsval_to_value(JSContext *ctx, jsval *vp, JSType type, void *var)
{
	jsint *boolean = var;
	jsdouble **number = var;
	unsigned char **string = var;
	jsval val;

	if (JS_ConvertValue(ctx, *vp, type, &val) == JS_FALSE) {
		switch (type) {
			case JSTYPE_BOOLEAN: *boolean = JS_FALSE; break;
			case JSTYPE_NUMBER: *number = NULL; break;
			case JSTYPE_STRING: *string = NULL; break;
			case JSTYPE_VOID:
			case JSTYPE_OBJECT:
			case JSTYPE_FUNCTION:
			case JSTYPE_LIMIT:
			default:
				INTERNAL("Invalid type %d in jsval_to_value()", type);
				break;
		}
		return;
	}

	switch (type) {
		case JSTYPE_BOOLEAN: *boolean = JSVAL_TO_BOOLEAN(val); break;
		case JSTYPE_NUMBER: *number = JSVAL_TO_DOUBLE(val); break;
		case JSTYPE_STRING:
			*string = (unsigned char *) JS_GetStringChars(JS_ValueToString(ctx, val));
			break;
		case JSTYPE_VOID:
		case JSTYPE_OBJECT:
		case JSTYPE_FUNCTION:
		case JSTYPE_LIMIT:
		default:
			INTERNAL("Invalid type %d in jsval_to_value()", type);
			break;
	}
}


static const JSClass global_class = {
	"Compilation scope",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};


static JSBool document_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);
static JSBool document_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);

static const JSClass document_class = {
	"document",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	document_get_property, document_set_property,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

enum document_prop { JSP_DOC_TITLE, JSP_DOC_URL };
static const JSPropertySpec document_props[] = {
	{ "title",	JSP_DOC_TITLE,	JSPROP_ENUMERATE }, /* TODO: Charset? */
	{ "URL",	JSP_DOC_URL,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

static JSBool
document_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct document_view *doc_view = JS_GetContextPrivate(ctx);
	struct document *document = doc_view->document;

	VALUE_TO_JSVAL_START;

	switch (JSVAL_TO_INT(id)) {
	case JSP_DOC_TITLE: string = document->title; prop_type = JSPT_STRING; break;
	case JSP_DOC_URL: string = get_uri_string(document->uri, URI_ORIGINAL); prop_type = JSPT_ASTRING; break;
	default:
		INTERNAL("Invalid ID %d in document_get_property().", JSVAL_TO_INT(id));
		goto bye;
	}

	VALUE_TO_JSVAL_END;
}

static JSBool
document_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct document_view *doc_view = JS_GetContextPrivate(ctx);
	struct document *document = doc_view->document;

	JSVAL_TO_VALUE_START;

	switch (JSVAL_TO_INT(id)) {
	case JSP_DOC_TITLE:
		JSVAL_REQUIRE(vp, STRING, string);
		if (document->title) mem_free(document->title);
		document->title = stracpy(string);
		free(string);
		break;
	}

	JSVAL_TO_VALUE_END;
}



/*** The ELinks interface */

static JSRuntime *jsrt;

void
spidermonkey_init(void)
{
	jsrt = JS_NewRuntime(0x400000UL);
}

void
spidermonkey_done(void)
{
	JS_DestroyRuntime(jsrt);
	JS_ShutDown();
}


void *
spidermonkey_get_interpreter(struct ecmascript_interpreter *interpreter)
{
	JSContext *ctx;
	JSObject *global_obj, *document_obj;

	assert(interpreter);

	ctx = JS_NewContext(jsrt, 8192 /* Stack allocation chunk size */);
	if (!ctx)
		return NULL;
	interpreter->backend_data = ctx;

	global_obj = JS_NewObject(ctx, (JSClass *) &global_class, NULL, NULL);
	if (!global_obj) {
		spidermonkey_put_interpreter(interpreter);
		return NULL;
	}
	JS_InitStandardClasses(ctx, global_obj);
	JS_SetContextPrivate(ctx, interpreter->doc_view);

	document_obj = JS_InitClass(ctx, global_obj, NULL,
	                            (JSClass *) &document_class, NULL, 0,
	                            (JSPropertySpec *) document_props, NULL,
	                            NULL, NULL);

	return ctx;
}

void
spidermonkey_put_interpreter(struct ecmascript_interpreter *interpreter)
{
	JSContext *ctx;

	assert(interpreter);
	ctx = interpreter->backend_data;
	JS_DestroyContext(ctx);
	interpreter->backend_data = NULL;
}


unsigned char *
spidermonkey_eval_stringback(struct ecmascript_interpreter *interpreter,
                             struct string *code)
{
	JSContext *ctx;
	jsval rval;
	unsigned char *string = NULL;
	unsigned char *ret = NULL;

	assert(interpreter);
	ctx = interpreter->backend_data;
	if (JS_EvaluateScript(ctx, JS_GetGlobalObject(ctx),
	                      code->source, code->length, "", 0, &rval)
	    == JS_FALSE)
		return NULL;

	JSVAL_REQUIRE(&rval, STRING, string);
	if (string) {
		ret = stracpy(string);
		free(string);
	}
	return ret;
}
