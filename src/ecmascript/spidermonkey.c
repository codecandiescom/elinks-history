/* The SpiderMonkey ECMAScript backend. */
/* $Id: spidermonkey.c,v 1.35 2004/09/25 01:04:12 jonas Exp $ */

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

#include "bfu/msgbox.h"
#include "bfu/style.h"
#include "dialogs/status.h"
#include "document/document.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/spidermonkey.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/select.h"
#include "protocol/uri.h"
#include "sched/task.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
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
	JSPT_OBJECT,
};

union prop_union {
	int boolean;
	int number;
	JSObject *object;
	unsigned char *string;
};

#define VALUE_TO_JSVAL_START \
	enum prop_type prop_type; \
	union prop_union p; \
 \
	/* Prevent "Unused variable" warnings. */ \
	if (!JSVAL_IS_INT(id) || (p.string = NULL)) \
		goto bye;

#define VALUE_TO_JSVAL_END(vp) \
	value_to_jsval(ctx, vp, prop_type, &p); \
 \
bye: \
	return JS_TRUE;

static void
value_to_jsval(JSContext *ctx, jsval *vp, enum prop_type prop_type,
	       union prop_union *prop)
{
	switch (prop_type) {
	case JSPT_STRING:
	case JSPT_ASTRING:
		if (!prop->string) {
			*vp = JSVAL_NULL;
			break;
		}
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(ctx, prop->string));
		if (prop_type == JSPT_ASTRING)
			mem_free(prop->string);
		break;

	case JSPT_BOOLEAN:
		*vp = BOOLEAN_TO_JSVAL(prop->boolean);
		break;

	case JSPT_OBJECT:
		*vp = OBJECT_TO_JSVAL(prop->object);
		break;

	case JSPT_UNDEF:
	default:
		*vp = JSVAL_NULL;
		break;
	}
}

union jsval_union {
	jsint boolean;
	jsdouble *number;
	unsigned char *string;
};

#define JSVAL_TO_VALUE_START \
	union jsval_union v; \
 \
	/* Prevent "Unused variable" warnings. */ \
	if (!JSVAL_IS_INT(id) || (v.string = NULL)) \
		goto bye;

#define JSVAL_REQUIRE(vp, type) \
	jsval_to_value(ctx, vp, JSTYPE_ ## type, &v);

#define JSVAL_TO_VALUE_END \
bye: \
	return JS_TRUE;

static void
jsval_to_value(JSContext *ctx, jsval *vp, JSType type, union jsval_union *var)
{
	jsval val;

	if (JS_ConvertValue(ctx, *vp, type, &val) == JS_FALSE) {
		switch (type) {
			case JSTYPE_BOOLEAN: var->boolean = JS_FALSE; break;
			case JSTYPE_NUMBER: var->number = NULL; break;
			case JSTYPE_STRING: var->string = NULL; break;
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
		case JSTYPE_BOOLEAN: var->boolean = JSVAL_TO_BOOLEAN(val); break;
		case JSTYPE_NUMBER: var->number = JSVAL_TO_DOUBLE(val); break;
		case JSTYPE_STRING:
			var->string = JS_GetStringBytes(JS_ValueToString(ctx, val));
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



static JSBool window_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);
static JSBool window_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);

static const JSClass window_class = {
	"window",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	window_get_property, window_set_property,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

enum window_prop {
	JSP_WIN_CLOSED,
	JSP_WIN_DOC,
	JSP_WIN_LOC,
	JSP_WIN_MBAR,
	JSP_WIN_SELF,
	JSP_WIN_SBAR,
	JSP_WIN_TOP,
};
static const JSPropertySpec window_props[] = {
	{ "closed",	JSP_WIN_CLOSED,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "document",	JSP_WIN_DOC,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "location",	JSP_WIN_LOC,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "menubar",	JSP_WIN_MBAR,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "self",	JSP_WIN_SELF,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "statusbar",	JSP_WIN_SBAR,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "top",	JSP_WIN_TOP,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "window",	JSP_WIN_SELF,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

static JSBool
window_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct document_view *doc_view = JS_GetPrivate(ctx, obj);

	VALUE_TO_JSVAL_START;

	switch (JSVAL_TO_INT(id)) {
	case JSP_WIN_CLOSED:
		/* TODO: It will be a major PITA to implement this properly.
		 * Well, perhaps not so much if we introduce reference tracking
		 * for (struct session)? Still... --pasky */
		p.boolean = 0; prop_type = JSPT_BOOLEAN; break;
	case JSP_WIN_SELF: p.object = obj; prop_type = JSPT_OBJECT; break;
	case JSP_WIN_TOP:
	{
		struct document_view *top_view = doc_view->session->doc_view;

		assert(top_view && top_view->ecmascript);
		p.object=JS_GetGlobalObject(top_view->ecmascript->backend_data);
		prop_type = JSPT_OBJECT;
		break;
	}
	default:
		INTERNAL("Invalid ID %d in window_get_property().", JSVAL_TO_INT(id));
		goto bye;
	}

	VALUE_TO_JSVAL_END(vp);
}

static JSBool
window_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSVAL_TO_VALUE_START;

	switch (JSVAL_TO_INT(id)) {
	default:
		INTERNAL("Invalid ID %d in window_set_property().", JSVAL_TO_INT(id));
		goto bye;
	}

	JSVAL_TO_VALUE_END;
}

static JSBool window_alert(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

static const JSFunctionSpec window_funcs[] = {
	{ "alert",	window_alert,		1 },
	{ NULL }
};

static JSBool
window_alert(JSContext *ctx, JSObject *obj, uintN argc,jsval *argv, jsval *rval)
{
	struct document_view *doc_view = JS_GetPrivate(ctx, obj);
	union jsval_union v;
	enum prop_type prop_type;
	union prop_union p;

	assert(argc == 1);

	JSVAL_REQUIRE(&argv[0], STRING);
	if (!v.string || !*v.string)
		goto bye;

	msg_box(doc_view->session->tab->term, NULL, MSGBOX_FREE_TEXT | MSGBOX_NO_INTL,
		N_("JavaScript Alert"), ALIGN_CENTER,
		stracpy(v.string),
		NULL, 1,
		N_("OK"), NULL, B_ENTER | B_ESC);

	p.boolean = 1; prop_type = JSPT_BOOLEAN;
	VALUE_TO_JSVAL_END(rval);
}


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
	{ "url",	JSP_DOC_URL,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

static JSBool
document_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *parent = JS_GetParent(ctx, obj);
	struct document_view *doc_view = JS_GetPrivate(ctx, parent);
	struct document *document = doc_view->document;

	VALUE_TO_JSVAL_START;

	switch (JSVAL_TO_INT(id)) {
	case JSP_DOC_TITLE: p.string = document->title; prop_type = JSPT_STRING; break;
	case JSP_DOC_URL: p.string = get_uri_string(document->uri, URI_ORIGINAL); prop_type = JSPT_ASTRING; break;
	default:
		INTERNAL("Invalid ID %d in document_get_property().", JSVAL_TO_INT(id));
		goto bye;
	}

	VALUE_TO_JSVAL_END(vp);
}

static JSBool
document_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *parent = JS_GetParent(ctx, obj);
	struct document_view *doc_view = JS_GetPrivate(ctx, parent);
	struct document *document = doc_view->document;

	JSVAL_TO_VALUE_START;

	switch (JSVAL_TO_INT(id)) {
	case JSP_DOC_TITLE:
		JSVAL_REQUIRE(vp, STRING);
		if (document->title) mem_free(document->title);
		document->title = stracpy(v.string);
		break;
	}

	JSVAL_TO_VALUE_END;
}


static JSBool location_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);
static JSBool location_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);

static const JSClass location_class = {
	"location",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	location_get_property, location_set_property,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

enum location_prop { JSP_LOC_HREF };
static const JSPropertySpec location_props[] = {
	{ "href",	JSP_LOC_HREF,	JSPROP_ENUMERATE },
	{ NULL }
};

static JSBool
location_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *parent = JS_GetParent(ctx, obj);
	struct document_view *doc_view = JS_GetPrivate(ctx, parent);
	struct view_state *vs = &cur_loc(doc_view->session)->vs;

	VALUE_TO_JSVAL_START;

	switch (JSVAL_TO_INT(id)) {
	case JSP_LOC_HREF: p.string = get_uri_string(vs->uri, URI_ORIGINAL); prop_type = JSPT_ASTRING; break;
	default:
		INTERNAL("Invalid ID %d in location_get_property().", JSVAL_TO_INT(id));
		goto bye;
	}

	VALUE_TO_JSVAL_END(vp);
}

static JSBool
location_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *parent = JS_GetParent(ctx, obj);
	struct document_view *doc_view = JS_GetPrivate(ctx, parent);

	JSVAL_TO_VALUE_START;

	switch (JSVAL_TO_INT(id)) {
	case JSP_LOC_HREF:
	{
		struct uri *new_uri;

		JSVAL_REQUIRE(vp, STRING);
		new_uri = get_hooked_uri(v.string, doc_view->session,
					 doc_view->session->tab->term->cwd);
		if (!new_uri)
			break;
		goto_uri_frame(doc_view->session, new_uri, doc_view->name,
			       CACHE_MODE_NORMAL);
		done_uri(new_uri);
		break;
	}
	}

	JSVAL_TO_VALUE_END;
}


static JSBool unibar_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);
static JSBool unibar_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);

static const JSClass menubar_class = {
	"menubar",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	unibar_get_property, unibar_set_property,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};
static const JSClass statusbar_class = {
	"statusbar",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	unibar_get_property, unibar_set_property,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

enum unibar_prop { JSP_UNIBAR_VISIBLE };
static const JSPropertySpec unibar_props[] = {
	{ "visible",	JSP_UNIBAR_VISIBLE,	JSPROP_ENUMERATE },
	{ NULL }
};

static JSBool
unibar_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *parent = JS_GetParent(ctx, obj);
	struct document_view *doc_view = JS_GetPrivate(ctx, parent);
	struct session_status *status = &doc_view->session->status;
	unsigned char *bar = JS_GetPrivate(ctx, obj);

	VALUE_TO_JSVAL_START;

	switch (JSVAL_TO_INT(id)) {
	case JSP_UNIBAR_VISIBLE:
#define unibar_fetch(bar) \
	p.boolean = status->force_show_##bar##_bar >= 0 \
	            ? status->force_show_##bar##_bar \
	            : status->show_##bar##_bar
		switch (*bar) {
			case 's': unibar_fetch(status); break;
			case 't': unibar_fetch(title); break;
			default: p.boolean = 0; break;
		}
		prop_type = JSPT_BOOLEAN;
#undef unibar_fetch
		break;
	default:
		INTERNAL("Invalid ID %d in unibar_get_property().", JSVAL_TO_INT(id));
		goto bye;
	}

	VALUE_TO_JSVAL_END(vp);
}

static JSBool
unibar_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *parent = JS_GetParent(ctx, obj);
	struct document_view *doc_view = JS_GetPrivate(ctx, parent);
	struct session_status *status = &doc_view->session->status;
	unsigned char *bar = JS_GetPrivate(ctx, obj);

	JSVAL_TO_VALUE_START;

	switch (JSVAL_TO_INT(id)) {
	case JSP_UNIBAR_VISIBLE:
		JSVAL_REQUIRE(vp, BOOLEAN);
#define unibar_set(bar) \
	status->force_show_##bar##_bar = v.boolean;
		switch (*bar) {
			case 's': unibar_set(status); break;
			case 't': unibar_set(title); break;
			default: v.boolean = 0; break;
		}
		register_bottom_half((void (*)(void*)) update_status, NULL);
#undef unibar_set
		break;
	default:
		INTERNAL("Invalid ID %d in unibar_set_property().", JSVAL_TO_INT(id));
		goto bye;
	}

	JSVAL_TO_VALUE_END;
}



/*** The ELinks interface */

static JSRuntime *jsrt;

static void
error_reporter(JSContext *ctx, const char *message, JSErrorReport *report)
{
	struct ecmascript_interpreter *interpreter = JS_GetContextPrivate(ctx);
	struct terminal *term = interpreter->doc_view->session->tab->term;

#ifdef CONFIG_LEDS
	interpreter->doc_view->session->status.ecmascript_led->value = 'J';
#endif

	if (!get_opt_bool("ecmascript.error_reporting"))
		return;

	msg_box(term, NULL, MSGBOX_FREE_TEXT,
		N_("JavaScript Error"), ALIGN_CENTER,
		report->linebuf && report->tokenptr
		?
		msg_text(term, N_("A script embedded in the current "
		         "document raised the following exception: "
		         "\n\n%s\n\n%s\n.%*s^%*s."),
		         message,
			 report->linebuf,
			 report->tokenptr - report->linebuf - 2, " ",
			 strlen(report->linebuf) - (report->tokenptr - report->linebuf) - 1, " ")
		:
		msg_text(term, N_("A script embedded in the current "
		         "document raised the following exception: "
		         "\n\n%s"),
		         message)
		,
		NULL, 1,
		N_("OK"), NULL, B_ENTER | B_ESC);
}

static JSBool
safeguard(JSContext *ctx, JSScript *script)
{
	struct ecmascript_interpreter *interpreter = JS_GetContextPrivate(ctx);

	if (time(NULL) - interpreter->exec_start > 5) {
		/* A killer script! Alert! */
		msg_box(interpreter->doc_view->session->tab->term, NULL, 0,
			N_("JavaScript Emergency"), ALIGN_CENTER,
			N_("A script embedded in the current document was running "
			"for more than 5 seconds in line. This probably means "
			"there is a bug in the script and it could have halted "
			"the whole ELinks. The script execution was interrupted."),
			NULL, 1,
			N_("OK"), NULL, B_ENTER | B_ESC);
		return JS_FALSE;
	}
	return JS_TRUE;
}

static void
setup_safeguard(struct ecmascript_interpreter *interpreter,
                JSContext *ctx)
{
	interpreter->exec_start = time(NULL);
	JS_SetBranchCallback(ctx, safeguard);
}


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
	JSObject *window_obj, *document_obj, *location_obj,
	         *statusbar_obj, *menubar_obj;

	assert(interpreter);

	ctx = JS_NewContext(jsrt, 8192 /* Stack allocation chunk size */);
	if (!ctx)
		return NULL;
	interpreter->backend_data = ctx;
	JS_SetContextPrivate(ctx, interpreter);
	JS_SetErrorReporter(ctx, error_reporter);

	window_obj = JS_NewObject(ctx, (JSClass *) &window_class, NULL, NULL);
	if (!window_obj) {
		spidermonkey_put_interpreter(interpreter);
		return NULL;
	}
	JS_InitStandardClasses(ctx, window_obj);
	JS_DefineProperties(ctx, window_obj, (JSPropertySpec *) window_props);
	JS_DefineFunctions(ctx, window_obj, (JSFunctionSpec *) window_funcs);
	JS_SetPrivate(ctx, window_obj, interpreter->doc_view);

	document_obj = JS_InitClass(ctx, window_obj, NULL,
				    (JSClass *) &document_class, NULL, 0,
				    (JSPropertySpec *) document_props, NULL,
				    NULL, NULL);

	location_obj = JS_InitClass(ctx, window_obj, NULL,
				    (JSClass *) &location_class, NULL, 0,
				    (JSPropertySpec *) location_props, NULL,
				    NULL, NULL);

	menubar_obj = JS_InitClass(ctx, window_obj, NULL,
				   (JSClass *) &menubar_class, NULL, 0,
				   (JSPropertySpec *) unibar_props, NULL,
				   NULL, NULL);
	JS_SetPrivate(ctx, menubar_obj, "t");

	statusbar_obj = JS_InitClass(ctx, window_obj, NULL,
				     (JSClass *) &statusbar_class, NULL, 0,
				     (JSPropertySpec *) unibar_props, NULL,
				     NULL, NULL);
	JS_SetPrivate(ctx, statusbar_obj, "s");

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


void
spidermonkey_eval(struct ecmascript_interpreter *interpreter,
                  struct string *code)
{
	JSContext *ctx;
	jsval rval;

	assert(interpreter);
	ctx = interpreter->backend_data;
	setup_safeguard(interpreter, ctx);
	JS_EvaluateScript(ctx, JS_GetGlobalObject(ctx),
	                  code->source, code->length, "", 0, &rval);
}


unsigned char *
spidermonkey_eval_stringback(struct ecmascript_interpreter *interpreter,
			     struct string *code)
{
	JSContext *ctx;
	jsval rval;
	union jsval_union v;
	unsigned char *ret = NULL;

	assert(interpreter);
	ctx = interpreter->backend_data;
	setup_safeguard(interpreter, ctx);
	if (JS_EvaluateScript(ctx, JS_GetGlobalObject(ctx),
			      code->source, code->length, "", 0, &rval)
	    == JS_FALSE) {
		return NULL;
	}
	if (JSVAL_IS_VOID(rval)) {
		/* Undefined value. */
		return NULL;
	}

	JSVAL_REQUIRE(&rval, STRING);
	if (v.string) {
		ret = stracpy(v.string);
	}
	return ret;
}
