/* Ruby interface (scripting engine) */
/* $Id: core.c,v 1.4 2005/01/20 09:44:45 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ruby.h>

#undef _

#include "elinks.h"

#include "bfu/dialog.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/home.h"
#include "modules/module.h"
#include "sched/session.h"
#include "scripting/ruby/core.h"
#include "scripting/ruby/hooks.h"
#include "scripting/ruby/ruby.h"
#include "scripting/scripting.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/error.h"
#include "util/file.h"
#include "util/string.h"

#define RUBY_HOOKS_FILENAME	"hooks.rb"


/* I've decided to use `erb' to refer to this ELinks/ruby thingy. */

VALUE erb_module;


/* Error reporting. */

void
alert_ruby_error(struct session *ses, unsigned char *msg)
{
	struct terminal *term;

	if (!ses) {
		if (list_empty(terminals)) {
			usrerror("Ruby: %s", msg);
			return;
		}

		term = terminals.next;

	} else {
		term = ses->tab->term;
	}

	msg = stracpy(msg);
	if (!msg) return;

	msg_box(term, NULL, MSGBOX_NO_TEXT_INTL | MSGBOX_FREE_TEXT,
		N_("Ruby Error"), ALIGN_LEFT,
		msg,
		NULL, 1,
		N_("OK"), NULL, B_ENTER | B_ESC);
}

/* Another Vim treat. */
void
erb_report_error(struct session *ses, int error)
{
	VALUE eclass;
	VALUE einfo;
	unsigned char buff[MAX_STR_LEN];
	unsigned char *msg;

	/* These are from the Ruby internals. */
#define TAG_RETURN	0x1
#define TAG_BREAK	0x2
#define TAG_NEXT	0x3
#define TAG_RETRY	0x4
#define TAG_REDO	0x5
#define TAG_RAISE	0x6
#define TAG_THROW	0x7
#define TAG_FATAL	0x8
#define TAG_MASK	0xf

	switch (error) {
	case TAG_RETURN:
		msg = "unexpected return";
		break;
	case TAG_NEXT:
		msg = "unexpected next";
		break;
	case TAG_BREAK:
		msg = "unexpected break";
		break;
	case TAG_REDO:
		msg = "unexpected redo";
		break;
	case TAG_RETRY:
		msg = "retry outside of rescue clause";
		break;
	case TAG_RAISE:
	case TAG_FATAL:
		eclass = CLASS_OF(ruby_errinfo);
		einfo = rb_obj_as_string(ruby_errinfo);

		if (eclass == rb_eRuntimeError && RSTRING(einfo)->len == 0) {
			msg = "unhandled exception";

		} else {
			VALUE epath;
			unsigned char *p;

			epath = rb_class_path(eclass);
			snprintf(buff, MAX_STR_LEN, "%s: %s",
				RSTRING(epath)->ptr, RSTRING(einfo)->ptr);

			p = strchr(buff, '\n');
			if (p) *p = '\0';
			msg = buff;
		}
		break;
	default:
		snprintf(buff, MAX_STR_LEN, "unknown longjmp status %d", error);
		msg = buff;
		break;
	}

	alert_ruby_error(ses, msg);
}


/* The ELinks module: */

static VALUE
erb_module_message(VALUE self, VALUE str)
{
	unsigned char *buff, *p;

	if (list_empty(terminals))
		return Qnil;

	str = rb_obj_as_string(str);
	buff = ALLOCA_N(unsigned char, RSTRING(str)->len);
	if (buff) {
		strcpy(buff, RSTRING(str)->ptr);

		p = strchr(buff, '\n');
		if (p) *p = '\0';

		msg_box(terminals.next, NULL, MSGBOX_NO_TEXT_INTL,
			N_("Ruby Message"), ALIGN_LEFT,
			p,
			NULL, 1,
			N_("OK"), NULL, B_ENTER | B_ESC);
	}

	return Qnil;
}

static VALUE
erb_module_method_missing(VALUE self, VALUE arg)
{
	return Qnil;
}

static void
init_erb_module(void)
{
	unsigned char *home;

	erb_module = rb_define_module("ELinks");
	rb_define_const(erb_module, "VERSION", rb_str_new2(VERSION_STRING));

	home = elinks_home ? elinks_home : (unsigned char *) CONFDIR;
	rb_define_const(erb_module, "HOME", rb_str_new2(home));

	rb_define_module_function(erb_module, "message", erb_module_message, 1);
	rb_define_module_function(erb_module, "method_missing", erb_module_method_missing, -1);
}


static VALUE
erb_stdout_p(int argc, VALUE *argv, VALUE self)
{
	int i;
	VALUE str = rb_str_new("", 0);

	for (i = 0; i < argc; i++) {
		if (i > 0) rb_str_cat(str, ", ", 2);
		rb_str_concat(str, rb_inspect(argv[i]));
	}

	if (list_empty(terminals))
		return Qnil;

	msg_box(terminals.next, NULL, MSGBOX_NO_TEXT_INTL,
		N_("Ruby Message"), ALIGN_LEFT,
		RSTRING(str)->ptr,
		NULL, 1,
		N_("OK"), NULL, B_ENTER | B_ESC);

	return Qnil;
}


static void
init_ruby(struct module *module)
{
	unsigned char *path;

	/* Set up and initialize the interpreter. This function should be called
	 * before any other Ruby-related functions. */
	ruby_init();
	ruby_script("ELinks-ruby");
	ruby_init_loadpath();

	/* ``Trap'' debug prints from scripts. */
#if 0
	/* Vim does this */
	rb_stdout = rb_obj_alloc(rb_cObject);
#endif
	rb_define_singleton_method(rb_stdout, "write", erb_module_message, 1);
	rb_define_global_function("p", erb_stdout_p, -1);

	/* Set up the ELinks module interface. */
	init_erb_module();

	if (elinks_home) {
		path = straconcat(elinks_home, RUBY_HOOKS_FILENAME, NULL);

	} else {
		path = stracpy(CONFDIR "/" RUBY_HOOKS_FILENAME);
	}

	if (!path) return;

	if (file_can_read(path)) {
		int error;

		/* Load ~/.elinks/hooks.rb into the interpreter. */
		//rb_load_file(path);
		rb_load_protect(rb_str_new2(path), 0, &error);
		if (error)
			erb_report_error(NULL, error);
	}

	mem_free(path);
}


struct module ruby_scripting_module = struct_module(
	/* name: */		"Ruby",
	/* options: */		NULL,
	/* events: */		ruby_scripting_hooks,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_ruby,
	/* done: */		NULL
);
