/* Lua interface (scripting engine) */
/* $Id: core.c,v 1.2 2002/05/08 13:55:05 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_LUA

#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <lua.h>
#include <lualib.h>

#include "links.h"

#include "bfu/align.h"
#include "bfu/bfu.h"
#include "config/kbdbind.h"
#include "document/cache.h"
#include "document/history.h"
#include "document/location.h"
#include "document/session.h"
#include "document/view.h"
#include "document/vs.h"
#include "document/html/renderer.h"
#include "intl/language.h"
#include "lowlevel/home.h"
#include "lowlevel/select.h"
#include "lowlevel/terminal.h"
#include "lua/core.h"


lua_State *lua_state;

static struct session *ses;
static struct terminal *errterm;
static jmp_buf errjmp;

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
	struct view_state *vs;

	if (have_location(ses) && (vs = ses ? &cur_loc(ses)->vs : 0))
		lua_pushstring(S, vs->url);
	else
		lua_pushnil(S);

	return 1;
}

static int
l_current_link(LS)
{
	struct f_data_c *fd = current_frame(ses);

	if (fd && fd->vs->current_link != -1) {
		struct link *l = &fd->f_data->links[fd->vs->current_link];

		if (l->type == L_LINK) {
			lua_pushstring(S, l->where);
			return 1;
		}
	}

	lua_pushnil(S);
	return 1;
}

static int
l_current_title(LS)
{
	struct f_data_c *fd = current_frame(ses);

	if (fd)
		lua_pushstring(S, fd->f_data->title);
	else
		lua_pushnil(S);

	return 1;
}

static int
l_current_document(LS)
{
	unsigned char *url;
	struct cache_entry *ce;
	struct fragment *f;

	if (ses && (url = cur_loc(ses)->vs.url)
	    && find_in_cache(url, &ce) && (f = ce->frag.next))
		lua_pushlstring(S, f->data, f->length);
	else
		lua_pushnil(S);

	return 1;
}

/* XXX: This function is mostly copied from `dump_to_file'. */
static int
l_current_document_formatted(LS)
{
	extern unsigned char frame_dumb[];
	int width, old_width = 0;
	struct f_data_c *f;
	struct f_data *fd;
	int x, y;
	unsigned char *buf;
	int l = 0;

	if (lua_gettop(S) == 0) width = -1;
	else if (!lua_isnumber(S, 1)) goto err;
	else if ((width = lua_tonumber(S, 1)) <= 0) goto err;

	if (!ses || !(f = current_frame(ses))) goto err;
	if (width > 0) {
		old_width = ses->term->x, ses->term->x = width;
		html_interpret(ses);
	}
	fd = f->f_data;
	buf = init_str();
	for (y = 0; y < fd->y; y++) for (x = 0; x <= fd->data[y].l; x++) {
		int c;
		if (x == fd->data[y].l) c = '\n';
		else {
			if (((c = fd->data[y].d[x]) & 0xff) == 1) c += ' ' - 1;
			if ((c >> 15) && (c & 0xff) >= 176 && (c & 0xff) < 224) c = frame_dumb[(c & 0xff) - 176];
		}
		add_chr_to_str(&buf, &l, c);
	}
	lua_pushlstring(S, buf, l);
	mem_free(buf);
	if (width > 0) {
		ses->term->x = old_width;
		html_interpret(ses);
	}
	return 1;

	err:
	lua_pushnil(S);
	return 1;
}

static int
l_pipe_read(LS)
{
	FILE *fp;
	unsigned char *s = NULL;
	int len = 0;

	if (!lua_isstring(S, 1) || !(fp = popen(lua_tostring(S, 1), "r"))) {
		lua_pushnil(S);
		return 1;
	}

	while (!feof(fp)) {
		unsigned char buf[1024];
		int l = fread(buf, 1, sizeof buf, fp);

		s = (!s) ? s = mem_alloc(l) : mem_realloc(s, len+l);
		memcpy(s+len, buf, l);
		len += l;
	}
	pclose(fp);

	lua_pushlstring(S, s, len);
	mem_free(s);
	return 1;
}

static int
l_execute(LS)
{
	if (lua_isstring(S, 1)) {
		exec_on_terminal(ses->term, (uchar *)lua_tostring(S, 1), "", 0);
		lua_pushnumber(S, 0);
	} else {
		lua_pushnil(L);
	}

	return 1;
}

static int
l_tmpname(LS)
{
	char *fn = tempnam(NULL, "links");

	if (fn) {
		lua_pushstring(S, fn);
		free(fn);
	} else {
		alert_lua_error("Error generating temporary file name");
		lua_pushnil(S);
	}

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

static int
l_bind_key(LS)
{
	int ref;
	unsigned char *err;

	if (!lua_isstring(S, 1) || !lua_isstring(S, 2) || !lua_isfunction(S, 3)) {
		alert_lua_error("bad arguments to bind_key");
		goto error;
	}

	lua_pushvalue(S, 3);
	ref = lua_ref(S, 1);

	if ((err = bind_lua_func((uchar *)lua_tostring(S, 1), (uchar *)lua_tostring(S, 2), ref))) {
		lua_unref(S, ref);
		alert_lua_error2("error in bind_key: ", err);
		goto error;
	}

	lua_pushnumber(S, 1);
	return 1;

error:
	lua_pushnil(S);
	return 1;
}


/* Begin very hackish bit for bookmark editing dialog.  */
/* XXX: Add history and generalise.  */

static unsigned char *dlg_msg[] = {
	TEXT(T_NNAME),
	TEXT(T_NNAME),
	TEXT(T_URL),
	""
};

struct dlg_data {
	lua_State *state;
	unsigned char cat[MAX_STR_LEN];
	unsigned char name[MAX_STR_LEN];
	unsigned char url[MAX_STR_LEN];
	int func_ref;
};

static void
dialog_run_lua(struct dlg_data *data)
{
	lua_State *L = data->state;
	int err;

	lua_getref(L, data->func_ref);
	lua_pushstring(L, data->cat);
	lua_pushstring(L, data->name);
	lua_pushstring(L, data->url);
	if (prepare_lua(ses)) return;
	err = lua_call(L, 3, 2);
	finish_lua();
	lua_unref(L, data->func_ref);
	handle_standard_lua_returns("post dialog function");
}

static void
dialog_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = -1;

	max_text_width(term, dlg_msg[0], &max);
	min_text_width(term, dlg_msg[0], &min);
	max_text_width(term, dlg_msg[1], &max);
	min_text_width(term, dlg_msg[1], &min);
	max_text_width(term, dlg_msg[2], &max);
	min_text_width(term, dlg_msg[2], &min);
	max_buttons_width(term, dlg->items + 3, 2, &max);
	min_buttons_width(term, dlg->items + 3, 2, &min);
	w = dlg->win->term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max) w = max;
	if (w < min) w = min;
	if (w > dlg->win->term->x - 2 * DIALOG_LB) w = dlg->win->term->x - 2 * DIALOG_LB;
	if (w < 1) w = 1;
	rw = 0;
	/*HACK*/ w = rw = 50;
	dlg_format_text(NULL, term, dlg_msg[0], 0, &y, w, &rw, COLOR_DIALOG_TEXT, AL_LEFT);
	y += 2;
	dlg_format_text(NULL, term, dlg_msg[1], 0, &y, w, &rw, COLOR_DIALOG_TEXT, AL_LEFT);
	y += 2;
	dlg_format_text(NULL, term, dlg_msg[2], 0, &y, w, &rw, COLOR_DIALOG_TEXT, AL_LEFT);
	y += 2;
	dlg_format_buttons(NULL, term, dlg->items + 3, 2, 0, &y, w, &rw, AL_CENTER);
	w = rw;
	dlg->xw = w + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);
	draw_dlg(dlg);
	y = dlg->y + DIALOG_TB;
	dlg_format_text(term, term, dlg_msg[0], dlg->x + DIALOG_LB, &y, w, NULL, COLOR_DIALOG_TEXT, AL_LEFT);
	dlg_format_field(term, term, &dlg->items[0], dlg->x + DIALOG_LB, &y, w, NULL, AL_LEFT);
	y++;
	dlg_format_text(term, term, dlg_msg[1], dlg->x + DIALOG_LB, &y, w, NULL, COLOR_DIALOG_TEXT, AL_LEFT);
	dlg_format_field(term, term, &dlg->items[1], dlg->x + DIALOG_LB, &y, w, NULL, AL_LEFT);
	y++;
	dlg_format_text(term, term, dlg_msg[2], dlg->x + DIALOG_LB, &y, w, NULL, COLOR_DIALOG_TEXT, AL_LEFT);
	dlg_format_field(term, term, &dlg->items[2], dlg->x + DIALOG_LB, &y, w, NULL, AL_LEFT);
	y++;
	dlg_format_buttons(term, term, &dlg->items[3], 2, dlg->x + DIALOG_LB, &y, w, NULL, AL_CENTER);
}

static int
l_edit_bookmark_dialog(LS)
{
	struct dialog *d;
	size_t sz;
	struct dlg_data *data;

	if (!lua_isstring(S, 1) || !lua_isstring(S, 2) || !lua_isstring(S, 3) || !lua_isfunction(S, 4)) {
		lua_pushnil(S);
		return 1;
	}

	sz = sizeof(struct dialog) + 6 * sizeof(struct dialog_item) + sizeof *data;
	if (!(d = mem_alloc(sz))) return 0;
	memset(d, 0, sz);

	data = (struct dlg_data *)&d->items[6];
	data->state = S;
	safe_strncpy(data->cat, (uchar *)lua_tostring(S, 1), MAX_STR_LEN-1);
	safe_strncpy(data->name, (uchar *)lua_tostring(S, 2), MAX_STR_LEN-1);
	safe_strncpy(data->url, (uchar *)lua_tostring(S, 3), MAX_STR_LEN-1);
	lua_pushvalue(S, 4);
	data->func_ref = lua_ref(S, 1);

	d->title = TEXT(T_EDIT_BOOKMARK);
	d->fn = dialog_fn;
	d->refresh = (void (*)(void *))dialog_run_lua;
	d->refresh_data = data;
	d->items[0].type = D_FIELD;
	d->items[0].dlen = MAX_STR_LEN;
	d->items[0].data = data->cat;
	d->items[1].type = D_FIELD;
	d->items[1].dlen = MAX_STR_LEN;
	d->items[1].data = data->name;
	d->items[2].type = D_FIELD;
	d->items[2].dlen = MAX_STR_LEN;
	d->items[2].data = data->url;
	d->items[3].type = D_BUTTON;
	d->items[3].gid = B_ENTER;
	d->items[3].fn = ok_dialog;
	d->items[3].text = TEXT(T_OK);
	d->items[4].type = D_BUTTON;
	d->items[4].gid = B_ESC;
	d->items[4].fn = cancel_dialog;
	d->items[4].text = TEXT(T_CANCEL);
	d->items[5].type = D_END;
	do_dialog(ses->term, d, getml(d, NULL));

	lua_pushnumber(S, 1);
	return 1;
}

/* End very hackish bit.  */


/* Begin hackish bit for half-generalised dialog.  */
/* XXX: Add history and custom labels.  */

#define XDIALOG_MAX_FIELDS	5

#if 0
static unsigned char *xdialog_msg[] = {
	TEXT(T_XDIALOG_FIELD),
	""
};
#endif

struct xdialog_data {
	lua_State *state;
	int func_ref;
	int nfields;
	unsigned char fields[XDIALOG_MAX_FIELDS][MAX_STR_LEN];
};

static void
xdialog_run_lua(struct xdialog_data *data)
{
	lua_State *L = data->state;
	int err;
	int i;

	lua_getref(L, data->func_ref);
	for (i = 0; i < data->nfields; i++) lua_pushstring(L, data->fields[i]);
	if (prepare_lua(ses)) return;
	err = lua_call(L, data->nfields, 2);
	finish_lua();
	lua_unref(L, data->func_ref);
	handle_standard_lua_returns("post xdialog function");
}

static void
xdialog_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = -1;
	int i;
	int nfields;

	for (nfields = 0; dlg->items[nfields].item->type == D_FIELD; nfields++);

	max_text_width(term, dlg_msg[0], &max);
	min_text_width(term, dlg_msg[0], &min);
	max_buttons_width(term, dlg->items + nfields, 2, &max);
	min_buttons_width(term, dlg->items + nfields, 2, &min);
	w = dlg->win->term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max) w = max;
	if (w < min) w = min;
	if (w > dlg->win->term->x - 2 * DIALOG_LB) w = dlg->win->term->x - 2 * DIALOG_LB;
	if (w < 1) w = 1;
	rw = 0;
	/*HACK*/ w = rw = 50;
	for (i = 0; i < nfields; i++) {
	    dlg_format_text(NULL, term, dlg_msg[0], 0, &y, w, &rw, COLOR_DIALOG_TEXT, AL_LEFT);
	    y += 2;
	}
	dlg_format_buttons(NULL, term, dlg->items + nfields, 2, 0, &y, w, &rw, AL_CENTER);
	w = rw;
	dlg->xw = w + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);
	draw_dlg(dlg);
	y = dlg->y + DIALOG_TB;
	for (i = 0; i < nfields; i++) {
	    dlg_format_text(term, term, dlg_msg[0], dlg->x + DIALOG_LB, &y, w, NULL, COLOR_DIALOG_TEXT, AL_LEFT);
	    dlg_format_field(term, term, &dlg->items[i], dlg->x + DIALOG_LB, &y, w, NULL, AL_LEFT);
	    y++;
	}
	dlg_format_buttons(term, term, &dlg->items[nfields], 2, dlg->x + DIALOG_LB, &y, w, NULL, AL_CENTER);
}

static int
l_xdialog(LS)
{
	struct dialog *d;
	size_t sz;
	struct xdialog_data *data;
	int nargs, nfields, nitems;
	int i;

	nargs = lua_gettop(S);
	nfields = nargs - 1;
	nitems = nfields + 3;

	if ((nfields < 1) || (nfields > XDIALOG_MAX_FIELDS)) goto error;
	for (i = 1; i < nargs; i++) if (!lua_isstring(S, i)) goto error;
	if (!lua_isfunction(S, nargs)) goto error;

	sz = sizeof(struct dialog) + nitems * sizeof(struct dialog_item) + sizeof *data;
	if (!(d = mem_alloc(sz))) return 0;
	memset(d, 0, sz);

	data = (struct xdialog_data *)&d->items[nitems];
	data->state = S;
	data->nfields = nfields;
	for (i = 0; i < nfields; i++)
		safe_strncpy(data->fields[i], (uchar *)lua_tostring(S, i+1), MAX_STR_LEN-1);
	lua_pushvalue(S, nargs);
	data->func_ref = lua_ref(S, 1);

	d->title = TEXT(T_XDIALOG_TITLE);
	d->fn = xdialog_fn;
	d->refresh = (void (*)(void *))xdialog_run_lua;
	d->refresh_data = data;
	for (i = 0; i < nfields; i++) {
		d->items[i].type = D_FIELD;
		d->items[i].dlen = MAX_STR_LEN;
		d->items[i].data = data->fields[i];
	}
	d->items[i].type = D_BUTTON;
	d->items[i].gid = B_ENTER;
	d->items[i].fn = ok_dialog;
	d->items[i].text = TEXT(T_OK);
	i++;
	d->items[i].type = D_BUTTON;
	d->items[i].gid = B_ESC;
	d->items[i].fn = cancel_dialog;
	d->items[i].text = TEXT(T_CANCEL);
	i++;
	d->items[i].type = D_END;
	do_dialog(ses->term, d, getml(d, NULL));

	lua_pushnumber(S, 1);
	return 1;

error:
	lua_pushnil(S);
	return 1;
}

/* End xdialog bit.  */


/*
 * Initialisation
 */

static void
do_hooks_file(LS, unsigned char *prefix, unsigned char *filename)
{
	int oldtop = lua_gettop(S);
	unsigned char *file = stracpy(prefix);

	add_to_strn(&file, filename);
	lua_dofile(S, file);
	mem_free(file);
	lua_settop(S, oldtop);
}

void
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
	do_hooks_file(L, "/etc/", "links-hooks.lua");
	if (links_home) do_hooks_file(L, links_home, "hooks.lua");
}


/*
 * Attempt to handle infinite loops by trapping SIGINT.  If we get a
 * SIGINT, we longjump to where prepare_lua was called.  finish_lua()
 * disables the trapping.
 */

static void
handle_sigint(void *data)
{
	finish_lua();
	siglongjmp(errjmp, -1);
}

int
prepare_lua(struct session *_ses)
{
	ses = _ses;
	errterm = ses ? ses->term : NULL;
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


/*
 * Error reporting.
 */

void
alert_lua_error(unsigned char *msg)
{
	if (errterm) {
		msg_box(errterm, NULL,
			TEXT(T_LUA_ERROR), AL_LEFT,
			msg,
			NULL, 1,
			TEXT(T_OK), NULL, B_ENTER | B_ESC);
	} else {
		fprintf(stderr, "Lua Error: %s\n", msg);
		sleep(3);
	}
}

void
alert_lua_error2(unsigned char *msg, unsigned char *msg2)
{
	unsigned char *tmp;

	tmp = stracpy(msg);
	add_to_strn(&tmp, msg2);
	alert_lua_error(tmp);
	mem_free(tmp);
}


/*
 * The following stuff is to handle the return values of
 * lua_console_hook and keystroke functions, and also the xdialog
 * function.  It expects two values on top of the stack.
 */

static void
handle_ret_eval(struct session *ses)
{
	const unsigned char *expr;
	int oldtop;

	if (!(expr = lua_tostring(L, -1))) {
		alert_lua_error("bad argument for eval");
		return;
	}

	oldtop = lua_gettop(L);
	if (prepare_lua(ses) == 0) {
		lua_dostring(L, expr);
		lua_settop(L, oldtop);
		finish_lua();
	}
}

static void
handle_ret_run(struct session *ses)
{
	unsigned char *cmd;

	if (!(cmd = (uchar *)lua_tostring(L, -1)))
		alert_lua_error("bad argument for run");
	else
		exec_on_terminal(ses->term, cmd, "", 1);
}

static void
handle_ret_goto_url(struct session *ses)
{
	unsigned char *url;

	if (!(url = (uchar *)lua_tostring(L, -1)))
		alert_lua_error("bad argument for goto_url");
	else
		goto_url(ses, url);
}

static void
handle_standard_lua_returns(unsigned char *from)
{
	const unsigned char *act;

	if ((act = lua_tostring(L, -2))) {
		if (!strcmp(act, "eval"))
			handle_ret_eval(ses);
		else if (!strcmp(act, "run"))
			handle_ret_run(ses);
		else if (!strcmp(act, "goto_url"))
			handle_ret_goto_url(ses);
		else
			alert_lua_error2("unrecognised return value from ", from);
	}
	else if (!lua_isnil(L, -2))
		alert_lua_error2("bad return type from ", from);

	lua_pop(L, 2);
}


/*
 * Console stuff.
 */

static struct input_history lua_console_history = { 0, {&lua_console_history.items, &lua_console_history.items} };

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
		if (!err)
			handle_standard_lua_returns("lua_console_hook");
	}
}

void
dialog_lua_console(struct session *ses)
{
	input_field(ses->term, NULL, TEXT(T_LUA_CONSOLE), TEXT(T_ENTER_EXPRESSION), TEXT(T_OK), TEXT(T_CANCEL), ses, &lua_console_history, MAX_STR_LEN, "", 0, 0, NULL, (void (*)(void *, unsigned char *)) lua_console, NULL);
}

void
free_lua_console_history(void)
{
	free_list(lua_console_history.items);
}


/*
 * Helper to run Lua functions bound to keystrokes.
 */

void
run_lua_func(struct session *ses, int func_ref)
{
	int err;

	if (func_ref == LUA_NOREF) {
		alert_lua_error("key bound to nothing (internal error)");
		return;
	}

	lua_getref(L, func_ref);
	if (prepare_lua(ses))
		return;
	err = lua_call(L, 0, 2);
	finish_lua();
	if (!err)
		handle_standard_lua_returns("keyboard function");
}


#endif
