/* The SpiderMonkey ECMAScript backend. */
/* $Id: spidermonkey.c,v 1.129 2004/12/19 13:09:52 pasky Exp $ */

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
#include "document/forms.h"
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
#include "viewer/text/draw.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
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

struct jsval_property {
	enum prop_type type;
	union {
		int boolean;
		int number;
		JSObject *object;
		unsigned char *string;
	} value;
};

#define P_UNDEF() do { \
	memset(&prop, 'J', sizeof(prop)); /* Active security ;) */\
	prop.type = JSPT_UNDEF; \
	if (0) goto bye;	/* Prevent unused label errors */ \
} while (0)

#define P_OBJECT(x) do { prop.value.object = (x); prop.type = JSPT_OBJECT; } while (0)
#define P_BOOLEAN(x) do { prop.value.boolean = (x); prop.type = JSPT_BOOLEAN; } while (0)
#define P_STRING(x) do { prop.value.string = (x); prop.type = JSPT_STRING; } while (0)
#define P_ASTRING(x) do { prop.value.string = (x); prop.type = JSPT_ASTRING; } while (0)
#define P_INT(x) do { prop.value.number = (x); prop.type = JSPT_INT; } while (0)


#define VALUE_TO_JSVAL_START \
	struct jsval_property prop; \
 \
	P_UNDEF();

#define VALUE_TO_JSVAL_END(vp) \
	value_to_jsval(ctx, vp, &prop); \
 \
bye: \
	return JS_TRUE;


static void
value_to_jsval(JSContext *ctx, jsval *vp, struct jsval_property *prop)
{
	switch (prop->type) {
	case JSPT_STRING:
	case JSPT_ASTRING:
		if (!prop->value.string) {
			*vp = JSVAL_NULL;
			break;
		}
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(ctx, prop->value.string));
		if (prop->type == JSPT_ASTRING)
			mem_free(prop->value.string);
		break;

	case JSPT_BOOLEAN:
		*vp = BOOLEAN_TO_JSVAL(prop->value.boolean);
		break;

	case JSPT_INT:
		*vp = INT_TO_JSVAL(prop->value.number);
		break;

	case JSPT_OBJECT:
		*vp = OBJECT_TO_JSVAL(prop->value.object);
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
		P_OBJECT(obj);
		goto convert;
	} else if (!JSVAL_IS_INT(id))
		goto bye;

	switch (JSVAL_TO_INT(id)) {
	case JSP_WIN_CLOSED:
		/* TODO: It will be a major PITA to implement this properly.
		 * Well, perhaps not so much if we introduce reference tracking
		 * for (struct session)? Still... --pasky */
		P_BOOLEAN(0);
		break;
	case JSP_WIN_SELF:
		P_OBJECT(obj);
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
		break;

found_parent:
		if (doc_view->vs.ecmascript_fragile)
			ecmascript_reset_state(&doc_view->vs);
		assert(doc_view->ecmascript);
		P_OBJECT(JS_GetGlobalObject(doc_view->ecmascript->backend_data));
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
		P_OBJECT(JS_GetGlobalObject(top_view->vs->ecmascript->backend_data));
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
	VALUE_TO_JSVAL_START;

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
	static time_t ratelimit_start;
	static int ratelimit_count;
	VALUE_TO_JSVAL_START;

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
		P_BOOLEAN(1);
	} else {
		/* When opening a new tab, we might get rerendered, losing our
		 * context and triggerring a disaster, so postpone that. */
		struct delayed_open *deo = mem_calloc(1, sizeof(*deo));

		if (deo) {
			deo->ses = ses;
			deo->uri = get_uri_reference(uri);
			register_bottom_half((void (*)(void *)) delayed_open,
			                     deo);
			P_BOOLEAN(1);
		}
	}

	done_uri(uri);

	VALUE_TO_JSVAL_END(rval);
}


/* Accordingly to the JS specs, each input type should own object. That'd be a
 * huge PITA though, however DOM comes to the rescue and defines just a single
 * HTMLInputElement. The difference could be spotted only by some clever tricky
 * JS code, but I hope it doesn't matter anywhere. --pasky */

static JSBool input_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);
static JSBool input_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);

static const JSClass input_class = {
	"input", /* here, we unleash ourselves */
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	input_get_property, input_set_property,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

enum input_prop {
	JSP_INPUT_ACCESSKEY,
	JSP_INPUT_ALT,
	JSP_INPUT_CHECKED,
	JSP_INPUT_DEFAULT_CHECKED,
	JSP_INPUT_DEFAULT_VALUE,
	JSP_INPUT_DISABLED,
	JSP_INPUT_FORM,
	JSP_INPUT_MAX_LENGTH,
	JSP_INPUT_NAME,
	JSP_INPUT_READONLY,
	JSP_INPUT_SIZE,
	JSP_INPUT_SRC,
	JSP_INPUT_TABINDEX,
	JSP_INPUT_TYPE,
	JSP_INPUT_VALUE
};

/* XXX: Some of those are marked readonly just because we can't change them
 * safely now. Changing default* values would affect all open instances of the
 * document, leading to a potential security risk. Changing size and type would
 * require re-rendering the document (TODO), tabindex would require renumbering
 * of all links and whatnot. --pasky */
static const JSPropertySpec input_props[] = {
	{ "accessKey",	JSP_INPUT_ACCESSKEY,	JSPROP_ENUMERATE },
	{ "alt",	JSP_INPUT_ALT,		JSPROP_ENUMERATE },
	{ "checked",	JSP_INPUT_CHECKED,	JSPROP_ENUMERATE },
	{ "defaultChecked",JSP_INPUT_DEFAULT_CHECKED,JSPROP_ENUMERATE },
	{ "defaultValue",JSP_INPUT_DEFAULT_VALUE,JSPROP_ENUMERATE },
	{ "disabled",	JSP_INPUT_DISABLED,	JSPROP_ENUMERATE },
	{ "form",	JSP_INPUT_FORM,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "maxLength",	JSP_INPUT_MAX_LENGTH,	JSPROP_ENUMERATE },
	{ "name",	JSP_INPUT_NAME,		JSPROP_ENUMERATE },
	{ "readonly",	JSP_INPUT_READONLY,	JSPROP_ENUMERATE },
	{ "size",	JSP_INPUT_SIZE,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "src",	JSP_INPUT_SRC,		JSPROP_ENUMERATE },
	{ "tabindex",	JSP_INPUT_TABINDEX,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "type",	JSP_INPUT_TYPE,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "value",	JSP_INPUT_VALUE,	JSPROP_ENUMERATE },
	{ NULL }
};

static JSBool input_blur(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool input_click(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool input_focus(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool input_select(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

static const JSFunctionSpec input_funcs[] = {
	{ "blur",	input_blur,	0 },
	{ "click",	input_click,	0 },
	{ "focus",	input_focus,	0 },
	{ "select",	input_select,	0 },
	{ NULL }
};

static JSBool
input_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *parent_form = JS_GetParent(ctx, obj);
	JSObject *parent_doc = JS_GetParent(ctx, parent_form);
	JSObject *parent_win = JS_GetParent(ctx, parent_doc);
	struct view_state *vs = JS_GetPrivate(ctx, parent_win);
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct form_state *fs = JS_GetPrivate(ctx, obj);
	struct form_control *fc = find_form_control(document, fs);
	int linknum;
	struct link *link = NULL;
	VALUE_TO_JSVAL_START;

	assert(fc);
	assert(fc->form && fs);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum >= 0) link = &document->links[linknum];

	if (!JSVAL_IS_INT(id))
		goto bye;

	switch (JSVAL_TO_INT(id)) {
	case JSP_INPUT_ACCESSKEY:
	{
		struct string keystr;

		if (!link) {
			P_UNDEF();
			break;
		}
		init_string(&keystr);
		make_keystroke(&keystr, link->accesskey, 0, 0);
		P_STRING(keystr.source);
		done_string(&keystr);
		break;
	}
	case JSP_INPUT_ALT:
		P_STRING(fc->alt);
		break;
	case JSP_INPUT_CHECKED:
		P_BOOLEAN(fs->state);
		break;
	case JSP_INPUT_DEFAULT_CHECKED:
		P_BOOLEAN(fc->default_state);
		break;
	case JSP_INPUT_DEFAULT_VALUE:
		P_STRING(fc->default_value);
		break;
	case JSP_INPUT_DISABLED:
		/* FIXME: <input readonly disabled> --pasky */
		P_BOOLEAN(fc->mode == FORM_MODE_DISABLED);
		break;
	case JSP_INPUT_FORM:
		P_OBJECT(parent_form);
		break;
	case JSP_INPUT_MAX_LENGTH:
		P_INT(fc->maxlength);
		break;
	case JSP_INPUT_NAME:
		P_STRING(fc->name);
		break;
	case JSP_INPUT_READONLY:
		/* FIXME: <input readonly disabled> --pasky */
		P_BOOLEAN(fc->mode == FORM_MODE_READONLY);
		break;
	case JSP_INPUT_SIZE:
		P_INT(fc->size);
		break;
	case JSP_INPUT_SRC:
		if (link && link->where_img)
			P_STRING(link->where_img);
		else
			P_UNDEF();
		break;
	case JSP_INPUT_TABINDEX:
		if (link)
			/* FIXME: This is WRONG. --pasky */
			P_INT(link->number);
		else
			P_UNDEF();
		break;
	case JSP_INPUT_TYPE:
		switch (fc->type) {
		case FC_TEXT: P_STRING("text"); break;
		case FC_PASSWORD: P_STRING("password"); break;
		case FC_FILE: P_STRING("file"); break;
		case FC_CHECKBOX: P_STRING("checkbox"); break;
		case FC_RADIO: P_STRING("radio"); break;
		case FC_SUBMIT: P_STRING("submit"); break;
		case FC_IMAGE: P_STRING("image"); break;
		case FC_RESET: P_STRING("reset"); break;
		case FC_BUTTON: P_STRING("button"); break;
		case FC_HIDDEN: P_STRING("hidden"); break;
		default: INTERNAL("input_get_property() upon a non-input item."); break;
		}
		break;
	case JSP_INPUT_VALUE:
		P_STRING(fs->value);
		break;

	default:
		INTERNAL("Invalid ID %d in input_get_property().", JSVAL_TO_INT(id));
		goto bye;
	}

	VALUE_TO_JSVAL_END(vp);
}

static JSBool
input_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *parent_form = JS_GetParent(ctx, obj);
	JSObject *parent_doc = JS_GetParent(ctx, parent_form);
	JSObject *parent_win = JS_GetParent(ctx, parent_doc);
	struct view_state *vs = JS_GetPrivate(ctx, parent_win);
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct form_state *fs = JS_GetPrivate(ctx, obj);
	struct form_control *fc = find_form_control(document, fs);
	int linknum;
	struct link *link = NULL;
	JSVAL_TO_VALUE_START;

	assert(fc);
	assert(fc->form && fs);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum >= 0) link = &document->links[linknum];

	if (!JSVAL_IS_INT(id))
		goto bye;

	switch (JSVAL_TO_INT(id)) {
	case JSP_INPUT_ACCESSKEY:
		JSVAL_REQUIRE(vp, STRING);
		if (link) link->accesskey = read_key(v.string);
		break;
	case JSP_INPUT_ALT:
		JSVAL_REQUIRE(vp, STRING);
		mem_free_set(fc->alt, stracpy(v.string));
		break;
	case JSP_INPUT_CHECKED:
		JSVAL_REQUIRE(vp, BOOLEAN);
		fs->state = v.boolean;
		break;
	case JSP_INPUT_DISABLED:
		/* FIXME: <input readonly disabled> --pasky */
		JSVAL_REQUIRE(vp, BOOLEAN);
		fc->mode = (v.boolean ? FORM_MODE_DISABLED
		                      : fc->mode == FORM_MODE_READONLY ? FORM_MODE_READONLY
		                                                       : FORM_MODE_NORMAL);
		break;
	case JSP_INPUT_MAX_LENGTH:
		JSVAL_REQUIRE(vp, STRING);
		fc->maxlength = atol(v.string);
		break;
	case JSP_INPUT_NAME:
		JSVAL_REQUIRE(vp, STRING);
		mem_free_set(fc->name, stracpy(v.string));
		break;
	case JSP_INPUT_READONLY:
		/* FIXME: <input readonly disabled> --pasky */
		JSVAL_REQUIRE(vp, BOOLEAN);
		fc->mode = (v.boolean ? FORM_MODE_READONLY
		                      : fc->mode == FORM_MODE_DISABLED ? FORM_MODE_DISABLED
		                                                       : FORM_MODE_NORMAL);
		break;
	case JSP_INPUT_SRC:
		if (link) {
			JSVAL_REQUIRE(vp, STRING);
			mem_free_set(link->where_img, stracpy(v.string));
		}
		break;
	case JSP_INPUT_VALUE:
		JSVAL_REQUIRE(vp, BOOLEAN);
		mem_free_set(fs->value, stracpy(v.string));
		break;

	default:
		INTERNAL("Invalid ID %d in input_set_property().", JSVAL_TO_INT(id));
		goto bye;
	}

	JSVAL_TO_VALUE_END;
}

static JSBool
input_blur(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	/* We are a text-mode browser and there *always* has to be something
	 * selected.  So we do nothing for now. (That was easy.) */
	return JS_TRUE;
}

static JSBool
input_click(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject *parent_form = JS_GetParent(ctx, obj);
	JSObject *parent_doc = JS_GetParent(ctx, parent_form);
	JSObject *parent_win = JS_GetParent(ctx, parent_doc);
	struct view_state *vs = JS_GetPrivate(ctx, parent_win);
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct session *ses = doc_view->session;
	struct form_state *fs = JS_GetPrivate(ctx, obj);
	struct form_control *fc;
	int linknum;
	VALUE_TO_JSVAL_START;

	P_BOOLEAN(0);

	assert(fc);
	fc = find_form_control(document, fs);
	assert(fc);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum < 0)
		goto bye;

	/* Restore old current_link afterwards? */
	jump_to_link_number(ses, doc_view, linknum);
	if (enter(ses, doc_view, 0) == FRAME_EVENT_REFRESH)
		refresh_view(ses, doc_view, 0);
	else
		print_screen_status(ses);

	VALUE_TO_JSVAL_END(rval);
}

static JSBool
input_focus(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject *parent_form = JS_GetParent(ctx, obj);
	JSObject *parent_doc = JS_GetParent(ctx, parent_form);
	JSObject *parent_win = JS_GetParent(ctx, parent_doc);
	struct view_state *vs = JS_GetPrivate(ctx, parent_win);
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct session *ses = doc_view->session;
	struct form_state *fs = JS_GetPrivate(ctx, obj);
	struct form_control *fc;
	int linknum;
	VALUE_TO_JSVAL_START;

	P_BOOLEAN(0);

	assert(fs);
	fc = find_form_control(document, fs);
	assert(fc);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum < 0)
		goto bye;

	jump_to_link_number(ses, doc_view, linknum);

	VALUE_TO_JSVAL_END(rval);
}

static JSBool
input_select(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	/* We support no text selecting yet.  So we do nothing for now.
	 * (That was easy, too.) */
	return JS_TRUE;
}

static JSObject *
get_input_object(JSContext *ctx, JSObject *jsform, struct form_state *fs)
{
	if (!fs->ecmascript_obj) {
		/* jsform ('form') is input's parent */
		/* FIXME: That is NOT correct since the real containing element
		 * should be its parent, but gimme DOM first. --pasky */
		JSObject *jsinput = JS_NewObject(ctx, (JSClass *) &input_class, NULL, jsform);

		JS_DefineProperties(ctx, jsinput, (JSPropertySpec *) input_props);
		JS_DefineFunctions(ctx, jsinput, (JSFunctionSpec *) input_funcs);
		JS_SetPrivate(ctx, jsinput, fs);
		fs->ecmascript_obj = jsinput;
	}
	return fs->ecmascript_obj;
}


static JSObject *
get_form_control_object(JSContext *ctx, JSObject *jsform, enum form_type type, struct form_state *fs)
{
	switch (type) {
		case FC_TEXT:
		case FC_PASSWORD:
		case FC_FILE:
		case FC_CHECKBOX:
		case FC_RADIO:
		case FC_SUBMIT:
		case FC_IMAGE:
		case FC_RESET:
		case FC_BUTTON:
		case FC_HIDDEN:
			return get_input_object(ctx, jsform, fs);

		case FC_TEXTAREA:
		case FC_SELECT:
			/* TODO */
			return NULL;

		default:
			INTERNAL("Weird fc->type %d", type);
			return NULL;
	}
}



static JSBool form_elements_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);

static const JSClass form_elements_class = {
	"elements",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	form_elements_get_property, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

static JSBool form_elements_item(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool form_elements_namedItem(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

static const JSFunctionSpec form_elements_funcs[] = {
	{ "item",		form_elements_item,		1 },
	{ "namedItem",		form_elements_namedItem,	1 },
	{ NULL }
};

/* INTs from 0 up are equivalent to item(INT), so we have to stuff length out
 * of the way. */
enum form_elements_prop { JSP_FORM_ELEMENTS_LENGTH = -1 };
static const JSPropertySpec form_elements_props[] = {
	{ "length",	JSP_FORM_ELEMENTS_LENGTH,	JSPROP_ENUMERATE | JSPROP_READONLY},
	{ NULL }
};

static JSBool
form_elements_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *parent_form = JS_GetParent(ctx, obj);
	JSObject *parent_doc = JS_GetParent(ctx, parent_form);
	JSObject *parent_win = JS_GetParent(ctx, parent_doc);
	struct view_state *vs = JS_GetPrivate(ctx, parent_win);
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct form_view *form_view = JS_GetPrivate(ctx, parent_form);
	struct form *form = find_form_by_form_view(document, form_view);
	VALUE_TO_JSVAL_START;

	if (JSVAL_IS_STRING(id)) {
		form_elements_namedItem(ctx, obj, 1, &id, vp);
		goto bye;
	} else if (!JSVAL_IS_INT(id))
		goto bye;

	switch (JSVAL_TO_INT(id)) {
	case JSP_FORM_ELEMENTS_LENGTH:
	{
		struct form_control *fc;
		int counter = 0;

		foreach (fc, form->items)
			counter++;

		P_INT(counter);
		break;
	}
	default:
		/* Array index. */
		form_elements_item(ctx, obj, 1, &id, vp);
		goto bye;
	}

	VALUE_TO_JSVAL_END(vp);
}

static JSBool
form_elements_item(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject *parent_form = JS_GetParent(ctx, obj);
	JSObject *parent_doc = JS_GetParent(ctx, parent_form);
	JSObject *parent_win = JS_GetParent(ctx, parent_doc);
	struct view_state *vs = JS_GetPrivate(ctx, parent_win);
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct form_view *form_view = JS_GetPrivate(ctx, parent_form);
	struct form *form = find_form_by_form_view(document, form_view);
	struct form_control *fc;
	union jsval_union v;
	int counter = -1;
	int index;
	VALUE_TO_JSVAL_START;

	if (argc != 1)
		goto bye;

	JSVAL_REQUIRE(&argv[0], STRING);
	index = atol(v.string);

	foreach (fc, form->items) {
		counter++;
		if (counter == index) {
			JSObject *fcobj = get_form_control_object(ctx, parent_form, fc->type, find_form_state(doc_view, fc));

			if (fcobj) {
				P_OBJECT(fcobj);
			} else {
				P_UNDEF();
			}

			VALUE_TO_JSVAL_END(rval);
			/* This returns. */
		}
	}

	goto bye;
}

static JSBool
form_elements_namedItem(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject *parent_form = JS_GetParent(ctx, obj);
	JSObject *parent_doc = JS_GetParent(ctx, parent_form);
	JSObject *parent_win = JS_GetParent(ctx, parent_doc);
	struct view_state *vs = JS_GetPrivate(ctx, parent_win);
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct form_view *form_view = JS_GetPrivate(ctx, parent_form);
	struct form *form = find_form_by_form_view(document, form_view);
	struct form_control *fc;
	union jsval_union v;
	VALUE_TO_JSVAL_START;

	if (argc != 1)
		goto bye;

	JSVAL_REQUIRE(&argv[0], STRING);
	if (!v.string || !*v.string)
		goto bye;

	foreach (fc, form->items) {
		if (fc->name && !strcasecmp(v.string, fc->name)) {
			JSObject *fcobj = get_form_control_object(ctx, parent_form, fc->type, find_form_state(doc_view, fc));

			if (fcobj) {
				P_OBJECT(fcobj);
			} else {
				P_UNDEF();
			}

			VALUE_TO_JSVAL_END(rval);
			/* This returns. */
		}
	}

	goto bye;
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
	JSP_FORM_ACTION,
	JSP_FORM_ELEMENTS,
	JSP_FORM_ENCODING,
	JSP_FORM_LENGTH,
	JSP_FORM_METHOD,
	JSP_FORM_NAME,
	JSP_FORM_TARGET
};

static const JSPropertySpec form_props[] = {
	{ "action",	JSP_FORM_ACTION,	JSPROP_ENUMERATE },
	{ "elements",	JSP_FORM_ELEMENTS,	JSPROP_ENUMERATE },
	{ "encoding",	JSP_FORM_ENCODING,	JSPROP_ENUMERATE },
	{ "length",	JSP_FORM_LENGTH,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "method",	JSP_FORM_METHOD,	JSPROP_ENUMERATE },
	{ "name",	JSP_FORM_NAME,		JSPROP_ENUMERATE },
	{ "target",	JSP_FORM_TARGET,	JSPROP_ENUMERATE },
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
	/* DBG("doc %p %s\n", parent_doc, JS_GetStringBytes(JS_ValueToString(ctx, OBJECT_TO_JSVAL(parent_doc)))); */
	JSObject *parent_doc = JS_GetParent(ctx, obj);
	JSObject *parent_win = JS_GetParent(ctx, parent_doc);
	struct view_state *vs = JS_GetPrivate(ctx, parent_win);
	struct document_view *doc_view = vs->doc_view;
	struct form_view *fv = JS_GetPrivate(ctx, obj);
	struct form *form = find_form_by_form_view(doc_view->document, fv);
	VALUE_TO_JSVAL_START;

	assert(form);

	if (JSVAL_IS_STRING(id)) {
		struct form_control *fc;
		JSVAL_TO_VALUE_START;

		JSVAL_REQUIRE(&id, STRING);
		foreach (fc, form->items) {
			JSObject *fcobj = NULL;

			if (!fc->name || strcasecmp(v.string, fc->name))
				continue;

			fcobj = get_form_control_object(ctx, obj, fc->type, find_form_state(doc_view, fc));
			if (fcobj) {
				P_OBJECT(fcobj);
			} else {
				P_UNDEF();
			}
			goto convert;
		}
		goto bye;
	} else if (!JSVAL_IS_INT(id))
		goto bye;

	switch (JSVAL_TO_INT(id)) {
	case JSP_FORM_ACTION:
		P_STRING(form->action);
		break;

	case JSP_FORM_ELEMENTS:
	{
		/* jsform ('form') is form_elements' parent; who knows is that's correct */
		JSObject *jsform_elems = JS_NewObject(ctx, (JSClass *) &form_elements_class, NULL, obj);

		JS_DefineProperties(ctx, jsform_elems, (JSPropertySpec *) form_elements_props);
		JS_DefineFunctions(ctx, jsform_elems, (JSFunctionSpec *) form_elements_funcs);
		P_OBJECT(jsform_elems);
		/* SM will cache this property value for us so we create this
		 * just once per form. */
	}
		break;

	case JSP_FORM_ENCODING:
		switch (form->method) {
		case FORM_METHOD_GET:
		case FORM_METHOD_POST:
			P_STRING("application/x-www-form-urlencoded");
			break;
		case FORM_METHOD_POST_MP:
			P_STRING("multipart/form-data");
			break;
		case FORM_METHOD_POST_TEXT_PLAIN:
			P_STRING("text/plain");
			break;
		}
		break;

	case JSP_FORM_LENGTH:
	{
		struct form_control *fc;
		int counter;

		foreach (fc, form->items)
			counter++;
		P_INT(counter);
		break;
	}

	case JSP_FORM_METHOD:
		switch (form->method) {
		case FORM_METHOD_GET:
			P_STRING("GET");
			break;

		case FORM_METHOD_POST:
		case FORM_METHOD_POST_MP:
		case FORM_METHOD_POST_TEXT_PLAIN:
			P_STRING("POST");
			break;
		}
		break;

	case JSP_FORM_NAME:
		P_STRING(form->name);
		break;

	case JSP_FORM_TARGET:
		P_STRING(form->target);
		break;

	default:
		INTERNAL("Invalid ID %d in form_get_property().", JSVAL_TO_INT(id));
		goto bye;
	}

convert:
	VALUE_TO_JSVAL_END(vp);
}

static JSBool
form_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *parent_doc = JS_GetParent(ctx, obj);
	JSObject *parent_win = JS_GetParent(ctx, parent_doc);
	struct view_state *vs = JS_GetPrivate(ctx, parent_win);
	struct document_view *doc_view = vs->doc_view;
	struct form_view *fv = JS_GetPrivate(ctx, obj);
	struct form *form = find_form_by_form_view(doc_view->document, fv);
	JSVAL_TO_VALUE_START;

	assert(form);

	if (!JSVAL_IS_INT(id))
		goto bye;

	switch (JSVAL_TO_INT(id)) {
	case JSP_FORM_ACTION:
		JSVAL_REQUIRE(vp, STRING);
		mem_free_set(form->action, stracpy(v.string));
		break;

	case JSP_FORM_ENCODING:
		JSVAL_REQUIRE(vp, STRING);
		if (!strcasecmp(v.string, "application/x-www-form-urlencoded")) {
			form->method = form->method == FORM_METHOD_GET ? FORM_METHOD_GET
			                                               : FORM_METHOD_POST;
		} else if (!strcasecmp(v.string, "multipart/form-data")) {
			form->method = FORM_METHOD_POST_MP;
		} else if (!strcasecmp(v.string, "text/plain")) {
			form->method = FORM_METHOD_POST_TEXT_PLAIN;
		}
		break;

	case JSP_FORM_METHOD:
		JSVAL_REQUIRE(vp, STRING);
		if (!strcasecmp(v.string, "GET")) {
			form->method = FORM_METHOD_GET;
		} else if (!strcasecmp(v.string, "POST")) {
			form->method = FORM_METHOD_POST;
		}
		break;

	case JSP_FORM_NAME:
		JSVAL_REQUIRE(vp, STRING);
		mem_free_set(form->name, stracpy(v.string));
		break;

	case JSP_FORM_TARGET:
		JSVAL_REQUIRE(vp, STRING);
		mem_free_set(form->target, stracpy(v.string));
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
	JSObject *parent_doc = JS_GetParent(ctx, obj);
	JSObject *parent_win = JS_GetParent(ctx, parent_doc);
	struct view_state *vs = JS_GetPrivate(ctx, parent_win);
	struct document_view *doc_view = vs->doc_view;
	struct form_view *fv = JS_GetPrivate(ctx, obj);
	struct form *form = find_form_by_form_view(doc_view->document, fv);
	VALUE_TO_JSVAL_START;

	P_BOOLEAN(0);

	assert(form);

	do_reset_form(doc_view, form);
	draw_forms(doc_view->session->tab->term, doc_view);

	VALUE_TO_JSVAL_END(rval);
}

static JSBool
form_submit(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject *parent_doc = JS_GetParent(ctx, obj);
	JSObject *parent_win = JS_GetParent(ctx, parent_doc);
	struct view_state *vs = JS_GetPrivate(ctx, parent_win);
	struct document_view *doc_view = vs->doc_view;
	struct session *ses = doc_view->session;
	struct form_view *fv = JS_GetPrivate(ctx, obj);
	struct form *form = find_form_by_form_view(doc_view->document, fv);
	VALUE_TO_JSVAL_START;

	P_BOOLEAN(0);

	assert(form);
	submit_given_form(ses, doc_view, form);

	VALUE_TO_JSVAL_END(rval);
}

static JSObject *
get_form_object(JSContext *ctx, JSObject *jsdoc, struct form_view *fv)
{
	if (!fv->ecmascript_obj) {
		/* jsdoc ('document') is fv's parent */
		/* FIXME: That is NOT correct since the real containing element
		 * should be its parent, but gimme DOM first. --pasky */
		JSObject *jsform = JS_NewObject(ctx, (JSClass *) &form_class, NULL, jsdoc);

		JS_DefineProperties(ctx, jsform, (JSPropertySpec *) form_props);
		JS_DefineFunctions(ctx, jsform, (JSFunctionSpec *) form_funcs);
		JS_SetPrivate(ctx, jsform, fv);
		fv->ecmascript_obj = jsform;
	}
	return fv->ecmascript_obj;
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

/* INTs from 0 up are equivalent to item(INT), so we have to stuff length out
 * of the way. */
enum forms_prop { JSP_FORMS_LENGTH = -1 };
static const JSPropertySpec forms_props[] = {
	{ "length",	JSP_FORMS_LENGTH,	JSPROP_ENUMERATE | JSPROP_READONLY},
	{ NULL }
};

static JSBool
forms_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *parent_doc = JS_GetParent(ctx, obj);
	JSObject *parent_win = JS_GetParent(ctx, parent_doc);
	struct view_state *vs = JS_GetPrivate(ctx, parent_win);
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	VALUE_TO_JSVAL_START;

	if (JSVAL_IS_STRING(id)) {
		forms_namedItem(ctx, obj, 1, &id, vp);
		goto bye;
	} else if (!JSVAL_IS_INT(id))
		goto bye;

	switch (JSVAL_TO_INT(id)) {
	case JSP_FORMS_LENGTH:
	{
		struct form *form;
		int counter = 0;

		foreach (form, document->forms)
			counter++;

		P_INT(counter);
		break;
	}
	default:
		/* Array index. */
		forms_item(ctx, obj, 1, &id, vp);
		goto bye;
	}

	VALUE_TO_JSVAL_END(vp);
}

static JSBool
forms_item(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject *parent_doc = JS_GetParent(ctx, obj);
	JSObject *parent_win = JS_GetParent(ctx, parent_doc);
	struct view_state *vs = JS_GetPrivate(ctx, parent_win);
	struct form_view *fv;
	union jsval_union v;
	int counter = -1;
	int index;
	VALUE_TO_JSVAL_START;

	if (argc != 1)
		goto bye;

	JSVAL_REQUIRE(&argv[0], STRING);
	index = atol(v.string);

	foreach (fv, vs->forms) {
		counter++;
		if (counter == index) {
			P_OBJECT(get_form_object(ctx, parent_doc, fv));

			VALUE_TO_JSVAL_END(rval);
			/* This returns. */
		}
	}

	goto bye;
}

static JSBool
forms_namedItem(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject *parent_doc = JS_GetParent(ctx, obj);
	JSObject *parent_win = JS_GetParent(ctx, parent_doc);
	struct view_state *vs = JS_GetPrivate(ctx, parent_win);
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct form *form;
	union jsval_union v;
	VALUE_TO_JSVAL_START;

	if (argc != 1)
		goto bye;

	JSVAL_REQUIRE(&argv[0], STRING);
	if (!v.string || !*v.string)
		goto bye;

	foreach (form, document->forms) {
		if (form->name && !strcasecmp(v.string, form->name)) {
			P_OBJECT(get_form_object(ctx, parent_doc, find_form_view(doc_view, form)));

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
		struct form *form;
		JSVAL_TO_VALUE_START;

		JSVAL_REQUIRE(&id, STRING);
#ifdef CONFIG_COOKIES
		if (!strcmp(v.string, "cookie")) {
			struct string *cookies = send_cookies(vs->uri);

			if (cookies) {
				P_STRING(cookies->source);
				goto convert;
			}
		}
#endif
		foreach (form, document->forms) {
			if (!form->name || strcasecmp(v.string, form->name))
				continue;

			P_OBJECT(get_form_object(ctx, obj, find_form_view(doc_view, form)));
			goto convert;
		}
		goto bye;
	} else if (!JSVAL_IS_INT(id))
		goto bye;

	switch (JSVAL_TO_INT(id)) {
	case JSP_DOC_REF:
		switch (get_opt_int("protocol.http.referer.policy")) {
		case REFERER_NONE:
			/* oh well */
			break;

		case REFERER_FAKE:
			P_STRING(get_opt_str("protocol.http.referer.fake"));
			break;

		case REFERER_TRUE:
			/* XXX: Encode as in add_url_to_httP_STRING() ? --pasky */
			if (ses->referrer) {
				P_ASTRING(get_uri_string(ses->referrer, URI_HTTP_REFERRER));
			}
			break;

		case REFERER_SAME_URL:
			P_ASTRING(get_uri_string(document->uri, URI_HTTP_REFERRER));
			break;
		}
		break;
	case JSP_DOC_TITLE:
		P_STRING(document->title);
		break;
	case JSP_DOC_URL:
		P_ASTRING(get_uri_string(document->uri, URI_ORIGINAL));
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

	P_BOOLEAN(0);

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
		P_ASTRING(get_uri_string(vs->uri, URI_ORIGINAL));
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
	P_BOOLEAN(status->force_show_##bar##_bar >= 0 \
	          ? status->force_show_##bar##_bar \
	          : status->show_##bar##_bar)
		switch (*bar) {
		case 's':
			unibar_fetch(status);
			break;
		case 't':
			unibar_fetch(title);
			break;
		default:
			P_BOOLEAN(0);
			break;
		}
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
	unsigned char *strict, *exception, *warning, *error, *msg;

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

	strict	  = JSREPORT_IS_STRICT(report->flags) ? " strict" : "";
	exception = JSREPORT_IS_EXCEPTION(report->flags) ? " exception" : "";
	warning   = JSREPORT_IS_WARNING(report->flags) ? " warning" : "";
	error	  = !report->flags ? " error" : "";

	if (report->linebuf && report->tokenptr) {
		int pos = report->tokenptr - report->linebuf;

		msg = msg_text(term, N_("A script embedded in the current "
			       "document raised the following%s%s%s%s: "
			       "\n\n%s\n\n%s\n.%*s^%*s."),
			       strict, exception, warning, error, message,
			       report->linebuf,
			       pos - 2, " ",
			       strlen(report->linebuf) - pos - 1, " ");
	} else {
		msg = msg_text(term, N_("A script embedded in the current "
			       "document raised the following%s%s%s%s: "
			       "\n\n%s"),
			       strict, exception, warning, error, message);
	}

	msg_box(term, NULL, MSGBOX_FREE_TEXT,
		N_("JavaScript Error"), ALIGN_CENTER,
		msg, NULL, 1,
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
		msg_box(term, NULL, MSGBOX_FREE_TEXT,
			N_("JavaScript Emergency"), ALIGN_CENTER,
			msg_text(term,
				 N_("A script embedded in the current document was running "
				    "for more than %d seconds in line.\n"
				    "This probably means there is a bug in the script and "
				    "it could have halted the whole ELinks.\n"
				    "The script execution was interrupted."),
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
