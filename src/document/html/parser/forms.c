/* HTML forms parser */
/* $Id: forms.c,v 1.5 2004/04/29 13:30:00 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/listmenu.h"
#include "bfu/menu.h"
#include "document/html/parser/forms.h"
#include "document/html/parser/link.h"
#include "document/html/parser/stack.h"
#include "document/html/parser/parse.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "intl/charsets.h"
#include "protocol/uri.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memdebug.h"
#include "util/memlist.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/form.h"

/* Unsafe macros */
#include "document/html/internal.h"


struct form {
	unsigned char *action;
	unsigned char *target;
	enum form_method method;
	int num;
};

static struct form form;


void
done_form(void)
{
	mem_free_if(form.action);
	mem_free_if(form.target);
	memset(&form, 0, sizeof(form));
}

void
html_form(unsigned char *a)
{
	was_br = 1;
}

static void
get_html_form(unsigned char *a, struct form *form)
{
	unsigned char *al;

	form->method = FM_GET;

	al = get_attr_val(a, "method");
	if (al) {
		if (!strcasecmp(al, "post")) {
			unsigned char *enctype = get_attr_val(a, "enctype");

			form->method = FM_POST;
			if (enctype) {
				if (!strcasecmp(enctype, "multipart/form-data"))
					form->method = FM_POST_MP;
				mem_free(enctype);
			}
		}
		mem_free(al);
	}

	al = get_attr_val(a, "action");
	if (al) {
		form->action = join_urls(format.href_base, trim_chars(al, ' ', 0));
		mem_free(al);
	} else {
		form->action = stracpy(format.href_base);
		if (form->action) {
			int len = get_no_post_url_length(form->action);

			form->action[len] = '\0';

			/* We have to do following for GET method, because we would end
			 * up with two '?' otherwise. */
			if (form->method == FM_GET) {
				unsigned char *ch = strchr(form->action, '?');

				if (ch) *ch = '\0';
			}
		}
	}

	al = get_target(a);
	if (al) {
		form->target = al;
	} else {
		form->target = stracpy(format.target_base);
	}

	form->num = a - startf;
}

static void
find_form_for_input(unsigned char *i)
{
	unsigned char *s, *ss, *name, *attr;
	unsigned char *lf = NULL;
	unsigned char *la = NULL;
	int namelen;

	done_form();
	
	if (!special_f(ff, SP_USED, NULL)) return;

	if (last_input_tag && i <= last_input_tag && i > last_form_tag) {
		get_html_form(last_form_attr, &form);
		return;
	}
	if (last_input_tag && i > last_input_tag)
		s = last_form_tag;
	else
		s = startf;

se:
	while (s < i && *s != '<') {

sp:
		s++;
	}
	if (s >= i) goto end_parse;
	if (s + 2 <= eofff && (s[1] == '!' || s[1] == '?')) {
		s = skip_comment(s, i);
		goto se;
	}
	ss = s;
	if (parse_element(s, i, &name, &namelen, &attr, &s)) goto sp;
	if (strlcasecmp(name, namelen, "FORM", 4)) goto se;
	lf = ss;
	la = attr;
	goto se;


end_parse:
	if (lf && la) {
		last_form_tag = lf;
		last_form_attr = la;
		last_input_tag = i;
		get_html_form(la, &form);
	} else {
		memset(&form, 0, sizeof(struct form));
	}
}

void
html_button(unsigned char *a)
{
	unsigned char *al;
	struct form_control *fc;

	find_form_for_input(a);
	html_focusable(a);

	fc = mem_calloc(1, sizeof(struct form_control));
	if (!fc) return;

	al = get_attr_val(a, "type");
	if (!al) {
		fc->type = FC_SUBMIT;
		goto xxx;
	}

	if (!strcasecmp(al, "submit")) fc->type = FC_SUBMIT;
	else if (!strcasecmp(al, "reset")) fc->type = FC_RESET;
	else if (!strcasecmp(al, "button")) {
		mem_free(al);
		put_chrs(" [&nbsp;", 8, put_chars_f, ff);

		al = get_attr_val(a, "value");
		if (al) {
			put_chrs(al, strlen(al), put_chars_f, ff);
			mem_free(al);
		} else put_chrs("BUTTON", 6, put_chars_f, ff);

		put_chrs("&nbsp;] ", 8, put_chars_f, ff);
		mem_free(fc);
		return;
	} else {
		mem_free(al);
		mem_free(fc);
		return;
	}
	mem_free(al);

xxx:
	fc->form_num = last_form_tag - startf;
	fc->ctrl_num = a - last_form_tag;
	fc->position = a - startf;
	fc->method = form.method;
	fc->action = null_or_stracpy(form.action);
	fc->name = get_attr_val(a, "name");

	fc->default_value = get_attr_val(a, "value");
	if (!fc->default_value && fc->type == FC_SUBMIT) fc->default_value = stracpy("Submit");
	if (!fc->default_value && fc->type == FC_RESET) fc->default_value = stracpy("Reset");
	if (!fc->default_value) fc->default_value = stracpy("");

	fc->ro = has_attr(a, "disabled") ? 2 : has_attr(a, "readonly") ? 1 : 0;
	if (fc->type == FC_IMAGE) fc->alt = get_attr_val(a, "alt");
	special_f(ff, SP_CONTROL, fc);
	format.form = fc;
	format.attr |= AT_BOLD;
#if 0
	put_chrs("[&nbsp;", 7, put_chars_f, ff);
	if (fc->default_value) put_chrs(fc->default_value, strlen(fc->default_value), put_chars_f, ff);
	put_chrs("&nbsp;]", 7, put_chars_f, ff);
	put_chrs(" ", 1, put_chars_f, ff);
#endif
}

void
html_input(unsigned char *a)
{
	int i;
	unsigned char *al;
	struct form_control *fc;

	find_form_for_input(a);
	html_focusable(a);

	fc = mem_calloc(1, sizeof(struct form_control));
	if (!fc) return;

	al = get_attr_val(a, "type");
	if (!al) {
		fc->type = FC_TEXT;
		goto xxx;
	}
	if (!strcasecmp(al, "text")) fc->type = FC_TEXT;
	else if (!strcasecmp(al, "password")) fc->type = FC_PASSWORD;
	else if (!strcasecmp(al, "checkbox")) fc->type = FC_CHECKBOX;
	else if (!strcasecmp(al, "radio")) fc->type = FC_RADIO;
	else if (!strcasecmp(al, "submit")) fc->type = FC_SUBMIT;
	else if (!strcasecmp(al, "reset")) fc->type = FC_RESET;
	else if (!strcasecmp(al, "file")) fc->type = FC_FILE;
	else if (!strcasecmp(al, "hidden")) fc->type = FC_HIDDEN;
	else if (!strcasecmp(al, "image")) fc->type = FC_IMAGE;
	else if (!strcasecmp(al, "button")) {
		mem_free(al);
		put_chrs(" [&nbsp;", 8, put_chars_f, ff);

		al = get_attr_val(a, "value");
		if (al) {
			put_chrs(al, strlen(al), put_chars_f, ff);
			mem_free(al);
		} else put_chrs("BUTTON", 6, put_chars_f, ff);

		put_chrs("&nbsp;] ", 8, put_chars_f, ff);
		mem_free(fc);
		return;
	} else fc->type = FC_TEXT;
	mem_free(al);

xxx:
	fc->form_num = last_form_tag - startf;
	fc->ctrl_num = a - last_form_tag;
	fc->position = a - startf;
	fc->method = form.method;
	fc->action = null_or_stracpy(form.action);
	fc->target = null_or_stracpy(form.target);
	fc->name = get_attr_val(a, "name");

	if (fc->type != FC_FILE) fc->default_value = get_attr_val(a, "value");
	if (!fc->default_value && fc->type == FC_CHECKBOX) fc->default_value = stracpy("on");
	if (!fc->default_value && fc->type == FC_SUBMIT) fc->default_value = stracpy("Submit");
	if (!fc->default_value && fc->type == FC_RESET) fc->default_value = stracpy("Reset");
	if (!fc->default_value) fc->default_value = stracpy("");

	fc->size = get_num(a, "size");
	if (fc->size == -1) fc->size = global_doc_opts->default_form_input_size;
	fc->size++;
	if (fc->size > global_doc_opts->width) fc->size = global_doc_opts->width;
	fc->maxlength = get_num(a, "maxlength");
	if (fc->maxlength == -1) fc->maxlength = MAXINT;
	if (fc->type == FC_CHECKBOX || fc->type == FC_RADIO) fc->default_state = has_attr(a, "checked");
	fc->ro = has_attr(a, "disabled") ? 2 : has_attr(a, "readonly") ? 1 : 0;
	if (fc->type == FC_IMAGE) fc->alt = get_attr_val(a, "alt");
	if (fc->type == FC_HIDDEN) goto hid;

	put_chrs(" ", 1, put_chars_f, ff);
	html_stack_dup(ELEMENT_KILLABLE);
	format.form = fc;
	if (format.title) mem_free(format.title);
	format.title = get_attr_val(a, "title");
	switch (fc->type) {
		case FC_TEXT:
		case FC_PASSWORD:
		case FC_FILE:
			format.attr |= AT_BOLD;
			for (i = 0; i < fc->size; i++) put_chrs("_", 1, put_chars_f, ff);
			break;
		case FC_CHECKBOX:
			format.attr |= AT_BOLD;
			put_chrs("[&nbsp;]", 8, put_chars_f, ff);
			break;
		case FC_RADIO:
			format.attr |= AT_BOLD;
			put_chrs("(&nbsp;)", 8, put_chars_f, ff);
			break;
		case FC_IMAGE:
			mem_free_set(&format.image, NULL);
			al = get_url_val(a, "src");
			if (!al) al = get_url_val(a, "dynsrc");
			if (al) {
				format.image = join_urls(format.href_base, al);
				mem_free(al);
			}
			format.attr |= AT_BOLD;
			put_chrs("[&nbsp;", 7, put_chars_f, ff);
			if (fc->alt)
				put_chrs(fc->alt, strlen(fc->alt), put_chars_f, ff);
			else if (fc->name)
				put_chrs(fc->name, strlen(fc->name), put_chars_f, ff);
			else
				put_chrs("Submit", 6, put_chars_f, ff);

			put_chrs("&nbsp;]", 7, put_chars_f, ff);
			break;
		case FC_SUBMIT:
		case FC_RESET:
			format.attr |= AT_BOLD;
			put_chrs("[&nbsp;", 7, put_chars_f, ff);
			if (fc->default_value)
				put_chrs(fc->default_value, strlen(fc->default_value), put_chars_f, ff);
			put_chrs("&nbsp;]", 7, put_chars_f, ff);
			break;
		case FC_TEXTAREA:
		case FC_SELECT:
		case FC_HIDDEN:
			INTERNAL("bad control type");
	}
	kill_html_stack_item(&html_top);
	put_chrs(" ", 1, put_chars_f, ff);

hid:
	special_f(ff, SP_CONTROL, fc);
}

void
html_select(unsigned char *a)
{
	/* Note I haven't seen this code in use, do_html_select() seems to take
	 * care of bussiness. --FF */

	unsigned char *al = get_attr_val(a, "name");

	if (!al) return;
	html_focusable(a);
	html_top.type = ELEMENT_DONT_KILL;
	format.select = al;
	format.select_disabled = 2 * has_attr(a, "disabled");
}

void
html_option(unsigned char *a)
{
	struct form_control *fc;
	unsigned char *val;

	find_form_for_input(a);
	if (!format.select) return;

	fc = mem_calloc(1, sizeof(struct form_control));
	if (!fc) return;

	val = get_attr_val(a, "value");
	if (!val) {
		struct string str;
		unsigned char *p, *r;
		unsigned char *name;
		int namelen;

		for (p = a - 1; *p != '<'; p--);

		if (!init_string(&str)) goto x;
		if (parse_element(p, eoff, NULL, NULL, NULL, &p)) {
			INTERNAL("parse element failed");
			val = str.source;
			goto x;
		}

rrrr:
		while (p < eoff && isspace(*p)) p++;
		while (p < eoff && !isspace(*p) && *p != '<') {

pppp:
			add_char_to_string(&str, *p), p++;
		}

		r = p;
		val = str.source; /* Has to be before the possible 'goto x' */

		while (r < eoff && isspace(*r)) r++;
		if (r >= eoff) goto x;
		if (r - 2 <= eoff && (r[1] == '!' || r[1] == '?')) {
			p = skip_comment(r, eoff);
			goto rrrr;
		}
		if (parse_element(r, eoff, &name, &namelen, NULL, &p)) goto pppp;
		if (strlcasecmp(name, namelen, "OPTION", 6)
		    && strlcasecmp(name, namelen, "/OPTION", 7)
		    && strlcasecmp(name, namelen, "SELECT", 6)
		    && strlcasecmp(name, namelen, "/SELECT", 7)
		    && strlcasecmp(name, namelen, "OPTGROUP", 8)
		    && strlcasecmp(name, namelen, "/OPTGROUP", 9))
			goto rrrr;
	}

x:
	fc->form_num = last_form_tag - startf;
	fc->ctrl_num = a - last_form_tag;
	fc->position = a - startf;
	fc->method = form.method;
	fc->action = null_or_stracpy(form.action);
	fc->type = FC_CHECKBOX;
	fc->name = null_or_stracpy(format.select);
	fc->default_value = val;
	fc->default_state = has_attr(a, "selected");
	fc->ro = format.select_disabled;
	if (has_attr(a, "disabled")) fc->ro = 2;
	put_chrs(" ", 1, put_chars_f, ff);
	html_stack_dup(ELEMENT_KILLABLE);
	format.form = fc;
	format.attr |= AT_BOLD;
	put_chrs("[ ]", 3, put_chars_f, ff);
	kill_html_stack_item(&html_top);
	put_chrs(" ", 1, put_chars_f, ff);
	special_f(ff, SP_CONTROL, fc);
}

static struct list_menu lnk_menu;

int
do_html_select(unsigned char *attr, unsigned char *html,
	       unsigned char *eof, unsigned char **end, void *f)
{
	struct conv_table *ct = special_f(f, SP_TABLE, NULL);
	struct form_control *fc;
	struct string lbl = NULL_STRING;
	unsigned char **values = NULL;
	unsigned char **labels;
	unsigned char *t_name, *t_attr, *en;
	int t_namelen;
	int nnmi = 0;
	int order = 0;
	int preselect = -1;
	int group = 0;
	int i, max_width;

	if (has_attr(attr, "multiple")) return 1;
	find_form_for_input(attr);
	html_focusable(attr);
	init_menu(&lnk_menu);

se:
        en = html;

see:
        html = en;
	while (html < eof && *html != '<') html++;

	if (html >= eof) {

abort:
		*end = html;
		if (lbl.source) done_string(&lbl);
		if (values) {
			int j;

			for (j = 0; j < order; j++)
				mem_free_if(values[j]);
			mem_free(values);
		}
		destroy_menu(&lnk_menu);
		*end = en;
		return 0;
	}

	if (lbl.source) {
		unsigned char *q, *s = en;
		int l = html - en;

		while (l && isspace(s[0])) s++, l--;
		while (l && isspace(s[l-1])) l--;
		q = convert_string(ct, s, l, CSM_DEFAULT);
		if (q) add_to_string(&lbl, q), mem_free(q);
	}

	if (html + 2 <= eof && (html[1] == '!' || html[1] == '?')) {
		html = skip_comment(html, eof);
		goto se;
	}

	if (parse_element(html, eof, &t_name, &t_namelen, &t_attr, &en)) {
		html++;
		goto se;
	}

	if (!strlcasecmp(t_name, t_namelen, "/SELECT", 7)) {
		add_select_item(&lnk_menu, &lbl, values, order, nnmi);
		goto end_parse;
	}

	if (!strlcasecmp(t_name, t_namelen, "/OPTION", 7)) {
		add_select_item(&lnk_menu, &lbl, values, order, nnmi);
		goto see;
	}

	if (!strlcasecmp(t_name, t_namelen, "OPTION", 6)) {
		unsigned char *value, *label;

		add_select_item(&lnk_menu, &lbl, values, order, nnmi);

		if (has_attr(t_attr, "disabled")) goto see;
		if (preselect == -1 && has_attr(t_attr, "selected")) preselect = order;
		value = get_attr_val(t_attr, "value");

		if (!mem_align_alloc(&values, order, order + 1, unsigned char *, 0xFF))
			goto abort;

		values[order++] = value;
		label = get_attr_val(t_attr, "label");
		if (label) new_menu_item(&lnk_menu, label, order - 1, 0);
		if (!value || !label) {
			init_string(&lbl);
			nnmi = !!label;
		}
		goto see;
	}

	if (!strlcasecmp(t_name, t_namelen, "OPTGROUP", 8)
	    || !strlcasecmp(t_name, t_namelen, "/OPTGROUP", 9)) {
		add_select_item(&lnk_menu, &lbl, values, order, nnmi);

		if (group) new_menu_item(&lnk_menu, NULL, -1, 0), group = 0;
	}

	if (!strlcasecmp(t_name, t_namelen, "OPTGROUP", 8)) {
		unsigned char *label = get_attr_val(t_attr, "label");

		if (!label) {
			label = stracpy("");
			if (!label) goto see;
		}
		new_menu_item(&lnk_menu, label, -1, 0);
		group = 1;
	}
	goto see;


end_parse:
	*end = en;
	if (!order) goto abort;

	fc = mem_calloc(1, sizeof(struct form_control));
	if (!fc) goto abort;

	labels = mem_calloc(order, sizeof(unsigned char *));
	if (!labels) {
		mem_free(fc);
		goto abort;
	}

	fc->form_num = last_form_tag - startf;
	fc->ctrl_num = attr - last_form_tag;
	fc->position = attr - startf;
	fc->method = form.method;
	fc->action = null_or_stracpy(form.action);
	fc->name = get_attr_val(attr, "name");
	fc->type = FC_SELECT;
	fc->default_state = preselect < 0 ? 0 : preselect;
	fc->default_value = order ? stracpy(values[fc->default_state]) : stracpy("");
	fc->ro = has_attr(attr, "disabled") ? 2 : has_attr(attr, "readonly") ? 1 : 0;
	fc->nvalues = order;
	fc->values = values;
	fc->menu = detach_menu(&lnk_menu);
	fc->labels = labels;

	menu_labels(fc->menu, "", labels);
	put_chrs("[", 1, put_chars_f, f);
	html_stack_dup(ELEMENT_KILLABLE);
	format.form = fc;
	format.attr |= AT_BOLD;

	max_width = 0;
	for (i = 0; i < order; i++) {
		if (!labels[i]) continue;
		int_lower_bound(&max_width, strlen(labels[i]));
	}

	for (i = 0; i < max_width; i++)
		put_chrs("_", 1, put_chars_f, f);

	kill_html_stack_item(&html_top);
	put_chrs("]", 1, put_chars_f, f);
	special_f(ff, SP_CONTROL, fc);

	return 0;
}

void
html_textarea(unsigned char *a)
{
	INTERNAL("This should be never called");
}

void
do_html_textarea(unsigned char *attr, unsigned char *html, unsigned char *eof,
		 unsigned char **end, void *f)
{
	struct form_control *fc;
	unsigned char *p, *t_name, *wrap_attr;
	int t_namelen;
	int cols, rows;
	int i;

	find_form_for_input(attr);
	html_focusable(attr);
	while (html < eof && (*html == '\n' || *html == '\r')) html++;
	p = html;
	while (p < eof && *p != '<') {

pp:
		p++;
	}
	if (p >= eof) {
		*end = eof;
		return;
	}
	if (parse_element(p, eof, &t_name, &t_namelen, NULL, end)) goto pp;
	if (strlcasecmp(t_name, t_namelen, "/TEXTAREA", 9)) goto pp;

	fc = mem_calloc(1, sizeof(struct form_control));
	if (!fc) return;

	fc->form_num = last_form_tag - startf;
	fc->ctrl_num = attr - last_form_tag;
	fc->position = attr - startf;
	fc->method = form.method;
	fc->action = null_or_stracpy(form.action);
	fc->name = get_attr_val(attr, "name");
	fc->type = FC_TEXTAREA;;
	fc->ro = has_attr(attr, "disabled") ? 2 : has_attr(attr, "readonly") ? 1 : 0;
	fc->default_value = memacpy(html, p - html);
	for (p = fc->default_value; p && p[0]; p++) {
		/* FIXME: We don't cope well with entities here. Bugzilla uses
		 * &#13; inside of textarea and we fail miserably upon that
		 * one.  --pasky */
		if (p[0] == '\r') {
			if (p[1] == '\n'
			    || (p > fc->default_value && p[-1] == '\n')) {
				memcpy(p, p + 1, strlen(p));
				p--;
			} else {
				p[0] = '\n';
			}
		}
	}

	cols = get_num(attr, "cols");
	if (cols <= 0) cols = global_doc_opts->default_form_input_size;
	cols++; /* Add 1 column, other browsers may have different
		   behavior here (mozilla adds 2) --Zas */
	if (cols > global_doc_opts->width) cols = global_doc_opts->width;
	fc->cols = cols;

	rows = get_num(attr, "rows");
	if (rows <= 0) rows = 1;
	if (rows > global_doc_opts->height) rows = global_doc_opts->height;
	fc->rows = rows;
	global_doc_opts->needs_height = 1;

	wrap_attr = get_attr_val(attr, "wrap");
	if (wrap_attr) {
		if (!strcasecmp(wrap_attr, "hard")
		    || !strcasecmp(wrap_attr, "physical")) {
			fc->wrap = 2;
		} else if (!strcasecmp(wrap_attr, "soft")
			   || !strcasecmp(wrap_attr, "virtual")) {
			fc->wrap = 1;
		} else if (!strcasecmp(wrap_attr, "none")
			   || !strcasecmp(wrap_attr, "off")) {
			fc->wrap = 0;
		}
		mem_free(wrap_attr);
	} else {
		fc->wrap = 1;
	}

	fc->maxlength = get_num(attr, "maxlength");
	if (fc->maxlength == -1) fc->maxlength = MAXINT;

	if (rows > 1) ln_break(1, line_break_f, f);
	else put_chrs(" ", 1, put_chars_f, f);

	html_stack_dup(ELEMENT_KILLABLE);
	format.form = fc;
	format.attr |= AT_BOLD;

	for (i = 0; i < rows; i++) {
		int j;

		for (j = 0; j < cols; j++)
			put_chrs("_", 1, put_chars_f, f);
		if (i < rows - 1)
			ln_break(1, line_break_f, f);
	}

	kill_html_stack_item(&html_top);
	if (rows > 1) ln_break(1, line_break_f, f);
	else put_chrs(" ", 1, put_chars_f, f);
	special_f(f, SP_CONTROL, fc);
}
