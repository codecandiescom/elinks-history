/* The SpiderMonkey ECMAScript backend. */
/* $Id: spidermonkey.c,v 1.195 2005/02/20 22:45:05 jonas Exp $ */

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
#include "lowlevel/sysname.h"
#include "osdep/newwin.h"
#include "protocol/http/http.h"
#include "protocol/uri.h"
#include "sched/session.h"
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

static void
string_to_jsval(JSContext *ctx, jsval *vp, unsigned char *string)
{
	if (!string) {
		*vp = JSVAL_NULL;
	} else {
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(ctx, string));
	}
}

static void
astring_to_jsval(JSContext *ctx, jsval *vp, unsigned char *string)
{
	string_to_jsval(ctx, vp, string);
	mem_free_if(string);
}

static void
int_to_jsval(JSContext *ctx, jsval *vp, int number)
{
	*vp = INT_TO_JSVAL(number);
}

static void
object_to_jsval(JSContext *ctx, jsval *vp, JSObject *object)
{
	*vp = OBJECT_TO_JSVAL(object);
}

static void
boolean_to_jsval(JSContext *ctx, jsval *vp, int boolean)
{
	*vp = BOOLEAN_TO_JSVAL(boolean);
}

static void
undef_to_jsval(JSContext *ctx, jsval *vp)
{
	*vp = JSVAL_NULL;
}


static int
jsval_to_boolean(JSContext *ctx, jsval *vp)
{
	jsval val;

	if (JS_ConvertValue(ctx, *vp, JSTYPE_BOOLEAN, &val) == JS_FALSE) {
		return JS_FALSE;
	}

	return JSVAL_TO_BOOLEAN(val);
}

static unsigned char *
jsval_to_string(JSContext *ctx, jsval *vp)
{
	jsval val;

	if (JS_ConvertValue(ctx, *vp, JSTYPE_STRING, &val) == JS_FALSE) {
		return "";
	}

	return empty_string_or_(JS_GetStringBytes(JS_ValueToString(ctx, val)));
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

	/* No need for special window.location measurements - when
	 * location is then evaluated in string context, toString()
	 * is called which we overrode for that class below, so
	 * everything's fine. */
	if (JSVAL_IS_STRING(id)) {
		struct document_view *doc_view = vs->doc_view;
		JSObject *obj;

		obj = try_resolve_frame(doc_view, jsval_to_string(ctx, &id));
		/* TODO: Try other lookups (mainly element lookup) until
		 * something yields data. */
		if (obj) {
			object_to_jsval(ctx, vp, obj);
		}
		return JS_TRUE;
	}

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	undef_to_jsval(ctx, vp);

	switch (JSVAL_TO_INT(id)) {
	case JSP_WIN_CLOSED:
		/* TODO: It will be a major PITA to implement this properly.
		 * Well, perhaps not so much if we introduce reference tracking
		 * for (struct session)? Still... --pasky */
		boolean_to_jsval(ctx, vp, 0);
		break;
	case JSP_WIN_SELF:
		object_to_jsval(ctx, vp, obj);
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
		some_domain_security_check();
		if (doc_view->vs.ecmascript_fragile)
			ecmascript_reset_state(&doc_view->vs);
		assert(doc_view->ecmascript);
		object_to_jsval(ctx, vp, JS_GetGlobalObject(doc_view->ecmascript->backend_data));
		break;
	}
#endif
	case JSP_WIN_TOP:
	{
		struct document_view *doc_view = vs->doc_view;
		struct document_view *top_view = doc_view->session->doc_view;
		JSObject *newjsframe;

		assert(top_view && top_view->vs);
		if (top_view->vs->ecmascript_fragile)
			ecmascript_reset_state(top_view->vs);
		if (!top_view->vs->ecmascript)
			break;
		newjsframe = JS_GetGlobalObject(top_view->vs->ecmascript->backend_data);

		/* Keep this unrolled this way. Will have to check document.domain
		 * JS property. */
		/* Note that this check is perhaps overparanoid. If top windows
		 * is alien but some other child window is not, we should still
		 * let the script walk thru. That'd mean moving the check to
		 * other individual properties in this switch. */
		if (compare_uri(vs->uri, top_view->vs->uri, URI_HOST))
			object_to_jsval(ctx, vp, newjsframe);
		/* else */
			/****X*X*X*** SECURITY VIOLATION! RED ALERT, SHIELDS UP! ***X*X*X****\
			|* (Pasky was apparently looking at the Links2 JS code   .  ___ ^.^ *|
			\* for too long.)                                        `.(,_,)\o/ */
		break;
	}
	default:
		INTERNAL("Invalid ID %d in window_get_property().", JSVAL_TO_INT(id));
		break;
	}

	return JS_TRUE;
}

static void location_goto(struct document_view *doc_view, unsigned char *url);

static JSBool
window_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct view_state *vs = JS_GetPrivate(ctx, obj);

	if (JSVAL_IS_STRING(id)) {
		if (!strcmp(jsval_to_string(ctx, &id), "location")) {
			struct document_view *doc_view = vs->doc_view;

			location_goto(doc_view, jsval_to_string(ctx, vp));
			/* Do NOT touch our .location property, evil
			 * SpiderMonkey!! */
			return JS_FALSE;
		}
		return JS_TRUE;
	}

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	default:
		INTERNAL("Invalid ID %d in window_set_property().", JSVAL_TO_INT(id));
		return JS_TRUE;
	}

	return JS_TRUE;
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
	unsigned char *string;

	if (argc != 1)
		return JS_TRUE;

	string = jsval_to_string(ctx, &argv[0]);
	if (!*string)
		return JS_TRUE;

	msg_box(vs->doc_view->session->tab->term, NULL, MSGBOX_FREE_TEXT,
		N_("JavaScript Alert"), ALIGN_CENTER,
		stracpy(string),
		NULL, 1,
		N_("OK"), NULL, B_ENTER | B_ESC);

	undef_to_jsval(ctx, rval);
	return JS_TRUE;
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
window_open(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct view_state *vs = JS_GetPrivate(ctx, obj);
	struct document_view *doc_view = vs->doc_view;
	struct session *ses = doc_view->session;
	unsigned char *url;
	struct uri *uri;
	static time_t ratelimit_start;
	static int ratelimit_count;

	if (get_opt_bool("ecmascript.block_window_opening")) {
#ifdef CONFIG_LEDS
		ses->status.popup_led->value = 'P';
#endif
		return JS_TRUE;
	}

	if (argc < 1) return JS_TRUE;

	/* Ratelimit window opening. Recursive window.open() is very nice.
	 * We permit at most 20 tabs in 2 seconds. The ratelimiter is very
	 * rough but shall suffice against the usual cases. */

	if (!ratelimit_start || time(NULL) - ratelimit_start > 2) {
		ratelimit_start = time(NULL);
		ratelimit_count = 0;
	} else {
		ratelimit_count++;
		if (ratelimit_count > 20)
			return JS_TRUE;
	}

	url = jsval_to_string(ctx, &argv[0]);

	/* TODO: Support for window naming and perhaps some window features? */

	url = join_urls(doc_view->document->uri,
	                trim_chars(url, ' ', 0));
	if (!url) return JS_TRUE;
	uri = get_uri(url, 0);
	mem_free(url);
	if (!uri) return JS_TRUE;

	if (!get_cmd_opt_bool("no-connect")
	    && !get_cmd_opt_bool("no-home")
	    && !get_cmd_opt_bool("anonymous")
	    && can_open_in_new(ses->tab->term)) {
		open_uri_in_new_window(ses, uri, ENV_ANY);
		boolean_to_jsval(ctx, rval, 1);
	} else {
		/* When opening a new tab, we might get rerendered, losing our
		 * context and triggerring a disaster, so postpone that. */
		struct delayed_open *deo = mem_calloc(1, sizeof(*deo));

		if (deo) {
			deo->ses = ses;
			deo->uri = get_uri_reference(uri);
			register_bottom_half((void (*)(void *)) delayed_open,
			                     deo);
			boolean_to_jsval(ctx, rval, 1);
		} else {
			undef_to_jsval(ctx, rval);
		}
	}

	done_uri(uri);

	return JS_TRUE;
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

	assert(fc);
	assert(fc->form && fs);

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum >= 0) link = &document->links[linknum];

	undef_to_jsval(ctx, vp);

	switch (JSVAL_TO_INT(id)) {
	case JSP_INPUT_ACCESSKEY:
	{
		struct string keystr;

		if (!link) break;

		init_string(&keystr);
		make_keystroke(&keystr, link->accesskey, 0, 0);
		string_to_jsval(ctx, vp, keystr.source);
		done_string(&keystr);
		break;
	}
	case JSP_INPUT_ALT:
		string_to_jsval(ctx, vp, fc->alt);
		break;
	case JSP_INPUT_CHECKED:
		boolean_to_jsval(ctx, vp, fs->state);
		break;
	case JSP_INPUT_DEFAULT_CHECKED:
		boolean_to_jsval(ctx, vp, fc->default_state);
		break;
	case JSP_INPUT_DEFAULT_VALUE:
		string_to_jsval(ctx, vp, fc->default_value);
		break;
	case JSP_INPUT_DISABLED:
		/* FIXME: <input readonly disabled> --pasky */
		boolean_to_jsval(ctx, vp, fc->mode == FORM_MODE_DISABLED);
		break;
	case JSP_INPUT_FORM:
		object_to_jsval(ctx, vp, parent_form);
		break;
	case JSP_INPUT_MAX_LENGTH:
		int_to_jsval(ctx, vp, fc->maxlength);
		break;
	case JSP_INPUT_NAME:
		string_to_jsval(ctx, vp, fc->name);
		break;
	case JSP_INPUT_READONLY:
		/* FIXME: <input readonly disabled> --pasky */
		boolean_to_jsval(ctx, vp, fc->mode == FORM_MODE_READONLY);
		break;
	case JSP_INPUT_SIZE:
		int_to_jsval(ctx, vp, fc->size);
		break;
	case JSP_INPUT_SRC:
		if (link && link->where_img)
			string_to_jsval(ctx, vp, link->where_img);
		break;
	case JSP_INPUT_TABINDEX:
		if (link)
			/* FIXME: This is WRONG. --pasky */
			int_to_jsval(ctx, vp, link->number);
		break;
	case JSP_INPUT_TYPE:
	{
		unsigned char *s = NULL;

		switch (fc->type) {
		case FC_TEXT: s = "text"; break;
		case FC_PASSWORD: s = "password"; break;
		case FC_FILE: s = "file"; break;
		case FC_CHECKBOX: s = "checkbox"; break;
		case FC_RADIO: s = "radio"; break;
		case FC_SUBMIT: s = "submit"; break;
		case FC_IMAGE: s = "image"; break;
		case FC_RESET: s = "reset"; break;
		case FC_BUTTON: s = "button"; break;
		case FC_HIDDEN: s = "hidden"; break;
		default: INTERNAL("input_get_property() upon a non-input item."); break;
		}
		string_to_jsval(ctx, vp, s);
		break;
	}
	case JSP_INPUT_VALUE:
		string_to_jsval(ctx, vp, fs->value);
		break;

	default:
		INTERNAL("Invalid ID %d in input_get_property().", JSVAL_TO_INT(id));
		break;
	}

	return JS_TRUE;
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

	assert(fc);
	assert(fc->form && fs);

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum >= 0) link = &document->links[linknum];

	switch (JSVAL_TO_INT(id)) {
	case JSP_INPUT_ACCESSKEY:
		if (link)
			link->accesskey = read_key(jsval_to_string(ctx, vp));
		break;
	case JSP_INPUT_ALT:
		mem_free_set(&fc->alt, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_INPUT_CHECKED:
		if (fc->type != FC_CHECKBOX && fc->type != FC_RADIO)
			break;
		fs->state = jsval_to_boolean(ctx, vp);
		break;
	case JSP_INPUT_DISABLED:
		/* FIXME: <input readonly disabled> --pasky */
		fc->mode = (jsval_to_boolean(ctx, vp) ? FORM_MODE_DISABLED
		                      : fc->mode == FORM_MODE_READONLY ? FORM_MODE_READONLY
		                                                       : FORM_MODE_NORMAL);
		break;
	case JSP_INPUT_MAX_LENGTH:
		fc->maxlength = atol(jsval_to_string(ctx, vp));
		break;
	case JSP_INPUT_NAME:
		mem_free_set(&fc->name, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_INPUT_READONLY:
		/* FIXME: <input readonly disabled> --pasky */
		fc->mode = (jsval_to_boolean(ctx, vp) ? FORM_MODE_READONLY
		                      : fc->mode == FORM_MODE_DISABLED ? FORM_MODE_DISABLED
		                                                       : FORM_MODE_NORMAL);
		break;
	case JSP_INPUT_SRC:
		if (link) {
			mem_free_set(&link->where_img, stracpy(jsval_to_string(ctx, vp)));
		}
		break;
	case JSP_INPUT_VALUE:
		if (fc->type == FC_FILE)
			break; /* A huge security risk otherwise. */
		mem_free_set(&fs->value, stracpy(jsval_to_string(ctx, vp)));
		if (fc->type == FC_TEXT || fc->type == FC_PASSWORD)
			fs->state = strlen(fs->value);
		break;

	default:
		INTERNAL("Invalid ID %d in input_set_property().", JSVAL_TO_INT(id));
		return JS_TRUE;
	}

	return JS_TRUE;
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

	assert(fs);
	fc = find_form_control(document, fs);
	assert(fc);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum < 0)
		return JS_TRUE;

	/* Restore old current_link afterwards? */
	jump_to_link_number(ses, doc_view, linknum);
	if (enter(ses, doc_view, 0) == FRAME_EVENT_REFRESH)
		refresh_view(ses, doc_view, 0);
	else
		print_screen_status(ses);

	boolean_to_jsval(ctx, rval, 0);
	return JS_TRUE;
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

	assert(fs);
	fc = find_form_control(document, fs);
	assert(fc);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum < 0)
		return JS_TRUE;

	jump_to_link_number(ses, doc_view, linknum);

	boolean_to_jsval(ctx, rval, 0);
	return JS_TRUE;
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

	if (JSVAL_IS_STRING(id)) {
		form_elements_namedItem(ctx, obj, 1, &id, vp);
		return JS_TRUE;
	}

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	undef_to_jsval(ctx, vp);

	switch (JSVAL_TO_INT(id)) {
	case JSP_FORM_ELEMENTS_LENGTH:
	{
		struct form_control *fc;
		int counter = 0;

		foreach (fc, form->items)
			counter++;

		int_to_jsval(ctx, vp, counter);
		break;
	}
	default:
		/* Array index. */
		form_elements_item(ctx, obj, 1, &id, vp);
		break;
	}

	return JS_TRUE;
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
	int counter = -1;
	int index;

	if (argc != 1)
		return JS_TRUE;

	index = atol(jsval_to_string(ctx, &argv[0]));

	undef_to_jsval(ctx, rval);

	foreach (fc, form->items) {
		counter++;
		if (counter == index) {
			JSObject *fcobj = get_form_control_object(ctx, parent_form, fc->type, find_form_state(doc_view, fc));

			if (fcobj) {
				object_to_jsval(ctx, rval, fcobj);
			}
			break;
		}
	}

	return JS_TRUE;
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
	unsigned char *string;

	if (argc != 1)
		return JS_TRUE;

	string = jsval_to_string(ctx, &argv[0]);
	if (!*string)
		return JS_TRUE;

	undef_to_jsval(ctx, rval);

	foreach (fc, form->items) {
		if (fc->name && !strcasecmp(string, fc->name)) {
			JSObject *fcobj = get_form_control_object(ctx, parent_form, fc->type, find_form_state(doc_view, fc));

			if (fcobj) {
				object_to_jsval(ctx, rval, fcobj);
			}
			break;
		}
	}

	return JS_TRUE;
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

	assert(form);

	if (JSVAL_IS_STRING(id)) {
		struct form_control *fc;
		unsigned char *string;

		string = jsval_to_string(ctx, &id);
		foreach (fc, form->items) {
			JSObject *fcobj = NULL;

			if (!fc->name || strcasecmp(string, fc->name))
				continue;

			fcobj = get_form_control_object(ctx, obj, fc->type, find_form_state(doc_view, fc));
			if (fcobj) {
				object_to_jsval(ctx, vp, fcobj);
			} else {
				undef_to_jsval(ctx, vp);
			}
			break;
		}
		return JS_TRUE;
	}

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	undef_to_jsval(ctx, vp);

	switch (JSVAL_TO_INT(id)) {
	case JSP_FORM_ACTION:
		string_to_jsval(ctx, vp, form->action);
		break;

	case JSP_FORM_ELEMENTS:
	{
		/* jsform ('form') is form_elements' parent; who knows is that's correct */
		JSObject *jsform_elems = JS_NewObject(ctx, (JSClass *) &form_elements_class, NULL, obj);

		JS_DefineProperties(ctx, jsform_elems, (JSPropertySpec *) form_elements_props);
		JS_DefineFunctions(ctx, jsform_elems, (JSFunctionSpec *) form_elements_funcs);
		object_to_jsval(ctx, vp, jsform_elems);
		/* SM will cache this property value for us so we create this
		 * just once per form. */
	}
		break;

	case JSP_FORM_ENCODING:
		switch (form->method) {
		case FORM_METHOD_GET:
		case FORM_METHOD_POST:
			string_to_jsval(ctx, vp, "application/x-www-form-urlencoded");
			break;
		case FORM_METHOD_POST_MP:
			string_to_jsval(ctx, vp, "multipart/form-data");
			break;
		case FORM_METHOD_POST_TEXT_PLAIN:
			string_to_jsval(ctx, vp, "text/plain");
			break;
		}
		break;

	case JSP_FORM_LENGTH:
	{
		struct form_control *fc;
		int counter = 0;

		foreach (fc, form->items)
			counter++;
		int_to_jsval(ctx, vp, counter);
		break;
	}

	case JSP_FORM_METHOD:
		switch (form->method) {
		case FORM_METHOD_GET:
			string_to_jsval(ctx, vp, "GET");
			break;

		case FORM_METHOD_POST:
		case FORM_METHOD_POST_MP:
		case FORM_METHOD_POST_TEXT_PLAIN:
			string_to_jsval(ctx, vp, "POST");
			break;
		}
		break;

	case JSP_FORM_NAME:
		string_to_jsval(ctx, vp, form->name);
		break;

	case JSP_FORM_TARGET:
		string_to_jsval(ctx, vp, form->target);
		break;

	default:
		INTERNAL("Invalid ID %d in form_get_property().", JSVAL_TO_INT(id));
		break;
	}

	return JS_TRUE;
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
	unsigned char *string;

	assert(form);

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_FORM_ACTION:
		mem_free_set(&form->action, stracpy(jsval_to_string(ctx, vp)));
		break;

	case JSP_FORM_ENCODING:
		string = jsval_to_string(ctx, vp);
		if (!strcasecmp(string, "application/x-www-form-urlencoded")) {
			form->method = form->method == FORM_METHOD_GET ? FORM_METHOD_GET
			                                               : FORM_METHOD_POST;
		} else if (!strcasecmp(string, "multipart/form-data")) {
			form->method = FORM_METHOD_POST_MP;
		} else if (!strcasecmp(string, "text/plain")) {
			form->method = FORM_METHOD_POST_TEXT_PLAIN;
		}
		break;

	case JSP_FORM_METHOD:
		string = jsval_to_string(ctx, vp);
		if (!strcasecmp(string, "GET")) {
			form->method = FORM_METHOD_GET;
		} else if (!strcasecmp(string, "POST")) {
			form->method = FORM_METHOD_POST;
		}
		break;

	case JSP_FORM_NAME:
		mem_free_set(&form->name, stracpy(jsval_to_string(ctx, vp)));
		break;

	case JSP_FORM_TARGET:
		mem_free_set(&form->target, stracpy(jsval_to_string(ctx, vp)));
		break;

	default:
		INTERNAL("Invalid ID %d in form_set_property().", JSVAL_TO_INT(id));
		break;
	}

	return JS_TRUE;
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

	assert(form);

	do_reset_form(doc_view, form);
	draw_forms(doc_view->session->tab->term, doc_view);

	boolean_to_jsval(ctx, rval, 0);

	return JS_TRUE;
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

	assert(form);
	submit_given_form(ses, doc_view, form);

	boolean_to_jsval(ctx, rval, 0);

	return JS_TRUE;
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

	if (JSVAL_IS_STRING(id)) {
		forms_namedItem(ctx, obj, 1, &id, vp);
		return JS_TRUE;
	}

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_FORMS_LENGTH:
	{
		struct form *form;
		int counter = 0;

		foreach (form, document->forms)
			counter++;

		int_to_jsval(ctx, vp, counter);
		break;
	}
	default:
		/* Array index. */
		forms_item(ctx, obj, 1, &id, vp);
		break;
	}

	return JS_TRUE;
}

static JSBool
forms_item(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject *parent_doc = JS_GetParent(ctx, obj);
	JSObject *parent_win = JS_GetParent(ctx, parent_doc);
	struct view_state *vs = JS_GetPrivate(ctx, parent_win);
	struct form_view *fv;
	int counter = -1;
	int index;

	if (argc != 1)
		return JS_TRUE;

	index = atol(jsval_to_string(ctx, &argv[0]));

	undef_to_jsval(ctx, rval);

	foreach (fv, vs->forms) {
		counter++;
		if (counter == index) {
			object_to_jsval(ctx, rval, get_form_object(ctx, parent_doc, fv));
			break;
		}
	}

	return JS_TRUE;
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
	unsigned char *string;

	if (argc != 1)
		return JS_TRUE;

	undef_to_jsval(ctx, rval);

	string = jsval_to_string(ctx, &argv[0]);
	if (!*string)
		return JS_TRUE;

	foreach (form, document->forms) {
		if (form->name && !strcasecmp(string, form->name)) {
			object_to_jsval(ctx, rval, get_form_object(ctx, parent_doc,
					find_form_view(doc_view, form)));
			break;
		}
	}

	return JS_TRUE;
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

	if (JSVAL_IS_STRING(id)) {
		struct form *form;
		unsigned char *string = jsval_to_string(ctx, &id);

#ifdef CONFIG_COOKIES
		if (!strcmp(string, "cookie")) {
			struct string *cookies = send_cookies(vs->uri);

			if (cookies) {
				static unsigned char cookiestr[1024];

				strncpy(cookiestr, cookies->source, 1024);
				done_string(cookies);

				string_to_jsval(ctx, vp, cookiestr);
				return JS_TRUE;
			}
		}
#endif
		foreach (form, document->forms) {
			if (!form->name || strcasecmp(string, form->name))
				continue;

			object_to_jsval(ctx, vp, get_form_object(ctx, obj, find_form_view(doc_view, form)));
			break;
		}
		return JS_TRUE;
	}

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	undef_to_jsval(ctx, vp);

	switch (JSVAL_TO_INT(id)) {
	case JSP_DOC_REF:
		switch (get_opt_int("protocol.http.referer.policy")) {
		case REFERER_NONE:
			/* oh well */
			undef_to_jsval(ctx, vp);
			break;

		case REFERER_FAKE:
			string_to_jsval(ctx, vp, get_opt_str("protocol.http.referer.fake"));
			break;

		case REFERER_TRUE:
			/* XXX: Encode as in add_url_to_httset_prop_string(&prop, ) ? --pasky */
			if (ses->referrer) {
				astring_to_jsval(ctx, vp, get_uri_string(ses->referrer, URI_HTTP_REFERRER));
			}
			break;

		case REFERER_SAME_URL:
			astring_to_jsval(ctx, vp, get_uri_string(document->uri, URI_HTTP_REFERRER));
			break;
		}
		break;
	case JSP_DOC_TITLE:
		string_to_jsval(ctx, vp, document->title);
		break;
	case JSP_DOC_URL:
		astring_to_jsval(ctx, vp, get_uri_string(document->uri, URI_ORIGINAL));
		break;
	default:
		INTERNAL("Invalid ID %d in document_get_property().", JSVAL_TO_INT(id));
		break;
	}

	return JS_TRUE;
}

static JSBool
document_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *parent = JS_GetParent(ctx, obj);
	struct view_state *vs = JS_GetPrivate(ctx, parent);
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;

	if (JSVAL_IS_STRING(id)) {
#ifdef CONFIG_COOKIES
		if (!strcmp(jsval_to_string(ctx, &id), "cookie")) {
			set_cookie(vs->uri, jsval_to_string(ctx, vp));
			/* Do NOT touch our .cookie property, evil
			 * SpiderMonkey!! */
			return JS_FALSE;
		}
#endif
		return JS_TRUE;
	}

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_DOC_TITLE:
		mem_free_set(&document->title, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_DOC_URL:
		/* According to the specs this should be readonly but some
		 * broken sites still assign to it (i.e.
		 * http://www.e-handelsfonden.dk/validering.asp?URL=www.polyteknisk.dk).
		 * So emulate window.location. */
		location_goto(doc_view, jsval_to_string(ctx, vp));
		break;
	}

	return JS_TRUE;
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

	/* XXX: I don't know about you, but I have *ENOUGH* of those 'Undefined
	 * function' errors, I want to see just the useful ones. So just
	 * lighting a led and going away, no muss, no fuss. --pasky */
	/* TODO: Perhaps we can introduce ecmascript.error_report_unsupported
	 * -> "Show information about the document using some valid,
	 *  nevertheless unsupported methods/properties." --pasky too */

#ifdef CONFIG_LEDS
	interpreter->vs->doc_view->session->status.ecmascript_led->value = 'J';
#endif

	boolean_to_jsval(ctx, rval, 0);

	return JS_TRUE;
}


static JSBool history_back(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool history_forward(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

static const JSClass history_class = {
	"history",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

static const JSFunctionSpec history_funcs[] = {
	{ "back",		history_back,		0 },
	{ "forward",		history_forward,	0 },
	{ NULL }
};

static JSBool
history_back(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct ecmascript_interpreter *interpreter = JS_GetContextPrivate(ctx);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct session *ses = doc_view->session;

	go_back(ses);

	return 2;
}

static JSBool
history_forward(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct ecmascript_interpreter *interpreter = JS_GetContextPrivate(ctx);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct session *ses = doc_view->session;

	go_unback(ses);

	return 2;
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

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	undef_to_jsval(ctx, vp);

	switch (JSVAL_TO_INT(id)) {
	case JSP_LOC_HREF:
		astring_to_jsval(ctx, vp, get_uri_string(vs->uri, URI_ORIGINAL));
		break;
	default:
		INTERNAL("Invalid ID %d in location_get_property().", JSVAL_TO_INT(id));
		break;
	}

	return JS_TRUE;
}

static JSBool
location_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *parent = JS_GetParent(ctx, obj);
	struct view_state *vs = JS_GetPrivate(ctx, parent);
	struct document_view *doc_view = vs->doc_view;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_LOC_HREF:
		location_goto(doc_view, jsval_to_string(ctx, vp));
		break;
	}

	return JS_TRUE;
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

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_UNIBAR_VISIBLE:
#define unibar_fetch(bar) \
	boolean_to_jsval(ctx, vp, status->force_show_##bar##_bar >= 0 \
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
			boolean_to_jsval(ctx, vp, 0);
			break;
		}
#undef unibar_fetch
		break;
	default:
		INTERNAL("Invalid ID %d in unibar_get_property().", JSVAL_TO_INT(id));
		break;
	}

	return JS_TRUE;
}

static JSBool
unibar_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *parent = JS_GetParent(ctx, obj);
	struct view_state *vs = JS_GetPrivate(ctx, parent);
	struct document_view *doc_view = vs->doc_view;
	struct session_status *status = &doc_view->session->status;
	unsigned char *bar = JS_GetPrivate(ctx, obj);

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_UNIBAR_VISIBLE:
		switch (*bar) {
		case 's':
			status->force_show_status_bar = jsval_to_boolean(ctx, vp);
			break;
		case 't':
			status->force_show_title_bar = jsval_to_boolean(ctx, vp);
			break;
		default:
			break;
		}
		register_bottom_half((void (*)(void*)) update_status, NULL);
		break;
	default:
		INTERNAL("Invalid ID %d in unibar_set_property().", JSVAL_TO_INT(id));
		return JS_TRUE;
	}

	return JS_TRUE;
}


static JSBool navigator_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);

static const JSClass navigator_class = {
	"navigator",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	navigator_get_property, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

enum navigator_prop {
	JSP_NAVIGATOR_APP_CODENAME,
	JSP_NAVIGATOR_APP_NAME,
	JSP_NAVIGATOR_APP_VERSION,
	JSP_NAVIGATOR_LANGUAGE,
	/* JSP_NAVIGATOR_MIME_TYPES, */
	JSP_NAVIGATOR_PLATFORM,
	/* JSP_NAVIGATOR_PLUGINS, */
	JSP_NAVIGATOR_USER_AGENT,
};
static const JSPropertySpec navigator_props[] = {
	{ "appCodeName",	JSP_NAVIGATOR_APP_CODENAME,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "appName",		JSP_NAVIGATOR_APP_NAME,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "appVersion",		JSP_NAVIGATOR_APP_VERSION,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "language",		JSP_NAVIGATOR_LANGUAGE,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "platform",		JSP_NAVIGATOR_PLATFORM,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "userAgent",		JSP_NAVIGATOR_USER_AGENT,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};


static JSBool
navigator_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	undef_to_jsval(ctx, vp);

	switch (JSVAL_TO_INT(id)) {
	case JSP_NAVIGATOR_APP_CODENAME:
		string_to_jsval(ctx, vp, "Mozilla"); /* More like a constant nowadays. */
		break;
	case JSP_NAVIGATOR_APP_NAME:
		/* This evil hack makes the compatibility checking .indexOf()
		 * code find what it's looking for. */
		string_to_jsval(ctx, vp, "ELinks (roughly compatible with Netscape Navigator, Mozilla and Microsoft Internet Explorer)");
		break;
	case JSP_NAVIGATOR_APP_VERSION:
		string_to_jsval(ctx, vp, VERSION);
		break;
	case JSP_NAVIGATOR_LANGUAGE:
#ifdef CONFIG_NLS
		if (get_opt_bool("protocol.http.accept_ui_language"))
			string_to_jsval(ctx, vp, language_to_iso639(current_language));

#endif
		break;
	case JSP_NAVIGATOR_PLATFORM:
		string_to_jsval(ctx, vp, system_name);
		break;
	case JSP_NAVIGATOR_USER_AGENT:
	{
		/* FIXME: Code duplication. */
		unsigned char *optstr = get_opt_str("protocol.http.user_agent");

		if (*optstr && strcmp(optstr, " ")) {
			unsigned char *ustr, ts[64] = "";
			static unsigned char custr[256];

			if (!list_empty(terminals)) {
				unsigned int tslen = 0;
				struct terminal *term = terminals.prev;

				ulongcat(ts, &tslen, term->width, 3, 0);
				ts[tslen++] = 'x';
				ulongcat(ts, &tslen, term->height, 3, 0);
			}
			ustr = subst_user_agent(optstr, VERSION_STRING, system_name, ts);

			if (ustr) {
				safe_strncpy(custr, ustr, 256);
				mem_free(ustr);
				string_to_jsval(ctx, vp, custr);
			}
		}
	}
		break;
	default:
		INTERNAL("Invalid ID %d in navigator_get_property().", JSVAL_TO_INT(id));
		break;
	}

	return JS_TRUE;
}



/*** The ELinks interface */

static JSRuntime *jsrt;

static void
error_reporter(JSContext *ctx, const char *message, JSErrorReport *report)
{
	struct ecmascript_interpreter *interpreter = JS_GetContextPrivate(ctx);
	struct terminal *term;
	unsigned char *strict, *exception, *warning, *error;
	struct string msg;

	assert(interpreter && interpreter->vs && interpreter->vs->doc_view
	       && interpreter->vs->doc_view->session
	       && interpreter->vs->doc_view->session->tab);
	if_assert_failed goto reported;

	term = interpreter->vs->doc_view->session->tab->term;

#ifdef CONFIG_LEDS
	interpreter->vs->doc_view->session->status.ecmascript_led->value = 'J';
#endif

	if (!get_opt_bool("ecmascript.error_reporting")
	    || !init_string(&msg))
		goto reported;

	strict	  = JSREPORT_IS_STRICT(report->flags) ? " strict" : "";
	exception = JSREPORT_IS_EXCEPTION(report->flags) ? " exception" : "";
	warning   = JSREPORT_IS_WARNING(report->flags) ? " warning" : "";
	error	  = !report->flags ? " error" : "";

	add_format_to_string(&msg, _("A script embedded in the current "
			"document raised the following%s%s%s%s", term),
			strict, exception, warning, error);

	add_to_string(&msg, ":\n\n");
	add_to_string(&msg, message);

	if (report->linebuf && report->tokenptr) {
		int pos = report->tokenptr - report->linebuf;

		add_format_to_string(&msg, "\n\n%s\n.%*s^%*s.",
			       report->linebuf,
			       pos - 2, " ",
			       strlen(report->linebuf) - pos - 1, " ");
	}

	msg_box(term, NULL, MSGBOX_FREE_TEXT,
		N_("JavaScript Error"), ALIGN_CENTER,
		msg.source, NULL, 1,
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
			N_("JavaScript Emergency"), ALIGN_LEFT,
			msg_text(term,
				 N_("A script embedded in the current document was running\n"
				 "for more than %d seconds. This probably means there is\n"
				 "a bug in the script and it could have halted the whole\n"
				 "ELinks, so the script execution was interrupted."),
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
	JSObject *window_obj, *document_obj, *forms_obj, *history_obj, *location_obj,
	         *statusbar_obj, *menubar_obj, *navigator_obj;

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

	history_obj = JS_InitClass(ctx, window_obj, NULL,
				    (JSClass *) &history_class, NULL, 0,
				    (JSPropertySpec *) NULL,
				    (JSFunctionSpec *) history_funcs,
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

	navigator_obj = JS_InitClass(ctx, window_obj, NULL,
				     (JSClass *) &navigator_class, NULL, 0,
				     (JSPropertySpec *) navigator_props, NULL,
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

	return stracpy(jsval_to_string(ctx, &rval));
}


int
spidermonkey_eval_boolback(struct ecmascript_interpreter *interpreter,
			   struct string *code)
{
	JSContext *ctx;
	jsval rval;
	int ret;

	assert(interpreter);
	ctx = interpreter->backend_data;
	setup_safeguard(interpreter, ctx);
	ret = JS_EvaluateScript(ctx, JS_GetGlobalObject(ctx),
			  code->source, code->length, "", 0, &rval);
	if (ret == 2) { /* onClick="history.back()" */
		return 0;
	};		
	if (ret == JS_FALSE) {
		return -1;
	}
	if (JSVAL_IS_VOID(rval)) {
		/* Undefined value. */
		return -1;
	}

	return jsval_to_boolean(ctx, &rval);
}
