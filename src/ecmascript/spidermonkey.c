/* The SpiderMonkey ECMAScript backend. */
/* $Id: spidermonkey.c,v 1.97 2004/12/17 13:59:43 zas Exp $ */

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

#include "bfu/dialog.h"
#include "cookies/cookies.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "document/html/frames.h"
#include "document/document.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/spidermonkey.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/select.h"
#include "osdep/newwin.h"
#include "protocol/uri.h"
#include "sched/task.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/string.h"
#include "viewer/text/form.h"
#include "viewer/text/vs.h"


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
	enum prop_type prop_type = JSPT_UNDEF; \
	union prop_union p; \
 \
	/* Prevent "Unused variable" warnings. */ \
	if ((p.string = NULL)) \
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
	if ((v.string = NULL)) \
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
		case JSTYPE_BOOLEAN:
			var->boolean = JS_FALSE;
			break;
		case JSTYPE_NUMBER:
			var->number = NULL;
			break;
		case JSTYPE_STRING:
			var->string = NULL;
			break;
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
	case JSTYPE_BOOLEAN:
		var->boolean = JSVAL_TO_BOOLEAN(val);
		break;
	case JSTYPE_NUMBER:
		var->number = JSVAL_TO_DOUBLE(val);
		break;
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
	JSP_WIN_PARENT,
	JSP_WIN_SELF,
	JSP_WIN_TOP,
};
/* "location" is special because we need to simulate "location.href"
 * when the code is asking directly for "location". We do not register
 * it as a "known" property since that was yielding strange bugs
 * (SpiderMonkey was still asking us about the "location" string after
 * assigning to it once), instead we do just a little string
 * comparing. */
static const JSPropertySpec window_props[] = {
	{ "closed",	JSP_WIN_CLOSED,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "parent",	JSP_WIN_PARENT,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "self",	JSP_WIN_SELF,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "top",	JSP_WIN_TOP,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "window",	JSP_WIN_SELF,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};


static JSObject *
try_resolve_frame(struct document_view *doc_view, unsigned char *id)
{
	struct session *ses = doc_view->session;
	struct frame *target;

	assert(ses);
	target = ses_find_frame(ses, id);
	if (!target) return NULL;
	if (target->vs.ecmascript_fragile)
		ecmascript_reset_state(&target->vs);
	if (!target->vs.ecmascript) return NULL;
	return JS_GetGlobalObject(target->vs.ecmascript->backend_data);
}

#if 0
static struct frame_desc *
find_child_frame(struct document_view *doc_view, struct frame_desc *tframe)
{
	struct frameset_desc *frameset = doc_view->document->frame_desc;
	int i;

	if (!frameset)
		return NULL;

	for (i = 0; i < frameset->n; i++) {
		struct frame_desc *frame = &frameset->frame_desc[i];

		if (frame == tframe)
			return frame;
	}

	return NULL;
}
#endif

static JSBool
window_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct view_state *vs = JS_GetPrivate(ctx, obj);
	VALUE_TO_JSVAL_START;

	/* No need for special window.location measurements - when
	 * location is then evaluated in string context, toString()
	 * is called which we overrode for that class below, so
	 * everything's fine. */
	if (JSVAL_IS_STRING(id)) {
		struct document_view *doc_view = vs->doc_view;
		JSObject *obj;
		JSVAL_TO_VALUE_START;

		JSVAL_REQUIRE(&id, STRING);
		obj = try_resolve_frame(doc_view, v.string);
		/* TODO: Try other lookups (mainly element lookup) until
		 * something yields data. */
		if (!obj) goto bye;
		p.object = obj; prop_type = JSPT_OBJECT;
		goto convert;
	} else if (!JSVAL_IS_INT(id))
		goto bye;

	switch (JSVAL_TO_INT(id)) {
	case JSP_WIN_CLOSED:
		/* TODO: It will be a major PITA to implement this properly.
		 * Well, perhaps not so much if we introduce reference tracking
		 * for (struct session)? Still... --pasky */
		p.boolean = 0; prop_type = JSPT_BOOLEAN;
		break;
	case JSP_WIN_SELF:
		p.object = obj; prop_type = JSPT_OBJECT;
		break;
	case JSP_WIN_PARENT:
		/* XXX: It would be nice if the following worked, yes.
		 * The problem is that we get called at the point where
		 * document.frame properties are going to be mostly NULL.
		 * But the problem is deeper because at that time we are
		 * yet building scrn_frames so our parent might not be there
		 * yet (XXX: is this true?). The true solution will be to just
		 * have struct document_view *(document_view.parent). --pasky */
		/* FIXME: So now we alias window.parent to window.top, which is
		 * INCORRECT but works for the most common cases of just two
		 * frames. Better something than nothing. */
#if 0
	{
		/* This is horrible. */
		struct document_view *doc_view = vs->doc_view;
		struct session *ses = doc_view->session;
		struct frame_desc *frame = doc_view->document->frame;

		if (!ses->doc_view->document->frame_desc) {
			INTERNAL("Looking for parent but there're no frames.");
			prop_type = JSPT_UNDEF;
			break;
		}
		assert(frame);
		doc_view = ses->doc_view;
		if (find_child_frame(doc_view, frame))
			goto found_parent;
		foreach (doc_view, ses->scrn_frames) {
			if (find_child_frame(doc_view, frame))
				goto found_parent;
		}
		INTERNAL("Cannot find frame %s parent.",doc_view->name);
		prop_type = JSPT_UNDEF;
		break;

found_parent:
		if (doc_view->vs.ecmascript_fragile)
			ecmascript_reset_state(&doc_view->vs);
		assert(doc_view->ecmascript);
		p.object=JS_GetGlobalObject(doc_view->ecmascript->backend_data);
		prop_type = JSPT_OBJECT;
		break;
	}
#endif
	case JSP_WIN_TOP:
	{
		struct document_view *doc_view = vs->doc_view;
		struct document_view *top_view = doc_view->session->doc_view;

		assert(top_view && top_view->vs);
		if (top_view->vs->ecmascript_fragile)
			ecmascript_reset_state(top_view->vs);
		if (!top_view->vs->ecmascript) break;
		p.object = JS_GetGlobalObject(top_view->vs->ecmascript->backend_data);
		prop_type = JSPT_OBJECT;
		break;
	}
	default:
		INTERNAL("Invalid ID %d in window_get_property().", JSVAL_TO_INT(id));
		goto bye;
	}

convert:
	VALUE_TO_JSVAL_END(vp);
}

static void location_goto(struct document_view *doc_view, unsigned char *url);

static JSBool
window_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct view_state *vs = JS_GetPrivate(ctx, obj);
	JSVAL_TO_VALUE_START;

	if (JSVAL_IS_STRING(id)) {
		JSVAL_REQUIRE(&id, STRING);
		if (!strcmp(v.string, "location")) {
			struct document_view *doc_view = vs->doc_view;

			JSVAL_REQUIRE(vp, STRING);
			location_goto(doc_view, v.string);
			/* Do NOT touch our .location property, evil
			 * SpiderMonkey!! */
			return JS_FALSE;
		}
		goto bye;
	} else if (!JSVAL_IS_INT(id))
		goto bye;

	switch (JSVAL_TO_INT(id)) {
	default:
		INTERNAL("Invalid ID %d in window_set_property().", JSVAL_TO_INT(id));
		goto bye;
	}

	JSVAL_TO_VALUE_END;
}

static JSBool window_alert(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool window_open(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

static const JSFunctionSpec window_funcs[] = {
	{ "alert",	window_alert,		1 },
	{ "open",	window_open,		3 },
	{ NULL }
};

static JSBool
window_alert(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct view_state *vs = JS_GetPrivate(ctx, obj);
	union jsval_union v;
	enum prop_type prop_type;
	union prop_union p;

	prop_type = JSPT_UNDEF;

	if (argc != 1)
		goto bye;

	JSVAL_REQUIRE(&argv[0], STRING);
	if (!v.string || !*v.string)
		goto bye;

	msg_box(vs->doc_view->session->tab->term, NULL, MSGBOX_FREE_TEXT | MSGBOX_NO_INTL,
		N_("JavaScript Alert"), ALIGN_CENTER,
		stracpy(v.string),
		NULL, 1,
		N_("OK"), NULL, B_ENTER | B_ESC);

	VALUE_TO_JSVAL_END(rval);
}

struct delayed_open {
	struct session *ses;
	struct uri *uri;
};

static void
delayed_open(void *data)
{
	struct delayed_open *deo = data;

	assert(deo);
	open_uri_in_new_tab(deo->ses, deo->uri, 0, 0);
	done_uri(deo->uri);
	mem_free(deo);
}

static JSBool
window_open(JSContext *ctx, JSObject *obj, uintN argc,jsval *argv, jsval *rval)
{
	struct view_state *vs = JS_GetPrivate(ctx, obj);
	struct document_view *doc_view = vs->doc_view;
	struct session *ses = doc_view->session;
	union jsval_union v;
	unsigned char *url;
	struct uri *uri;
	enum prop_type prop_type;
	union prop_union p;
	static time_t ratelimit_start;
	static int ratelimit_count;

	prop_type = JSPT_UNDEF;
	if (argc < 1) goto bye;

	/* Ratelimit window opening. Recursive window.open() is very nice.
	 * We permit at most 20 tabs in 2 seconds. The ratelimiter is very
	 * rough but shall suffice against the usual cases. */

	if (!ratelimit_start || time(NULL) - ratelimit_start > 2) {
		ratelimit_start = time(NULL);
		ratelimit_count = 0;
	} else {
		ratelimit_count++;
		if (ratelimit_count > 20)
			goto bye;
	}

	JSVAL_REQUIRE(&argv[0], STRING);
	url = v.string;
	assert(url);
	/* TODO: Support for window naming and perhaps some window features? */

	url = join_urls(doc_view->document->uri,
	                trim_chars(url, ' ', 0));
	if (!url) goto bye;
	uri = get_uri(url, 0);
	mem_free(url);
	if (!uri) goto bye;

	if (!get_cmd_opt_bool("no-connect")
	    && !get_cmd_opt_bool("no-home")
	    && !get_cmd_opt_bool("anonymous")
	    && can_open_in_new(ses->tab->term)) {
		open_uri_in_new_window(ses, uri, ENV_ANY);
		p.boolean = 1; prop_type = JSPT_BOOLEAN;
	} else {
		/* When opening a new tab, we might get rerendered, losing our
		 * context and triggerring a disaster, so postpone that. */
		struct delayed_open *deo = mem_calloc(1, sizeof(*deo));

		if (deo) {
			deo->ses = ses;
			deo->uri = get_uri_reference(uri);
			register_bottom_half((void (*)(void *)) delayed_open,
			                     deo);
			p.boolean = 1; prop_type = JSPT_BOOLEAN;
		}
	}

	done_uri(uri);

	VALUE_TO_JSVAL_END(rval);
}


static JSBool form_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);
static JSBool form_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);

static const JSClass form_class = {
	"form",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	form_get_property, form_set_property,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

enum form_prop {
	JSP_FORM_CONTROL_NAME,
	JSP_FORM_CONTROL_ACTION,
	JSP_FORM_CONTROL_METHOD,
	JSP_FORM_CONTROL_TARGET
};

static const JSPropertySpec form_props[] = {
	{ "name",	JSP_FORM_CONTROL_NAME,	JSPROP_ENUMERATE },
	{ "action",	JSP_FORM_CONTROL_ACTION,	JSPROP_ENUMERATE },
	{ "method",	JSP_FORM_CONTROL_METHOD,	JSPROP_ENUMERATE },
	{ "target",	JSP_FORM_CONTROL_TARGET,	JSPROP_ENUMERATE },
	{ NULL }
};

static JSBool form_reset(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool form_submit(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

static const JSFunctionSpec form_funcs[] = {
	{ "reset",	form_reset,	0 },
	{ "submit",	form_submit,	0 },
	{ NULL }
};

static JSBool
form_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct form_control *fc = JS_GetPrivate(ctx, obj);
	VALUE_TO_JSVAL_START;

	assert(fc);

	if (!JSVAL_IS_INT(id))
		goto bye;

	switch (JSVAL_TO_INT(id)) {
	case JSP_FORM_CONTROL_NAME:
		p.string = fc->name;
		prop_type = JSPT_STRING;
		break;

	case JSP_FORM_CONTROL_ACTION:
		p.string = fc->action;
		prop_type  = JSPT_STRING;
		break;

	case JSP_FORM_CONTROL_METHOD:
		switch (fc->method) {
		case FORM_METHOD_GET:
			p.string = "GET";
			prop_type = JSPT_STRING;
			goto end;

		case FORM_METHOD_POST:
		case FORM_METHOD_POST_MP:
		case FORM_METHOD_POST_TEXT_PLAIN:
			p.string = "POST";
			prop_type = JSPT_STRING;
			goto end;
		}
		break;

	case JSP_FORM_CONTROL_TARGET:
		p.string = fc->target;
		prop_type = JSPT_STRING;
		break;

	default:
		INTERNAL("Invalid ID %d in form_get_property().", JSVAL_TO_INT(id));
		goto bye;
	}

end:
	VALUE_TO_JSVAL_END(vp);
}

static JSBool
form_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct form_control *fc = JS_GetPrivate(ctx, obj);
	JSVAL_TO_VALUE_START;

	assert(fc);

	if (!JSVAL_IS_INT(id))
		goto bye;

	switch (JSVAL_TO_INT(id)) {
	case JSP_FORM_CONTROL_NAME:
		JSVAL_REQUIRE(vp, STRING);
		mem_free_set(fc->name, stracpy(v.string));
		break;

	case JSP_FORM_CONTROL_ACTION:
		JSVAL_REQUIRE(vp, STRING);
		mem_free_set(fc->action, stracpy(v.string));
		break;

	case JSP_FORM_CONTROL_METHOD:
		JSVAL_REQUIRE(vp, STRING);
		if (!strcasecmp(v.string, "GET")) {
			fc->method = FORM_METHOD_GET;
		} else if (!strcasecmp(v.string, "POST")) {
			fc->method = FORM_METHOD_POST;
		}
		break;

	case JSP_FORM_CONTROL_TARGET:
		JSVAL_REQUIRE(vp, STRING);
		mem_free_set(fc->target, stracpy(v.string));
		break;

	default:
		INTERNAL("Invalid ID %d in form_set_property().", JSVAL_TO_INT(id));
		goto bye;
	}

	JSVAL_TO_VALUE_END;
}

static JSBool
form_reset(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject *parent = JS_GetParent(ctx, obj);
	struct view_state *vs = JS_GetPrivate(ctx, parent);
	struct document_view *doc_view = vs->doc_view;
	struct form_control *fc = JS_GetPrivate(ctx, obj);
	VALUE_TO_JSVAL_START;

	p.boolean = 0; prop_type = JSPT_BOOLEAN;

	assert(fc);
	do_reset_form(doc_view, fc->form_num);
	draw_forms(doc_view->session->tab->term, doc_view);

	VALUE_TO_JSVAL_END(rval);
}

static JSBool
form_submit(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject *parent = JS_GetParent(ctx, obj);
	struct view_state *vs = JS_GetPrivate(ctx, parent);
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct form_control *fc = JS_GetPrivate(ctx, obj);
	int link;
	VALUE_TO_JSVAL_START;

	p.boolean = 0; prop_type = JSPT_BOOLEAN;

	assert(fc);
	for (link = 0; link < document->nlinks; link++) {
		if (get_link_form_control(&document->links[link]) == fc) {
			doc_view->vs->current_link = link;
			submit_form(doc_view->session, doc_view, 0);

			VALUE_TO_JSVAL_END(rval);
			/* This returns. */
		}
	}

	goto bye;
}


static JSBool forms_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);

static const JSClass forms_class = {
	"forms",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	forms_get_property, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

static JSBool forms_item(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool forms_namedItem(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

static const JSFunctionSpec forms_funcs[] = {
	{ "item",		forms_item,		1 },
	{ "namedItem",		forms_namedItem,	1 },
	{ NULL }
};

enum forms_prop { JSP_FORMS_LENGTH };
static const JSPropertySpec forms_props[] = {
	{ "length",	JSP_FORMS_LENGTH,	JSPROP_ENUMERATE | JSPROP_READONLY},
	{ NULL }
};

static JSObject *
get_form_object(JSContext *ctx, JSObject *parent, struct form_control *fc)
{
	JSObject *form = JS_NewObject(ctx, (JSClass *) &form_class, NULL, parent);

	JS_DefineFunctions(ctx, form, (JSFunctionSpec *)&form_funcs);
	JS_SetPrivate(ctx, form, fc);
	return form;
}

static JSBool
forms_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *parent = JS_GetParent(ctx, obj);
	struct view_state *vs = JS_GetPrivate(ctx, parent);
	VALUE_TO_JSVAL_START;

	if (!JSVAL_IS_INT(id))
		goto bye;

	switch (JSVAL_TO_INT(id)) {
	case JSP_FORMS_LENGTH:
	{
		struct form_control *fc;
		int counter = 0;
		struct document_view *doc_view = vs->doc_view;
		struct document *document = doc_view->document;

		foreach (fc, document->forms)
			counter++;

		p.number = counter;
		prop_type = JSPT_INT;
		break;
	}
	default:
		INTERNAL("Invalid ID %d in forms_get_property().", JSVAL_TO_INT(id));
		goto bye;
	}

	VALUE_TO_JSVAL_END(vp);
}

static JSBool
forms_item(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject *parent = JS_GetParent(ctx, obj);
	struct view_state *vs = JS_GetPrivate(ctx, parent);
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct form_control *fc;
	union jsval_union v;
	enum prop_type prop_type;
	union prop_union p;
	int counter = 0;
	int index;

	prop_type = JSPT_UNDEF;

	if (argc != 1)
		goto bye;

	JSVAL_REQUIRE(&argv[0], NUMBER);
	index = DOUBLE_TO_JSVAL(v.number);

	foreach (fc, document->forms) {
		counter++;
		if (counter == index) {
			p.object = get_form_object(ctx, obj, fc);
			prop_type = JSPT_OBJECT;

			VALUE_TO_JSVAL_END(rval);
			/* This returns. */
		}
	}

	goto bye;
}

static JSBool
forms_namedItem(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject *parent = JS_GetParent(ctx, obj);
	struct view_state *vs = JS_GetPrivate(ctx, parent);
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct form_control *fc;
	union jsval_union v;
	enum prop_type prop_type;
	union prop_union p;

	prop_type = JSPT_UNDEF;

	if (argc != 1)
		goto bye;

	JSVAL_REQUIRE(&argv[0], STRING);
	if (!v.string || !*v.string)
		goto bye;

	foreach (fc, document->forms) {
		if (fc->formname && !strcasecmp(v.string, fc->formname)) {
			p.object = get_form_object(ctx, obj, fc);
			prop_type = JSPT_OBJECT;

			VALUE_TO_JSVAL_END(rval);
			/* This returns. */
		}
	}

	goto bye;
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

enum document_prop { JSP_DOC_REF, JSP_DOC_TITLE, JSP_DOC_URL };
/* "cookie" is special; it isn't a regular property but we channel it to the
 * cookie-module. XXX: Would it work if "cookie" was defined in this array? */
static const JSPropertySpec document_props[] = {
	{ "location",	JSP_DOC_URL,	JSPROP_ENUMERATE },
	{ "referrer",	JSP_DOC_REF,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "title",	JSP_DOC_TITLE,	JSPROP_ENUMERATE }, /* TODO: Charset? */
	{ "url",	JSP_DOC_URL,	JSPROP_ENUMERATE },
	{ NULL }
};

static JSBool
document_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *parent = JS_GetParent(ctx, obj);
	struct view_state *vs = JS_GetPrivate(ctx, parent);
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct session *ses = doc_view->session;
	VALUE_TO_JSVAL_START;

	if (JSVAL_IS_STRING(id)) {
		struct form_control *fc;
		JSVAL_TO_VALUE_START;

		JSVAL_REQUIRE(&id, STRING);
#ifdef CONFIG_COOKIES
		if (!strcmp(v.string, "cookie")) {
			struct string *cookies = send_cookies(vs->uri);

			if (cookies) {
				p.string = cookies->source;
				prop_type = JSPT_ASTRING;
				goto convert;
			}
		}
#endif
		foreach (fc, document->forms) {
			jsval forms;
			JSBool success;

			if (!fc->formname || strcasecmp(v.string, fc->formname))
				continue;

			success = JS_GetProperty(ctx, obj, "forms", &forms);
			assert(success == JS_TRUE);

			p.object = get_form_object(ctx, JSVAL_TO_OBJECT(forms), fc);
			prop_type = JSPT_OBJECT;
			goto convert;
		}
		goto bye;
	} else if (!JSVAL_IS_INT(id))
		goto bye;

	switch (JSVAL_TO_INT(id)) {
	case JSP_DOC_REF:
		prop_type = JSPT_UNDEF;
		switch (get_opt_int("protocol.http.referer.policy")) {
		case REFERER_NONE:
			/* oh well */
			break;

		case REFERER_FAKE:
			p.string = get_opt_str("protocol.http.referer.fake");
			prop_type = JSPT_STRING;
			break;

		case REFERER_TRUE:
			/* XXX: Encode as in add_url_to_http_string() ? --pasky */
			if (ses->referrer) {
				p.string = get_uri_string(ses->referrer, URI_HTTP_REFERRER);
				prop_type = JSPT_ASTRING;
			}
			break;

		case REFERER_SAME_URL:
			p.string = get_uri_string(document->uri, URI_HTTP_REFERRER);
			prop_type = JSPT_ASTRING;
			break;
		}
		break;
	case JSP_DOC_TITLE:
		p.string = document->title; prop_type = JSPT_STRING;
		break;
	case JSP_DOC_URL:
		p.string = get_uri_string(document->uri, URI_ORIGINAL);
		prop_type = JSPT_ASTRING;
		break;
	default:
		INTERNAL("Invalid ID %d in document_get_property().", JSVAL_TO_INT(id));
		goto bye;
	}

convert:
	VALUE_TO_JSVAL_END(vp);
}

static JSBool
document_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *parent = JS_GetParent(ctx, obj);
	struct view_state *vs = JS_GetPrivate(ctx, parent);
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	JSVAL_TO_VALUE_START;

	if (JSVAL_IS_STRING(id)) {
		JSVAL_REQUIRE(&id, STRING);
#ifdef CONFIG_COOKIES
		if (!strcmp(v.string, "cookie")) {
			JSVAL_REQUIRE(vp, STRING);
			set_cookie(vs->uri, v.string);
			/* Do NOT touch our .cookie property, evil
			 * SpiderMonkey!! */
			return JS_FALSE;
		}
#endif
		goto bye;
	} else if (!JSVAL_IS_INT(id))
		goto bye;

	switch (JSVAL_TO_INT(id)) {
	case JSP_DOC_TITLE:
		JSVAL_REQUIRE(vp, STRING);
		if (document->title) mem_free(document->title);
		document->title = stracpy(v.string);
		break;
	case JSP_DOC_URL:
		/* According to the specs this should be readonly but some
		 * broken sites still assign to it (i.e.
		 * http://www.e-handelsfonden.dk/validering.asp?URL=www.polyteknisk.dk).
		 * So emulate window.location. */
		JSVAL_REQUIRE(vp, STRING);
		location_goto(doc_view, v.string);
		break;
	}

	JSVAL_TO_VALUE_END;
}

static JSBool document_write(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

static const JSFunctionSpec document_funcs[] = {
	{ "write",		document_write,		1 },
	{ NULL }
};

static JSBool
document_write(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
#ifdef CONFIG_LEDS
	struct ecmascript_interpreter *interpreter = JS_GetContextPrivate(ctx);
#endif
	VALUE_TO_JSVAL_START;

	p.boolean = 0; prop_type = JSPT_BOOLEAN;

	/* XXX: I don't know about you, but I have *ENOUGH* of those 'Undefined
	 * function' errors, I want to see just the useful ones. So just
	 * lighting a led and going away, no muss, no fuss. --pasky */
	/* TODO: Perhaps we can introduce ecmascript.error_report_unsupported
	 * -> "Show information about the document using some valid,
	 *  nevertheless unsupported methods/properties." --pasky too */

#ifdef CONFIG_LEDS
	interpreter->vs->doc_view->session->status.ecmascript_led->value = 'J';
#endif

	VALUE_TO_JSVAL_END(rval);
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
	struct view_state *vs = JS_GetPrivate(ctx, parent);
	VALUE_TO_JSVAL_START;

	if (!JSVAL_IS_INT(id))
		goto bye;

	switch (JSVAL_TO_INT(id)) {
	case JSP_LOC_HREF:
		p.string = get_uri_string(vs->uri, URI_ORIGINAL); prop_type = JSPT_ASTRING;
		break;
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
	struct view_state *vs = JS_GetPrivate(ctx, parent);
	struct document_view *doc_view = vs->doc_view;
	JSVAL_TO_VALUE_START;

	if (!JSVAL_IS_INT(id))
		goto bye;

	switch (JSVAL_TO_INT(id)) {
	case JSP_LOC_HREF:
		JSVAL_REQUIRE(vp, STRING);
		location_goto(doc_view, v.string);
		break;
	}

	JSVAL_TO_VALUE_END;
}

static JSBool location_toString(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

static const JSFunctionSpec location_funcs[] = {
	{ "toString",		location_toString,	0 },
	{ "toLocaleString",	location_toString,	0 },
	{ NULL }
};

static JSBool
location_toString(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_GetProperty(ctx, obj, "href", rval);
}

struct delayed_goto {
	/* It might look more convenient to pass doc_view around but it could
	 * disappear during wild dances inside of frames or so. */
	struct view_state *vs;
	struct uri *uri;
};

static void
delayed_goto(void *data)
{
	struct delayed_goto *deg = data;

	assert(deg);
	if (deg->vs->doc_view) {
		goto_uri_frame(deg->vs->doc_view->session, deg->uri,
		               deg->vs->doc_view->name,
			       CACHE_MODE_NORMAL);
	}
	done_uri(deg->uri);
	mem_free(deg);
}

static void
location_goto(struct document_view *doc_view, unsigned char *url)
{
	unsigned char *new_abs_url;
	struct uri *new_uri;
	struct delayed_goto *deg;

	new_abs_url = join_urls(doc_view->document->uri,
	                        trim_chars(url, ' ', 0));
	if (!new_abs_url)
		return;
	new_uri = get_uri(new_abs_url, 0);
	mem_free(new_abs_url);
	if (!new_uri)
		return;
	deg = mem_calloc(1, sizeof(struct delayed_goto));
	if (!deg) {
		done_uri(new_uri);
		return;
	}
	assert(doc_view->vs);
	deg->vs = doc_view->vs;
	deg->uri = new_uri;
	/* It does not seem to be very safe inside of frames to
	 * call goto_uri() right away. */
	register_bottom_half((void (*)(void *)) delayed_goto, deg);
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
	struct view_state *vs = JS_GetPrivate(ctx, parent);
	struct document_view *doc_view = vs->doc_view;
	struct session_status *status = &doc_view->session->status;
	unsigned char *bar = JS_GetPrivate(ctx, obj);
	VALUE_TO_JSVAL_START;

	if (!JSVAL_IS_INT(id))
		goto bye;

	switch (JSVAL_TO_INT(id)) {
	case JSP_UNIBAR_VISIBLE:
#define unibar_fetch(bar) \
	p.boolean = status->force_show_##bar##_bar >= 0 \
	            ? status->force_show_##bar##_bar \
	            : status->show_##bar##_bar
		switch (*bar) {
		case 's':
			unibar_fetch(status);
			break;
		case 't':
			unibar_fetch(title);
			break;
		default:
			p.boolean = 0;
			break;
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
	struct view_state *vs = JS_GetPrivate(ctx, parent);
	struct document_view *doc_view = vs->doc_view;
	struct session_status *status = &doc_view->session->status;
	unsigned char *bar = JS_GetPrivate(ctx, obj);
	JSVAL_TO_VALUE_START;

	if (!JSVAL_IS_INT(id))
		goto bye;

	switch (JSVAL_TO_INT(id)) {
	case JSP_UNIBAR_VISIBLE:
		JSVAL_REQUIRE(vp, BOOLEAN);
#define unibar_set(bar) \
	status->force_show_##bar##_bar = v.boolean;
		switch (*bar) {
		case 's':
			unibar_set(status);
			break;
		case 't':
			unibar_set(title);
			break;
		default:
			v.boolean = 0;
			break;
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
	struct terminal *term;

	assert(interpreter && interpreter->vs && interpreter->vs->doc_view
	       && interpreter->vs->doc_view->session
	       && interpreter->vs->doc_view->session->tab);
	if_assert_failed goto reported;

	term = interpreter->vs->doc_view->session->tab->term;

#ifdef CONFIG_LEDS
	interpreter->vs->doc_view->session->status.ecmascript_led->value = 'J';
#endif

	if (!get_opt_bool("ecmascript.error_reporting"))
		goto reported;

	msg_box(term, NULL, MSGBOX_FREE_TEXT,
		N_("JavaScript Error"), ALIGN_CENTER,
		report->linebuf && report->tokenptr
		?
		msg_text(term, N_("A script embedded in the current "
		         "document raised the following%s%s%s%s: "
		         "\n\n%s\n\n%s\n.%*s^%*s."),
		         JSREPORT_IS_STRICT(report->flags) ? " strict" : "",
		         JSREPORT_IS_EXCEPTION(report->flags) ? " exception" : "",
		         JSREPORT_IS_WARNING(report->flags) ? " warning" : "",
		         !report->flags ? " error" : "",
		         message,
			 report->linebuf,
			 report->tokenptr - report->linebuf - 2, " ",
			 strlen(report->linebuf) - (report->tokenptr - report->linebuf) - 1, " ")
		:
		msg_text(term, N_("A script embedded in the current "
		         "document raised the following%s%s%s%s: "
		         "\n\n%s"),
		         JSREPORT_IS_STRICT(report->flags) ? " strict" : "",
		         JSREPORT_IS_EXCEPTION(report->flags) ? " exception" : "",
		         JSREPORT_IS_WARNING(report->flags) ? " warning" : "",
		         !report->flags ? " error" : "",
		         message)
		,
		NULL, 1,
		N_("OK"), NULL, B_ENTER | B_ESC);

reported:
	/* Im clu'les. --pasky */
	JS_ClearPendingException(ctx);
}

static JSBool
safeguard(JSContext *ctx, JSScript *script)
{
	struct ecmascript_interpreter *interpreter = JS_GetContextPrivate(ctx);
	int max_exec_time = get_opt_int("ecmascript.max_exec_time");

	if (time(NULL) - interpreter->exec_start > max_exec_time) {
		struct terminal *term = interpreter->vs->doc_view->session->tab->term;

		/* A killer script! Alert! */
		msg_box(term, NULL, 0,
			N_("JavaScript Emergency"), ALIGN_CENTER,
			msg_text(term,
				 N_("A script embedded in the current document was running "
				    "for more than %d seconds in line. This probably means "
				    "there is a bug in the script and it could have halted "
			            "the whole ELinks. The script execution was interrupted."),
				 max_exec_time),
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
	JSObject *window_obj, *document_obj, *forms_obj, *location_obj,
	         *statusbar_obj, *menubar_obj;

	assert(interpreter);

	ctx = JS_NewContext(jsrt, 8192 /* Stack allocation chunk size */);
	if (!ctx)
		return NULL;
	interpreter->backend_data = ctx;
	JS_SetContextPrivate(ctx, interpreter);
	/* TODO: Make JSOPTION_STRICT and JSOPTION_WERROR configurable. */
#ifndef JSOPTION_COMPILE_N_GO
#define JSOPTION_COMPILE_N_GO 0 /* Older SM versions don't have it. */
#endif
	/* XXX: JSOPTION_COMPILE_N_GO will go (will it?) when we implement
	 * some kind of bytecode cache. (If we will ever do that.) */
	JS_SetOptions(ctx, JSOPTION_VAROBJFIX | JSOPTION_COMPILE_N_GO);
	JS_SetErrorReporter(ctx, error_reporter);

	window_obj = JS_NewObject(ctx, (JSClass *) &window_class, NULL, NULL);
	if (!window_obj) {
		spidermonkey_put_interpreter(interpreter);
		return NULL;
	}
	JS_InitStandardClasses(ctx, window_obj);
	JS_DefineProperties(ctx, window_obj, (JSPropertySpec *) window_props);
	JS_DefineFunctions(ctx, window_obj, (JSFunctionSpec *) window_funcs);
	JS_SetPrivate(ctx, window_obj, interpreter->vs);

	document_obj = JS_InitClass(ctx, window_obj, NULL,
				    (JSClass *) &document_class, NULL, 0,
				    (JSPropertySpec *) document_props,
				    (JSFunctionSpec *) document_funcs,
				    NULL, NULL);

	forms_obj = JS_InitClass(ctx, document_obj, NULL,
				    (JSClass *) &forms_class, NULL, 0,
				    (JSPropertySpec *) forms_props,
				    (JSFunctionSpec *) forms_funcs,
				    NULL, NULL);

	location_obj = JS_InitClass(ctx, window_obj, NULL,
				    (JSClass *) &location_class, NULL, 0,
				    (JSPropertySpec *) location_props,
				    (JSFunctionSpec *) location_funcs,
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
	if (!v.string) return NULL;

	return stracpy(v.string);
}
