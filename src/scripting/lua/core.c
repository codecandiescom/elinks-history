/* Lua interface (scripting engine) */
/* $Id: core.c,v 1.85 2003/10/24 23:19:44 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_LUA

#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <lua.h>
#include <lualib.h>

#include "elinks.h"

#include "bfu/button.h"
#include "bfu/dialog.h"
#include "bfu/inpfield.h"
#include "bfu/inphist.h"
#include "bfu/msgbox.h"
#include "bfu/style.h"
#include "bfu/text.h"
#include "config/kbdbind.h"
#include "document/cache.h"
#include "document/html/renderer.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/home.h"
#include "lowlevel/signals.h"
#include "protocol/uri.h"
#include "sched/event.h"
#include "sched/session.h"
#include "scripting/lua/core.h"
#include "scripting/lua/hooks.h"
#include "scripting/scripting.h"
#include "terminal/terminal.h"
#include "util/color.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/dump/dump.h"
#include "viewer/text/view.h"
#include "viewer/text/vs.h"


lua_State *lua_state;

static struct session *lua_ses;
static struct terminal *errterm;
static sigjmp_buf errjmp;

#define L	lua_state
#define LS	lua_State *S

typedef	unsigned char uchar;

static void handle_standard_lua_returns(unsigned char *from);


/*
 * Functions exported to the lua_State.
 */

static int
l_alert(LS)
{
	alert_lua_error((uchar *) lua_tostring(S, 1));
	return 0;
}

static int
l_current_url(LS)
{
	if (lua_ses && have_location(lua_ses)) {
		struct view_state *vs = &cur_loc(lua_ses)->vs;
		unsigned char *postchar = strchr(vs->url, POST_CHAR);
		unsigned char *url;

		url = memacpy(vs->url, postchar ? postchar - vs->url
						: vs->url_len);
		if (url) {
			lua_pushstring(S, url);
			mem_free(url);
			return 1;
		}
	}

	lua_pushnil(S);
	return 1;
}

static int
l_current_link(LS)
{
	struct link *link = get_current_link(lua_ses);

	if (link) {
		lua_pushstring(S, link->where);
	} else {
		lua_pushnil(S);
	}

	return 1;
}

static int
l_current_title(LS)
{
	struct document_view *doc_view = current_frame(lua_ses);

	if (doc_view && doc_view->document->title) {
		lua_pushstring(S, doc_view->document->title);
		return 1;
	}

	lua_pushnil(S);
	return 1;
}

static int
l_current_document(LS)
{
	if (lua_ses) {
		unsigned char *url = cur_loc(lua_ses)->vs.url;
		struct cache_entry *ce;
		struct fragment *f;

		if (url && find_in_cache(url, &ce) && (f = ce->frag.next)) {
			lua_pushlstring(S, f->data, f->length);
			return 1;
		}
	}

	lua_pushnil(S);
	return 1;
}

/* XXX: This function is mostly copied from `dump_to_file'. */
static int
l_current_document_formatted(LS)
{
	struct document_view *doc_view;
	struct string buffer;
	int width, old_width = 0;

	if (lua_gettop(S) == 0) width = -1;
	else if (!lua_isnumber(S, 1)) goto lua_error;
	else if ((width = lua_tonumber(S, 1)) <= 0) goto lua_error;

	if (!lua_ses || !(doc_view = current_frame(lua_ses))) goto lua_error;
	if (width > 0) {
		old_width = lua_ses->tab->term->x, lua_ses->tab->term->x = width;
		html_interpret(lua_ses);
	}

	if (init_string(&buffer)) {
		add_document_to_string(&buffer, doc_view->document);
		lua_pushlstring(S, buffer.source, buffer.length);
		done_string(&buffer);
	}

	if (width > 0) {
		lua_ses->tab->term->x = old_width;
		html_interpret(lua_ses);
	}
	return 1;

lua_error:
	lua_pushnil(S);
	return 1;
}

static int
l_pipe_read(LS)
{
	FILE *fp;
	unsigned char *s = NULL;
	int len = 0;

	if (!lua_isstring(S, 1)) goto lua_error;

	fp = popen(lua_tostring(S, 1), "r");
	if (!fp) goto lua_error;

	while (!feof(fp)) {
		unsigned char buf[1024];
		int l = fread(buf, 1, sizeof buf, fp);

		if (l > 0) {
			s = mem_realloc(s, len + l);
			if (!s) goto lua_error;
			memcpy(s + len, buf, l);
			len += l;

		} else if (l < 0) {
			goto lua_error;
		}
	}
	pclose(fp);

	lua_pushlstring(S, s, len);
	if (s) mem_free(s);
	return 1;

lua_error:
	if (s) mem_free(s);
	lua_pushnil(S);
	return 1;
}

static int
l_execute(LS)
{
	if (lua_isstring(S, 1)) {
		exec_on_terminal(lua_ses->tab->term, (uchar *)lua_tostring(S, 1), "", 0);
		lua_pushnumber(S, 0);
		return 1;
	}

	lua_pushnil(L);
	return 1;
}

static int
l_tmpname(LS)
{
	char *fn = tempnam(NULL, "links");

	if (fn) {
		lua_pushstring(S, fn);
		free(fn);
		return 1;
	}

	alert_lua_error("Error generating temporary file name");
	lua_pushnil(S);
	return 1;
}

static int
l_enable_systems_functions(LS)
{
	lua_iolibopen(S);
	lua_register(S, "pipe_read", l_pipe_read);
	lua_register(S, "execute", l_execute);
	lua_register(S, "tmpname", l_tmpname);

	return 0;
}

/*
 * Helper to run Lua functions bound to keystrokes.
 */

static enum evhook_status
run_lua_func(va_list ap, void *data)
{
	struct session *ses = va_arg(ap, struct session *);
	int func_ref = (int)data;
	int err;

	if (func_ref == LUA_NOREF) {
		alert_lua_error("key bound to nothing (internal error)");
		return EHS_NEXT;
	}

	lua_getref(L, func_ref);
	if (prepare_lua(ses)) return EHS_NEXT;
	err = lua_call(L, 0, 2);
	finish_lua();
	if (!err) handle_standard_lua_returns("keyboard function");

	return EHS_NEXT;
}

static int
l_bind_key(LS)
{
	int ref;
	unsigned char *err;
	struct string event_name = NULL_STRING;
	int event_id;

	if (!lua_isstring(S, 1) || !lua_isstring(S, 2)
	    || !lua_isfunction(S, 3)) {
		alert_lua_error("bad arguments to bind_key");
		goto lua_error;
	}

	if (!init_string(&event_name)) goto lua_error;

	lua_pushvalue(S, 3);
	ref = lua_ref(S, 1);
	add_format_to_string(&event_name, "lua-run-func %i", ref);
	event_id = register_event(event_name.source);
	event_id = register_event_hook(event_id, run_lua_func, 0, (void *)ref);
	done_string(&event_name);
	if (event_id == EVENT_NONE) goto lua_error;

	err = bind_scripting_func((uchar *)lua_tostring(S, 1),
			    (uchar *)lua_tostring(S, 2), event_id);
	if (err) {
		lua_unref(S, ref);
		alert_lua_error2("error in bind_key: ", err);
		goto lua_error;
	}

	lua_pushnumber(S, 1);
	return 1;

lua_error:
	lua_pushnil(S);
	return 1;
}


/* Begin very hackish bit for bookmark editing dialog.  */
/* XXX: Add history and generalise.  */

static unsigned char *dlg_msg[] = {
	N_("Name"),
	N_("Name"),
	N_("URL"),
	""
};

struct lua_dlg_data {
	lua_State *state;
	unsigned char cat[MAX_STR_LEN];
	unsigned char name[MAX_STR_LEN];
	unsigned char url[MAX_STR_LEN];
	int func_ref;
};

static void
dialog_run_lua(struct lua_dlg_data *data)
{
	lua_State *s = data->state;
	int err;

	lua_getref(s, data->func_ref);
	lua_pushstring(s, data->cat);
	lua_pushstring(s, data->name);
	lua_pushstring(s, data->url);
	if (prepare_lua(lua_ses)) return;
	err = lua_call(s, 3, 2);
	finish_lua();
	lua_unref(s, data->func_ref);
	handle_standard_lua_returns("post dialog function");
}

static void
dialog_fn(struct dialog_data *dlg_data)
{
	struct terminal *term = dlg_data->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = -1;
	struct color_pair *dialog_text_color = get_bfu_color(term, "dialog.text");

	text_width(term, dlg_msg[0], &min, &max);
	text_width(term, dlg_msg[1], &min, &max);
	text_width(term, dlg_msg[2], &min, &max);
	buttons_width(term, dlg_data->items + 3, 2, &min, &max);

	w = dlg_data->win->term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max) w = max;
	if (w < min) w = min;
	int_upper_bound(&w, dlg_data->win->term->x - 2 * DIALOG_LB);
	if (w < 1) w = 1;

	/*rw = 0;*/
	/*HACK*/ w = rw = 50;
	dlg_format_text(NULL, term, dlg_msg[0], 0, &y, w, &rw,
			dialog_text_color, AL_LEFT);
	y += 2;
	dlg_format_text(NULL, term, dlg_msg[1], 0, &y, w, &rw,
			dialog_text_color, AL_LEFT);
	y += 2;
	dlg_format_text(NULL, term, dlg_msg[2], 0, &y, w, &rw,
			dialog_text_color, AL_LEFT);
	y += 2;
	dlg_format_buttons(NULL, term, dlg_data->items + 3, 2, 0, &y, w, &rw,
			   AL_CENTER);
	w = rw;
	dlg_data->xw = w + 2 * DIALOG_LB;
	dlg_data->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg_data);
	draw_dlg(dlg_data);
	y = dlg_data->y + DIALOG_TB;
	dlg_format_text(term, term, dlg_msg[0], dlg_data->x + DIALOG_LB, &y, w, NULL,
			dialog_text_color, AL_LEFT);
	dlg_format_field(term, term, &dlg_data->items[0], dlg_data->x + DIALOG_LB, &y, w,
			 NULL, AL_LEFT);
	y++;
	dlg_format_text(term, term, dlg_msg[1], dlg_data->x + DIALOG_LB, &y, w, NULL,
			dialog_text_color, AL_LEFT);
	dlg_format_field(term, term, &dlg_data->items[1], dlg_data->x + DIALOG_LB, &y, w,
			 NULL, AL_LEFT);
	y++;
	dlg_format_text(term, term, dlg_msg[2], dlg_data->x + DIALOG_LB, &y, w, NULL,
			dialog_text_color, AL_LEFT);
	dlg_format_field(term, term, &dlg_data->items[2], dlg_data->x + DIALOG_LB, &y, w,
			 NULL, AL_LEFT);
	y++;
	dlg_format_buttons(term, term, &dlg_data->items[3], 2, dlg_data->x + DIALOG_LB,
			   &y, w, NULL, AL_CENTER);
}

static int
l_edit_bookmark_dialog(LS)
{
	struct dialog *dlg;
	struct lua_dlg_data *data;
	int n = 0;

	if (!lua_isstring(S, 1) || !lua_isstring(S, 2)
	    || !lua_isstring(S, 3) || !lua_isfunction(S, 4)) {
		lua_pushnil(S);
		return 1;
	}

#define L_EDIT_BMK_DLG_SIZE 5
	dlg = mem_calloc(1, sizeof(struct dialog)
			    + (L_EDIT_BMK_DLG_SIZE + 1) * sizeof(struct widget)
			    + sizeof *data);
	if (!dlg) return 0;

	data = (struct lua_dlg_data *)&dlg->items[L_EDIT_BMK_DLG_SIZE + 1];
	data->state = S;
	safe_strncpy(data->cat, (uchar *)lua_tostring(S, 1), MAX_STR_LEN-1);
	safe_strncpy(data->name, (uchar *)lua_tostring(S, 2), MAX_STR_LEN-1);
	safe_strncpy(data->url, (uchar *)lua_tostring(S, 3), MAX_STR_LEN-1);
	lua_pushvalue(S, 4);
	data->func_ref = lua_ref(S, 1);

	dlg->title = _("Edit bookmark", lua_ses->tab->term);
	dlg->fn = dialog_fn;
	dlg->refresh = (void (*)(void *))dialog_run_lua;
	dlg->refresh_data = data;

	set_dlg_field(dlg, n, 0, 0, NULL, MAX_STR_LEN, data->cat, NULL);
	set_dlg_field(dlg, n, 0, 0, NULL, MAX_STR_LEN, data->name, NULL);
	set_dlg_field(dlg, n, 0, 0, NULL, MAX_STR_LEN, data->url, NULL);

	set_dlg_button(dlg, n, B_ENTER, ok_dialog, _("OK", lua_ses->tab->term), NULL);
	set_dlg_button(dlg, n, B_ESC, cancel_dialog, _("Cancel", lua_ses->tab->term), NULL);

	assert(n == L_EDIT_BMK_DLG_SIZE);
	dlg->items[n].type = D_END;

	do_dialog(lua_ses->tab->term, dlg, getml(dlg, NULL));

	lua_pushnumber(S, 1);
	return 1;
}

/* End very hackish bit.  */


/* Begin hackish bit for half-generalised dialog.  */
/* XXX: Add history and custom labels.  */

#define XDIALOG_MAX_FIELDS	5

#if 0
static unsigned char *xdialog_msg[] = {
	N_("A field"),
	""
};
#endif

struct lua_xdialog_data {
	lua_State *state;
	int func_ref;
	int nfields;
	unsigned char fields[XDIALOG_MAX_FIELDS][MAX_STR_LEN];
};

static void
xdialog_run_lua(struct lua_xdialog_data *data)
{
	lua_State *s = data->state;
	int err;
	int i;

	lua_getref(s, data->func_ref);
	for (i = 0; i < data->nfields; i++) lua_pushstring(s, data->fields[i]);
	if (prepare_lua(lua_ses)) return;
	err = lua_call(s, data->nfields, 2);
	finish_lua();
	lua_unref(s, data->func_ref);
	handle_standard_lua_returns("post xdialog function");
}

static void
xdialog_fn(struct dialog_data *dlg_data)
{
	struct terminal *term = dlg_data->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = -1;
	int i;
	int nfields;
	struct color_pair *dialog_text_color = get_bfu_color(term, "dialog.text");

	for (nfields = 0; dlg_data->items[nfields].item->type == D_FIELD; nfields++);

	text_width(term, dlg_msg[0], &min, &max);
	buttons_width(term, dlg_data->items + nfields, 2, &min, &max);

	w = dlg_data->win->term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max) w = max;
	if (w < min) w = min;
	int_upper_bound(&w, dlg_data->win->term->x - 2 * DIALOG_LB);
	if (w < 1) w = 1;
	/*rw = 0;*/
	/*HACK*/ w = rw = 50;
	for (i = 0; i < nfields; i++) {
		dlg_format_text(NULL, term, dlg_msg[0], 0, &y, w, &rw,
				dialog_text_color, AL_LEFT);
		y += 2;
	}
	dlg_format_buttons(NULL, term, dlg_data->items + nfields, 2,
			   0, &y, w, &rw, AL_CENTER);
	w = rw;
	dlg_data->xw = w + 2 * DIALOG_LB;
	dlg_data->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg_data);
	draw_dlg(dlg_data);
	y = dlg_data->y + DIALOG_TB;
	for (i = 0; i < nfields; i++) {
		dlg_format_text(term, term, dlg_msg[0],
				dlg_data->x + DIALOG_LB, &y, w, NULL,
				dialog_text_color, AL_LEFT);
		dlg_format_field(term, term, &dlg_data->items[i],
				 dlg_data->x + DIALOG_LB, &y, w,
				 NULL, AL_LEFT);
		y++;
	}
	dlg_format_buttons(term, term, &dlg_data->items[nfields],
			   2, dlg_data->x + DIALOG_LB, &y, w,
			   NULL, AL_CENTER);
}

static int
l_xdialog(LS)
{
	struct dialog *dlg;
	struct lua_xdialog_data *data;
	int nargs, nfields, nitems;
	int i;

	nargs = lua_gettop(S);
	nfields = nargs - 1;
	nitems = nfields + 3;

	if ((nfields < 1) || (nfields > XDIALOG_MAX_FIELDS)) goto lua_error;
	for (i = 1; i < nargs; i++) if (!lua_isstring(S, i)) goto lua_error;
	if (!lua_isfunction(S, nargs)) goto lua_error;

	dlg = mem_calloc(1, sizeof(struct dialog) + nitems * sizeof(struct widget)
			  + sizeof *data);
	if (!dlg) return 0;

	data = (struct lua_xdialog_data *)&dlg->items[nitems];
	data->state = S;
	data->nfields = nfields;
	for (i = 0; i < nfields; i++)
		safe_strncpy(data->fields[i], (uchar *)lua_tostring(S, i+1),
			     MAX_STR_LEN-1);
	lua_pushvalue(S, nargs);
	data->func_ref = lua_ref(S, 1);

	dlg->title = _("User dialog", lua_ses->tab->term);
	dlg->fn = xdialog_fn;
	dlg->refresh = (void (*)(void *))xdialog_run_lua;
	dlg->refresh_data = data;

	for (i = 0; i < nfields; i++) {
		dlg->items[i].type = D_FIELD;
		dlg->items[i].dlen = MAX_STR_LEN;
		dlg->items[i].data = data->fields[i];
	}

	set_dlg_button(dlg, i, B_ENTER, ok_dialog, _("OK", lua_ses->tab->term), NULL);
	set_dlg_button(dlg, i, B_ESC, cancel_dialog, _("Cancel", lua_ses->tab->term), NULL);

	assert(i == nitems - 1);
	dlg->items[i].type = D_END;

	do_dialog(lua_ses->tab->term, dlg, getml(dlg, NULL));

	lua_pushnumber(S, 1);
	return 1;

lua_error:
	lua_pushnil(S);
	return 1;
}

/* End xdialog bit. */


/* Initialisation */

static void
do_hooks_file(LS, unsigned char *prefix, unsigned char *filename)
{
	unsigned char *file = straconcat(prefix, "/", filename, NULL);
	int oldtop = lua_gettop(S);

	lua_dofile(S, file);
	mem_free(file);
	lua_settop(S, oldtop);
}

static void
init_lua(void)
{
	L = lua_open(0);
	lua_baselibopen(L);
	lua_strlibopen(L);
	lua_register(L, LUA_ALERT, l_alert);
	lua_register(L, "current_url", l_current_url);
	lua_register(L, "current_link", l_current_link);
	lua_register(L, "current_title", l_current_title);
	lua_register(L, "current_document", l_current_document);
	lua_register(L, "current_document_formatted", l_current_document_formatted);
	lua_register(L, "enable_systems_functions", l_enable_systems_functions);
	lua_register(L, "bind_key", l_bind_key);
	lua_register(L, "edit_bookmark_dialog", l_edit_bookmark_dialog);
	lua_register(L, "xdialog", l_xdialog);

	lua_dostring(L, "function set_elinks_home(s) elinks_home = s end");
	lua_getglobal(L, "set_elinks_home");
	lua_pushstring(L, elinks_home ? elinks_home : (unsigned char *)CONFDIR);
	lua_call(L, 1, 0);

	do_hooks_file(L, CONFDIR, "hooks.lua");
	if (elinks_home) do_hooks_file(L, elinks_home, "hooks.lua");
}

static void
cleanup_lua(void)
{
	lua_close(L);
}

/* Attempt to handle infinite loops by trapping SIGINT.  If we get a
 * SIGINT, we longjump to where prepare_lua was called.  finish_lua()
 * disables the trapping. */

static void
handle_sigint(void *data)
{
	finish_lua();
	siglongjmp(errjmp, -1);
}

int
prepare_lua(struct session *ses)
{
	lua_ses = ses;
	errterm = lua_ses ? lua_ses->tab->term : NULL;
	/* XXX this uses the wrong term, I think */
	install_signal_handler(SIGINT, (void (*)(void *))handle_sigint, NULL, 1);

	return sigsetjmp(errjmp, 1);
}

void
sig_ctrl_c(struct terminal *t);

void
finish_lua(void)
{
	/* XXX should save previous handler instead of assuming this one */
	install_signal_handler(SIGINT, (void (*)(void *))sig_ctrl_c, errterm, 0);
}


/* Error reporting. */

void
alert_lua_error(unsigned char *msg)
{
	if (errterm) {
		msg_box(errterm, NULL, 0,
			N_("Lua Error"), AL_LEFT,
			msg,
			NULL, 1,
			N_("OK"), NULL, B_ENTER | B_ESC);
		return;
	}

	error("Lua: %s", msg);
	sleep(3);
}

void
alert_lua_error2(unsigned char *msg, unsigned char *msg2)
{
	unsigned char *tmp = stracpy(msg);

	if (!tmp) return;
	add_to_strn(&tmp, msg2);
	alert_lua_error(tmp);
	mem_free(tmp);
}


/* The following stuff is to handle the return values of
 * lua_console_hook and keystroke functions, and also the xdialog
 * function.  It expects two values on top of the stack. */

static void
handle_ret_eval(struct session *ses)
{
	const unsigned char *expr = lua_tostring(L, -1);

	if (expr) {
		int oldtop = lua_gettop(L);

		if (prepare_lua(ses) == 0) {
			lua_dostring(L, expr);
			lua_settop(L, oldtop);
			finish_lua();
		}
		return;
	}

	alert_lua_error("bad argument for eval");
}

static void
handle_ret_run(struct session *ses)
{
	unsigned char *cmd = (uchar *)lua_tostring(L, -1);

	if (cmd) {
		exec_on_terminal(ses->tab->term, cmd, "", 1);
		return;
	}

	alert_lua_error("bad argument for run");
}

static void
handle_ret_goto_url(struct session *ses)
{
	unsigned char *url = (uchar *)lua_tostring(L, -1);

	if (url) {
		goto_url(ses, url);
		return;
	}

	alert_lua_error("bad argument for goto_url");
}

static void
handle_standard_lua_returns(unsigned char *from)
{
	const unsigned char *act = lua_tostring(L, -2);

	if (act) {
		if (!strcmp(act, "eval"))
			handle_ret_eval(lua_ses);
		else if (!strcmp(act, "run"))
			handle_ret_run(lua_ses);
		else if (!strcmp(act, "goto_url"))
			handle_ret_goto_url(lua_ses);
		else
			alert_lua_error2("unrecognised return value from ", from);
	}
	else if (!lua_isnil(L, -2))
		alert_lua_error2("bad return type from ", from);

	lua_pop(L, 2);
}


/* Console stuff. */

static struct input_history lua_console_history = { 0, {D_LIST_HEAD(lua_console_history.items)} };

static void
lua_console(struct session *ses, unsigned char *expr)
{
	lua_getglobal(L, "lua_console_hook");
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		handle_ret_eval(ses);
		return;
	}

	lua_pushstring(L, expr);
	if (prepare_lua(ses) == 0) {
		int err = lua_call(L, 1, 2);

		finish_lua();
		if (!err) handle_standard_lua_returns("lua_console_hook");
	}
}

enum evhook_status
dialog_lua_console(va_list ap, void *data)
{
	struct session *ses = va_arg(ap, struct session *);

	input_field(ses->tab->term, NULL, 1,
		    N_("Lua Console"), N_("Enter expression"),
		    N_("OK"), N_("Cancel"), ses, &lua_console_history,
		    MAX_STR_LEN, "", 0, 0, NULL,
		    (void (*)(void *, unsigned char *)) lua_console, NULL);
	return EHS_NEXT;
}

enum evhook_status
free_lua_console_history(va_list ap, void *data)
{
	free_list(lua_console_history.items);
	return EHS_NEXT;
}


struct scripting_backend lua_scripting_backend = {
	/* init: */	init_lua,
	/* done: */	cleanup_lua,
	/* hooks: */	lua_scripting_hooks,
};

#endif
