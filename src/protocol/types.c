/* Internal MIME types implementation */
/* $Id: types.c,v 1.27 2002/06/17 11:23:18 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "links.h"

#include "bfu/align.h"
#include "bfu/bfu.h"
#include "bfu/menu.h"
#include "config/options.h"
#include "intl/language.h"
#include "lowlevel/terminal.h"
#include "protocol/http/header.h"
#include "protocol/types.h"
#include "protocol/url.h"
#include "util/memory.h"
#include "util/string.h"

struct list_head mailto_prog = { &mailto_prog, &mailto_prog };
struct list_head telnet_prog = { &telnet_prog, &telnet_prog };
struct list_head tn3270_prog = { &tn3270_prog, &tn3270_prog };

struct list_head assoc = { &assoc, &assoc };


struct option *
get_real_opt(unsigned char *base, unsigned char *id)
{
	struct option *opt;
	unsigned char *name = straconcat(base, ".", id, NULL);

	if (!name) return NULL;

	get_opt_rec(root_options, base)->flags &= ~OPT_AUTOCREATE;
	opt = get_opt_rec(root_options, name);
	get_opt_rec(root_options, base)->flags |= OPT_AUTOCREATE;

	mem_free(name);
	return opt;
}


tcount
get_assoc_cnt()
{
	static tcount assoc_cnt = 0;

	if (!++assoc_cnt) assoc_cnt = 1;
	return assoc_cnt;
}


void
delete_association(struct assoc *del)
{
	del_from_list(del);
	mem_free(del->label);
	mem_free(del->ct);
	mem_free(del->prog);
	mem_free(del);
}


/* Comma-separated list managing functions. */
/* TODO: Move to util/. --pasky */
int
is_in_list_iterate(unsigned char *list, unsigned char *str, int l,
		   int (*cmp)(unsigned char *, unsigned char *,
			      unsigned char *, int))
{
	unsigned char *tok_sep, *tok_end;

	if (!l) return 0;

	while (1) {
		/* Skip leading whitespaces */
		while (*list && *list <= ' ') list++;
		if (!*list) return 0;

		/* Move to token end */
		for (tok_sep = list;
		     *tok_sep && *tok_sep != ',';
		     tok_sep++);

		/* Move back to token start */
		for (tok_end = tok_sep - 1;
		     tok_end >= list && *tok_end <= ' ';
		     tok_end--);
		tok_end++;

		/* Compare the token */
		if (cmp(list, tok_end, str, l))
			return 1;

		/* Jump to next token */
		list = tok_sep;
		if (*list == ',') list++; /* It can be \0 as well. */
	}
}


int
is_in_list_cmp(unsigned char *start, unsigned char *end,
	       unsigned char *str, int l)
{
	return (end - start == l && !casecmp(str, start, l));
}


int
is_in_list(unsigned char *list, unsigned char *str, int l)
{
	return is_in_list_iterate(list, str, l, is_in_list_cmp);
}


int
is_in_list_rear_cmp(unsigned char *start, unsigned char *end,
		    unsigned char *str, int l)
{
	int l2 = end - start;

	/* XXX: The '.' cmp is hack for extensions. That means this compare
	 * function is not generally usable :(. But then again, who else than
	 * extensions department would want to use it? */
	return (l > l2 && (*start != '.' && str[l - l2 - 1] == '.')
		&& !casecmp(str + l - l2, start, l2));
}


int
is_in_list_rear(unsigned char *list, unsigned char *str, int l)
{
	return is_in_list_iterate(list, str, l, is_in_list_rear_cmp);
}


/* Guess content type of the document. */
unsigned char *
get_content_type(unsigned char *head, unsigned char *url)
{
	struct assoc *a;
	unsigned char *pos, *extension, *exxt;
	int ext_len, el, url_len;

	/* If there's one in header, it's simple.. */

	if (head) {
	       	char *ctype = parse_http_header(head, "Content-Type", NULL);

		if (ctype) {
			unsigned char *s;
			int slen;

			s = strchr(ctype, ';');
			if (s) *s = '\0';

			slen = strlen(ctype);
			while (slen && ctype[slen - 1] <= ' ') {
				ctype[--slen] = '\0';
			}

			return ctype;
		}
	}

	/* Get extension */

	extension = NULL;
	ext_len = 0;

	for (pos = url; *pos && !end_of_dir(*pos); pos++) {
		if (*pos == '.') {
			extension = pos + 1;
		} else if (dir_sep(*pos)) {
			extension = NULL;
		}
	}

	if (extension) {
		while (extension[ext_len]
		       && !dir_sep(extension[ext_len])
		       && !end_of_dir(extension[ext_len])) {
			ext_len++;
		}
	}

	/* We can't use the extension string we got just now, because we want
	 * to support also things like "ps.gz" - that'd never work, as we would
	 * always compare only to "gz". */

	/* Guess type accordingly to the extension */

	url_len = strlen(url);

	if ((!casecmp(url + url_len - 4, ".htm", 3)) ||
	    (!casecmp(url + url_len - 5, ".html", 4)))
		return stracpy("text/html");

	if (extension) {
		struct option *opt = get_real_opt("mime.extension", extension);

		if (opt) return opt->ptr;
	}

	/* Try to make application/x-extension from it */

	exxt = init_str();
	el = 0;
	add_to_str(&exxt, &el, "application/x-");
	if (extension) add_bytes_to_str(&exxt, &el, extension, ext_len);

	foreach(a, assoc) {
		if (is_in_list(a->ct, exxt, el))
			return exxt;
	}

	mem_free(exxt);

	/* Fallback.. use some hardwired default */

	return stracpy(get_opt_str("document.download.default_mime_type"));
}


struct assoc *
get_type_assoc(struct terminal *term, unsigned char *type)
{
	struct assoc *a;

	foreach(a, assoc)
		if (a->system == SYSTEM_ID
		    && (term->environment & ENV_XWIN ? a->xwin : a->cons)
		    && is_in_list(a->ct, type, strlen(type))) return a;

	return NULL;
}


void
free_types()
{
	struct assoc *a;
	struct protocol_program *p;

	foreach(a, assoc) {
		mem_free(a->ct);
		mem_free(a->prog);
		mem_free(a->label);
	}
	free_list(assoc);

	foreach(p, mailto_prog)
		mem_free(p->prog);
	free_list(mailto_prog);

	foreach(p, telnet_prog)
		mem_free(p->prog);
	free_list(telnet_prog);

	foreach(p, tn3270_prog)
		mem_free(p->prog);
	free_list(tn3270_prog);
}


unsigned char *ct_msg[] = {
	TEXT(T_LABEL),
	TEXT(T_CONTENT_TYPES),
	TEXT(T_PROGRAM__IS_REPLACED_WITH_FILE_NAME),
#ifdef ASSOC_BLOCK
	TEXT(T_BLOCK_TERMINAL_WHILE_PROGRAM_RUNNING),
#endif
#ifdef ASSOC_CONS_XWIN
	TEXT(T_RUN_ON_TERMINAL),
	TEXT(T_RUN_IN_XWINDOW),
#endif
	TEXT(T_ASK_BEFORE_OPENING),
};


void
add_ct_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = -1;
	int p = 1;

#ifdef ASSOC_BLOCK
	p++;
#endif
#ifdef ASSOC_CONS_XWIN
	p += 2;
#endif

	max_text_width(term, ct_msg[0], &max);
	min_text_width(term, ct_msg[0], &min);

	max_text_width(term, ct_msg[1], &max);
	min_text_width(term, ct_msg[1], &min);

	max_text_width(term, ct_msg[2], &max);
	min_text_width(term, ct_msg[2], &min);

	max_group_width(term, ct_msg + 3, dlg->items + 3, p, &max);
	min_group_width(term, ct_msg + 3, dlg->items + 3, p, &min);

	max_buttons_width(term, dlg->items + 3 + p, 2, &max);
	min_buttons_width(term, dlg->items + 3 + p, 2, &min);

	w = term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max) w = max;
	if (w < min) w = min;
	if (w > term->x - 2 * DIALOG_LB) w = term->x - 2 * DIALOG_LB;
	if (w < 1) w = 1;

	rw = 0;
	dlg_format_text(NULL, term,
		       	_(ct_msg[0], term),
			0, &y, w, &rw,
			COLOR_DIALOG_TEXT, AL_LEFT);

	y += 2;
	dlg_format_text(NULL, term,
			_(ct_msg[1], term),
			0, &y, w, &rw,
			COLOR_DIALOG_TEXT, AL_LEFT);

	y += 2;
	dlg_format_text(NULL, term,
			_(ct_msg[2], term),
			0, &y, w, &rw,
			COLOR_DIALOG_TEXT, AL_LEFT);

	y += 2;
	dlg_format_group(NULL, term,
			 ct_msg + 3, dlg->items + 3, p,
			 0, &y, w, &rw);

	y++;
	dlg_format_buttons(NULL, term,
			   dlg->items + 3 + p, 2,
			   0, &y, w, &rw,
			   AL_CENTER);

	w = rw;
	dlg->xw = w + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;

	center_dlg(dlg);
	draw_dlg(dlg);

	y = dlg->y + DIALOG_TB;
	dlg_format_text(term, term,
			ct_msg[0],
			dlg->x + DIALOG_LB, &y, w, NULL,
			COLOR_DIALOG_TEXT, AL_LEFT);
	dlg_format_field(term, term,
			 &dlg->items[0],
			 dlg->x + DIALOG_LB, &y, w, NULL,
			 AL_LEFT);

	y++;
	dlg_format_text(term, term,
			ct_msg[1],
			dlg->x + DIALOG_LB, &y, w, NULL,
			COLOR_DIALOG_TEXT, AL_LEFT);
	dlg_format_field(term, term,
			 &dlg->items[1],
			 dlg->x + DIALOG_LB, &y, w, NULL,
			 AL_LEFT);

	y++;
	dlg_format_text(term, term,
			ct_msg[2],
			dlg->x + DIALOG_LB, &y, w, NULL,
			COLOR_DIALOG_TEXT, AL_LEFT);
	dlg_format_field(term, term,
			 &dlg->items[2],
			 dlg->x + DIALOG_LB, &y, w, NULL,
			 AL_LEFT);

	y++;
	dlg_format_group(term, term,
			 ct_msg + 3, &dlg->items[3], p,
			 dlg->x + DIALOG_LB, &y, w, NULL);

	y++;
	dlg_format_buttons(term, term,
			   &dlg->items[3 + p], 2,
			   dlg->x + DIALOG_LB, &y, w, NULL,
			   AL_CENTER);
}


void
update_assoc(struct assoc *new)
{
	struct assoc *repl;

	if (!new->label[0] || !new->ct[0] || !new->prog[0]) return;

	if (new->cnt) {
		foreach(repl, assoc) {
			if (repl->cnt == new->cnt) {
				mem_free(repl->label);
				mem_free(repl->ct);
				mem_free(repl->prog);
				goto replace;
			}
		}

		return;
	}

	new->cnt = get_assoc_cnt();

	repl = mem_alloc(sizeof(struct assoc));
	if (!repl) return;
	add_to_list(assoc, repl);

replace:
	repl->label = stracpy(new->label);
	repl->ct = stracpy(new->ct);
	repl->prog = stracpy(new->prog);
	repl->block = new->block;
	repl->cons = new->cons;
	repl->xwin = new->xwin;
	repl->ask = new->ask;
	repl->system = new->system;
	repl->cnt = new->cnt;
}


void
really_del_ct(void *fcp)
{
	int fc = (int)fcp;
	struct assoc *del;

	foreach(del, assoc)
		if (del->cnt == fc)
			goto ok;

	return;

ok:
	delete_association(del);
}


void
menu_del_ct(struct terminal *term, void *fcp, void *xxx2)
{
	unsigned char *str;
	int strl;
	int fc = (int)fcp;
	struct assoc *del;

	foreach(del, assoc)
		if (del->cnt == fc)
			goto ok;

	return;

ok:
	str = init_str();
	if (!str) return;
	strl = 0;
	add_to_str(&str, &strl, del->ct);
	add_to_str(&str, &strl, " -> ");
	add_to_str(&str, &strl, del->prog);

	msg_box(term, getml(str, NULL),
		TEXT(T_DELETE_ASSOCIATION), AL_CENTER | AL_EXTD_TEXT,
		TEXT(T_DELETE_ASSOCIATION), ": ", str, "?", NULL,
		fcp, 2,
		TEXT(T_YES), really_del_ct, B_ENTER,
		TEXT(T_NO), NULL, B_ESC);
}


void
menu_add_ct(struct terminal *term, void *fcp, void *xxx2)
{
	int p;
	int fc = (int)fcp;
	struct assoc *new, *from;
	unsigned char *label;
	unsigned char *ct;
	unsigned char *prog;
	struct dialog *d;

	if (fc) {
		foreach(from, assoc)
			if (from->cnt == fc)
				goto ok;

		return;
	}
	from = NULL;

ok:
#define DIALOG_MEMSIZE sizeof(struct dialog) + 10 * sizeof(struct dialog_item) \
		       + sizeof(struct assoc) + 3 * MAX_STR_LEN

	d = mem_alloc(DIALOG_MEMSIZE);
	if (!d) return;
	memset(d, 0, DIALOG_MEMSIZE);

#undef DIALOG_MEMSIZE

	new = (struct assoc *)&d->items[10];
	new->label = label = (unsigned char *)(new + 1);
	new->ct = ct = label + MAX_STR_LEN;
	new->prog = prog = ct + MAX_STR_LEN;

	if (from) {
		safe_strncpy(label, from->label, MAX_STR_LEN - 1);
		safe_strncpy(ct, from->ct, MAX_STR_LEN - 1);
		safe_strncpy(prog, from->prog, MAX_STR_LEN - 1);
		new->block = from->block;
		new->cons = from->cons;
		new->xwin = from->xwin;
		new->ask = from->ask;
		new->system = from->system;
		new->cnt = from->cnt;
	} else {
		new->block = new->xwin = new->cons = 1;
		new->ask = 1;
		new->system = SYSTEM_ID;
	}

	d->title = TEXT(T_ASSOCIATION);
	d->fn = add_ct_fn;
	d->refresh = (void (*)(void *))update_assoc;
	d->refresh_data = new;

	d->items[0].type = D_FIELD;
	d->items[0].dlen = MAX_STR_LEN;
	d->items[0].data = label;
	d->items[0].fn = check_nonempty;

	d->items[1].type = D_FIELD;
	d->items[1].dlen = MAX_STR_LEN;
	d->items[1].data = ct;
	d->items[1].fn = check_nonempty;

	d->items[2].type = D_FIELD;
	d->items[2].dlen = MAX_STR_LEN;
	d->items[2].data = prog;
	d->items[2].fn = check_nonempty;

	p = 3;

#ifdef ASSOC_BLOCK
	d->items[p].type = D_CHECKBOX;
	d->items[p].data = (unsigned char *)&new->block;
	d->items[p++].dlen = sizeof(int);
#endif

#ifdef ASSOC_CONS_XWIN
	d->items[p].type = D_CHECKBOX;
	d->items[p].data = (unsigned char *)&new->cons;
	d->items[p++].dlen = sizeof(int);

	d->items[p].type = D_CHECKBOX;
	d->items[p].data = (unsigned char *)&new->xwin;
	d->items[p++].dlen = sizeof(int);
#endif

	d->items[p].type = D_CHECKBOX;
	d->items[p].data = (unsigned char *)&new->ask;
	d->items[p++].dlen = sizeof(int);

	d->items[p].type = D_BUTTON;
	d->items[p].gid = B_ENTER;
	d->items[p].fn = ok_dialog;
	d->items[p++].text = TEXT(T_OK);

	d->items[p].type = D_BUTTON;
	d->items[p].gid = B_ESC;
	d->items[p].text = TEXT(T_CANCEL);
	d->items[p++].fn = cancel_dialog;
	d->items[p++].type = D_END;

	do_dialog(term, d, getml(d, NULL));
}

struct menu_item mi_no_assoc[] = {
	{TEXT(T_NO_ASSOCIATIONS), "", M_BAR, NULL, NULL, 0, 0},
	{NULL, NULL, 0, NULL, NULL, 0, 0}
};


void
menu_list_assoc(struct terminal *term, void *fn, void *xxx)
{
	struct assoc *a;
	struct menu_item *mi = NULL;
	int n = 0;

	foreachback(a, assoc) {
		if (a->system == SYSTEM_ID) {
			if (!mi) {
				mi = new_menu(7);
			       	if (!mi) return;
			}

			add_to_menu(&mi, stracpy(a->label), stracpy(a->ct),
				    "", MENU_FUNC fn, (void *)a->cnt, 0);
			n++;
		}
	}

	if (!mi)
		do_menu(term, mi_no_assoc, xxx);
	else
		do_menu(term, mi, xxx);
}


unsigned char *ext_msg[] = {
	TEXT(T_EXTENSION_S),
	TEXT(T_CONTENT_TYPE),
};


void
add_ext_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = -1;

	max_text_width(term, ext_msg[0], &max);
	min_text_width(term, ext_msg[0], &min);
	max_text_width(term, ext_msg[1], &max);
	min_text_width(term, ext_msg[1], &min);
	max_buttons_width(term, dlg->items + 2, 2, &max);
	min_buttons_width(term, dlg->items + 2, 2, &min);

	w = term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max) w = max;
	if (w < min) w = min;
	if (w > term->x - 2 * DIALOG_LB) w = term->x - 2 * DIALOG_LB;
	if (w < 1) w = 1;

	rw = 0;
	dlg_format_text(NULL, term,
			ext_msg[0],
			0, &y, w, &rw,
			COLOR_DIALOG_TEXT, AL_LEFT);

	y += 2;
	dlg_format_text(NULL, term,
			ext_msg[1],
			0, &y, w, &rw,
			COLOR_DIALOG_TEXT, AL_LEFT);

	y += 2;
	dlg_format_buttons(NULL, term,
			   dlg->items + 2, 2,
			   0, &y, w, &rw,
			   AL_CENTER);

	w = rw;
	dlg->xw = w + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;

	center_dlg(dlg);
	draw_dlg(dlg);

	y = dlg->y + DIALOG_TB;
	dlg_format_text(term, term,
			ext_msg[0],
			dlg->x + DIALOG_LB, &y, w, NULL,
			COLOR_DIALOG_TEXT, AL_LEFT);
	dlg_format_field(term, term,
			 &dlg->items[0],
			 dlg->x + DIALOG_LB, &y, w, NULL,
			 AL_LEFT);

	y++;
	dlg_format_text(term, term,
			ext_msg[1],
			dlg->x + DIALOG_LB, &y, w, NULL,
			COLOR_DIALOG_TEXT, AL_LEFT);
	dlg_format_field(term, term,
			 &dlg->items[1],
			 dlg->x + DIALOG_LB, &y, w, NULL,
			 AL_LEFT);

	y++;
	dlg_format_buttons(term, term,
			   &dlg->items[2], 2,
			   dlg->x + DIALOG_LB, &y, w, NULL,
			   AL_CENTER);
}


void
really_del_ext(void *fcp)
{
	struct option *opt;
	
	opt = get_real_opt("mime.extension", (unsigned char *) fcp);
	if (opt) delete_option(opt);
}


void
menu_del_ext(struct terminal *term, void *fcp, void *xxx2)
{
	struct option *opt;
	unsigned char *str;
	int strl;
	
	opt = get_real_opt("mime.extension", (unsigned char *) fcp);
	if (!opt) return;

	str = init_str();
	if (!str) return;
	strl = 0;
	add_to_str(&str, &strl, (unsigned char *) fcp);
	add_to_str(&str, &strl, " -> ");
	add_to_str(&str, &strl, (unsigned char *) opt->ptr);

	msg_box(term, getml(str, NULL),
		TEXT(T_DELETE_EXTENSION), AL_CENTER | AL_EXTD_TEXT,
		TEXT(T_DELETE_EXTENSION), " ", str, "?", NULL,
		fcp, 2,
		TEXT(T_YES), really_del_ext, B_ENTER,
		TEXT(T_NO), NULL, B_ESC);
}


struct extension {
	unsigned char *ext_orig;
	unsigned char *ext;
	unsigned char *ct;
};

void
really_add_ext(void *fcp)
{
	struct extension *ext = (struct extension *) fcp;
	unsigned char *name = straconcat("mime.extension", ".", ext->ext, NULL);

	if (!name) return;

	really_del_ext(ext->ext_orig); /* ..or rename ;) */
	safe_strncpy(get_opt_str(name), ext->ct, MAX_STR_LEN);
	mem_free(name);
}

void
menu_add_ext(struct terminal *term, void *fcp, void *xxx2)
{
	struct option *opt = NULL;
	struct extension *new;
	unsigned char *ext;
	unsigned char *ct;
	unsigned char *ext_orig;
	struct dialog *d;

	if (fcp) opt = get_real_opt("mime.extension", (unsigned char *) fcp);

#define DIALOG_MEMSIZE sizeof(struct dialog) + 5 * sizeof(struct dialog_item) \
		       + sizeof(struct extension) + 3 * MAX_STR_LEN

	d = mem_alloc(DIALOG_MEMSIZE);
	if (!d) return;
	memset(d, 0, DIALOG_MEMSIZE);

#undef DIALOG_MEMSIZE

	new = (struct extension *) &d->items[5];
	new->ext = ext = (unsigned char *) (new + 1);
	new->ct = ct = ext + MAX_STR_LEN;
	new->ext_orig = ext_orig = ct + MAX_STR_LEN;

	if (opt) {
		safe_strncpy(ext, (unsigned char *) fcp, MAX_STR_LEN);
		safe_strncpy(ct, (unsigned char *) opt->ptr, MAX_STR_LEN);
		safe_strncpy(ext_orig, (unsigned char *) fcp, MAX_STR_LEN);
	}

	d->title = TEXT(T_EXTENSION);
	d->fn = add_ext_fn;
	d->refresh = (void (*)(void *)) really_add_ext;
	d->refresh_data = new;

	d->items[0].type = D_FIELD;
	d->items[0].dlen = MAX_STR_LEN;
	d->items[0].data = ext;
	d->items[0].fn = check_nonempty;

	d->items[1].type = D_FIELD;
	d->items[1].dlen = MAX_STR_LEN;
	d->items[1].data = ct;
	d->items[1].fn = check_nonempty;

	d->items[2].type = D_BUTTON;
	d->items[2].gid = B_ENTER;
	d->items[2].fn = ok_dialog;
	d->items[2].text = TEXT(T_OK);

	d->items[3].type = D_BUTTON;
	d->items[3].gid = B_ESC;
	d->items[3].text = TEXT(T_CANCEL);
	d->items[3].fn = cancel_dialog;

	d->items[4].type = D_END;

	do_dialog(term, d, getml(d, NULL));
}


struct menu_item mi_no_ext[] = {
	{TEXT(T_NO_EXTENSIONS), "", M_BAR, NULL, NULL, 0, 0},
	{NULL, NULL, 0, NULL, NULL, 0, 0}
};

void
menu_list_ext(struct terminal *term, void *fn, void *xxx)
{
	struct list_head *opt_tree;
	struct option *opt;
	struct menu_item *mi = NULL;
	int n = 0;
	
	opt_tree = (struct list_head *) get_opt_ptr("mime.extension");

	foreachback (opt, *opt_tree) {
		if (!strcmp(opt->name, "_template_")) continue;
		if (!mi) {
			mi = new_menu(7);
		       	if (!mi) return;
		}
		add_to_menu(&mi, stracpy(opt->name),
			    stracpy((unsigned char *) opt->ptr),
			    "", MENU_FUNC fn, opt->name, 0);
		n++;
	}

	if (!mi)
		do_menu(term, mi_no_ext, xxx);
	else
		do_menu(term, mi, xxx);
}


void
update_prog(struct list_head *l, unsigned char *p, int s)
{
	struct protocol_program *repl;

	foreach(repl, *l) {
		if (repl->system == s) {
			mem_free(repl->prog);
			goto replace;
		}
	}

	repl = mem_alloc(sizeof(struct protocol_program));
	if (!repl) return;
	add_to_list(*l, repl);
	repl->system = s;

replace:
	repl->prog = mem_alloc(MAX_STR_LEN);
	if (repl->prog) {
		safe_strncpy(repl->prog, p, MAX_STR_LEN);
	}
}


unsigned char *
get_prog(struct list_head *l)
{
	struct protocol_program *repl;

	foreach(repl, *l)
		if (repl->system == SYSTEM_ID)
			return repl->prog;

	update_prog(l, "", SYSTEM_ID);

	foreach(repl, *l)
		if (repl->system == SYSTEM_ID)
			return repl->prog;

	return NULL;
}
