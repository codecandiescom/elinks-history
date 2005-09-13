/* The SpiderMonkey ECMAScript backend. */
/* $Id: spidermonkey.c,v 1.223 2005/09/13 22:12:48 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/spidermonkey/util.h"

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "cookies/cookies.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "document/html/frames.h"
#include "document/document.h"
#include "document/forms.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/spidermonkey.h"
#include "ecmascript/spidermonkey/window.h"
#include "ecmascript/spidermonkey/form.h"
#include "intl/gettext/libintl.h"
#include "main/select.h"
#include "osdep/newwin.h"
#include "osdep/sysname.h"
#include "protocol/http/http.h"
#include "protocol/uri.h"
#include "session/history.h"
#include "session/location.h"
#include "session/session.h"
#include "session/task.h"
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

void location_goto(struct document_view *doc_view, unsigned char *url);


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
			} else {
				string_to_jsval(ctx, vp, "");
			}
			return JS_TRUE;
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
	set_led_value(interpreter->vs->doc_view->session->status.ecmascript_led, 'J');
#endif

	boolean_to_jsval(ctx, rval, 0);

	return JS_TRUE;
}


static JSBool history_back(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool history_forward(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool history_go(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

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
	{ "go",			history_go,		1 },
	{ NULL }
};

static JSBool
history_back(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct ecmascript_interpreter *interpreter = JS_GetContextPrivate(ctx);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct session *ses = doc_view->session;

	go_back(ses);

/* history_back() must return 0 for onClick to cause displaying previous page
 * and return non zero for <a href="javascript:history.back()"> to prevent
 * "calculating" new link. Returned value 2 is changed to 0 in function
 * spidermonkey_eval_boolback */
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

static JSBool
history_go(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct ecmascript_interpreter *interpreter = JS_GetContextPrivate(ctx);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct session *ses = doc_view->session;
	int index;
	struct location *loc;

	if (argc != 1)
		return JS_TRUE;

	index  = atol(jsval_to_string(ctx, &argv[0]));

	for (loc = cur_loc(ses);
	     loc != (struct location *) &ses->history.history;
	     loc = index > 0 ? loc->next : loc->prev) {
		if (!index) {
			go_history(ses, loc);
			break;
		}

		index += index > 0 ? -1 : 1;
	}

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

void
location_goto(struct document_view *doc_view, unsigned char *url)
{
	unsigned char *new_abs_url;
	struct uri *new_uri;
	struct delayed_goto *deg;

	/* Workaround for bug 611. Does not crash, but may lead to infinite loop.*/
	if (!doc_view) return;
	new_abs_url = join_urls(doc_view->document->uri,
	                        trim_chars(url, ' ', 0));
	if (!new_abs_url)
		return;
	new_uri = get_uri(new_abs_url, 0);
	mem_free(new_abs_url);
	if (!new_uri)
		return;
	deg = mem_calloc(1, sizeof(*deg));
	if (!deg) {
		done_uri(new_uri);
		return;
	}
	assert(doc_view->vs);
	deg->vs = doc_view->vs;
	deg->uri = new_uri;
	/* It does not seem to be very safe inside of frames to
	 * call goto_uri() right away. */
	register_bottom_half(delayed_goto, deg);
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
		register_bottom_half(update_status, NULL);
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
	set_led_value(interpreter->vs->doc_view->session->status.ecmascript_led, 'J');
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

	info_box(term, MSGBOX_FREE_TEXT, N_("JavaScript Error"), ALIGN_CENTER,
		 msg.source);

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
		info_box(term, MSGBOX_FREE_TEXT,
			 N_("JavaScript Emergency"), ALIGN_LEFT,
			 msg_text(term,
				  N_("A script embedded in the current document was running\n"
				  "for more than %d seconds. This probably means there is\n"
				  "a bug in the script and it could have halted the whole\n"
				  "ELinks, so the script execution was interrupted."),
				  max_exec_time));
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
	}
	if (ret == JS_FALSE) {
		return -1;
	}
	if (JSVAL_IS_VOID(rval)) {
		/* Undefined value. */
		return -1;
	}

	return jsval_to_boolean(ctx, &rval);
}
