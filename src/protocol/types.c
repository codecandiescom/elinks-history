/* Internal MIME types implementation */
/* $Id: types.c,v 1.36 2002/07/04 01:07:14 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "links.h"

#include "bfu/align.h"
#include "bfu/bfu.h"
#include "bfu/menu.h"
#include "bfu/msgbox.h"
#include "config/options.h"
#include "intl/language.h"
#include "lowlevel/terminal.h"
#include "protocol/http/header.h"
#include "protocol/types.h"
#include "protocol/url.h"
#include "util/memory.h"
#include "util/string.h"


struct option *
get_real_opt(unsigned char *base, unsigned char *id)
{
	struct option *opt;
	unsigned char *name = straconcat(base, ".", id, NULL);

	if (!name) return NULL;

	opt = get_opt_rec_real(root_options, name);

	mem_free(name);
	return opt;
}




/* Guess content type of the document. */
unsigned char *
get_content_type(unsigned char *head, unsigned char *url)
{
	unsigned char *pos, *extension;
	int ext_len, url_len;

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
		struct option *opt_tree = get_opt_rec_real(root_options,
							   "mime.extension");
		struct option *opt;

		foreach (opt, *((struct list_head *) opt_tree->ptr)) {
			/* strrcmp */
			int i, j;

			/* Match the longest possible part of URL.. */

			for (i = strlen(url) - 1, j = strlen(opt->name) - 1;
			     i >= 0 && j >= 0
			     && url[i] == (opt->name[j] == '-' ? '.'
				     			       : opt->name[j]);
			     i--, j--)
				/* */ ;

			/* If we matched whole extension and it is really an
			 * extension.. */
			if (j < 0 && i >= 0 && url[i] == '.') {
				return stracpy(opt->ptr);
			}
		}
	}

	/* Try to make application/x-extension from it */

	{
		unsigned char *ext_type = init_str();
		int el = 0;

		add_to_str(&ext_type, &el, "application/x-");
		if (extension)
			add_bytes_to_str(&ext_type, &el, extension, ext_len);

		if (get_mime_type_handler(NULL, ext_type))
			return ext_type;

		mem_free(ext_type);
	}

	/* Fallback.. use some hardwired default */
	/* TODO: Make this rather mime.extension._template_ ..? --pasky */

	return stracpy(get_opt_str("document.download.default_mime_type"));
}




unsigned char *
get_mime_type_name(unsigned char *type)
{
	unsigned char *class, *id;
	unsigned char *name;

	class = stracpy(type);
	if (!class) { return NULL; }

	id = strchr(class, '/');
	if (!id) { mem_free(class); return NULL; }

	*(id++) = '\0';

	name = straconcat("mime.type", ".", class, ".", id, NULL);
	mem_free(class);

	return name;
}

unsigned char *
get_mime_handler_name(unsigned char *type, int xwin)
{
	struct option *opt;
	unsigned char *name = get_mime_type_name(type);
	unsigned char *system_str;

	if (!name) return NULL;

	opt = get_opt_rec_real(root_options, name);
	mem_free(name);
	if (!opt) { return NULL; }

	system_str = get_system_str(xwin);
	if (!system_str) { return NULL; }

	name = straconcat("mime.handler", ".", (unsigned char *) opt->ptr,
			  ".", system_str, NULL);
	mem_free(system_str);

	return name;
}

/* Return tree containing options specific to this type. */
struct option *
get_mime_type_handler(struct terminal *term, unsigned char *type)
{
	struct option *opt_tree;
	unsigned char *name;
	int xwin = term ? term->environment & ENV_XWIN : 0;

	name = get_mime_handler_name(type, xwin);
	if (!name) return NULL;

	opt_tree = get_opt_rec_real(root_options, name);

	mem_free(name);

	return opt_tree;
}




#if 0

/* This stuff needs to be redesigned, reworked, rewritten. Separate menu for
 * MIME types and MIME associations. */

unsigned char *ct_msg[] = {
	TEXT(T_LABEL),
	TEXT(T_CONTENT_TYPES),
	TEXT(T_PROGRAM__IS_REPLACED_WITH_FILE_NAME),
#ifdef ASSOC_BLOCK
	TEXT(T_BLOCK_TERMINAL_WHILE_PROGRAM_RUNNING),
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
really_del_ct(void *fcp)
{
	struct option *opt;

	if (!fcp) return;
	opt = get_opt_rec_real(root_options, (unsigned char *) fcp);
	if (opt) delete_option(opt);
	mem_free(fcp);
}


void
menu_del_ct(struct terminal *term, void *fcp, void *xxx2)
{
	struct option *del;
	unsigned char *ct = (unsigned char *) fcp;
	unsigned char *str;
	int strl;

	del = get_type_assoc(term, ct);
	if (!del) return;

	str = init_str();
	if (!str) return;
	strl = 0;

	add_to_str(&str, &strl, ct);
	add_to_str(&str, &strl, " -> ");
	add_to_str(&str, &strl, get_opt_str_tree(del->ptr, "program"));

	msg_box(term, getml(str, NULL),
		TEXT(T_DELETE_ASSOCIATION), AL_CENTER | AL_EXTD_TEXT,
		TEXT(T_DELETE_ASSOCIATION), ": ", str, "?", NULL,
		get_type_assoc_name(ct, term->environment & ENV_XWIN), 2,
		TEXT(T_YES), really_del_ct, B_ENTER,
		TEXT(T_NO), NULL, B_ESC);
}



struct assoc {
	unsigned char *name;
	unsigned char *prog;
	int block;
	int ask;
};

void
really_add_ct(void *fcp)
{
	struct assoc *assoc = (struct assoc *) fcp;
	struct option *add = get_opt_rec_real(assoc->name);

	really_del_ct(assoc->name); /* ..or rename ;) */

	safe_strncpy(get_opt_str_tree(add, "program"), assoc->prog,
		     MAX_STR_LEN);
	get_opt_int_tree(add, "block") = assoc->block;
	get_opt_int_tree(add, "ask") = assoc->ask;

	mem_free(assoc->prog);
	mem_free(assoc->name);
	mem_free(assoc);
}

void
menu_add_ct(struct terminal *term, void *fcp, void *xxx2)
{
	int p;
	unsigned char *assoc_name = "";
	struct option *opt = NULL;
	struct assoc *new;
	unsigned char *ct;
	unsigned char *prog;
	struct dialog *d;

	if (fcp) {
		assoc_name = get_type_assoc_name((unsigned char *) fcp,
						 term->environment & ENV_XWIN);

		opt = get_opt_real_
	}

#define DIALOG_MEMSIZE sizeof(struct dialog) + 10 * sizeof(struct dialog_item) \
		       + sizeof(struct assoc) + 2 * MAX_STR_LEN

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
	d->items[p].data = (unsigned char *) &new->block;
	d->items[p++].dlen = sizeof(int);
#endif

	d->items[p].type = D_CHECKBOX;
	d->items[p].data = (unsigned char *) &new->ask;
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
	struct list_head *class_tree;
	struct option *class_opt;
	struct menu_item *mi = NULL;
	
	class_tree = (struct list_head *) get_opt_ptr("mime.association");
	foreachback (class_opt, *class_tree) {
		struct list_head *id_tree;
		struct option *id_opt;

		if (!strcmp(class_opt->name, "_template_")) continue;
		
		id_tree = (struct list_head *) class_opt->ptr;

		foreachback (id_opt, *id_tree) {
			unsigned char *system_str;
			unsigned char *ct;

			if (!strcmp(id_opt->name, "_template_")) continue;

			system_str = get_system_str(term->environment
						    & ENV_XWIN);
			if (!system_str) continue;
			id_opt = get_opt_rec_real((struct list_head *)
						  id_opt->ptr,
						  system_str);
			mem_free(system_str);
			if (!id_opt) continue;

			ct = straconcat(class_opt->name, "/", id_opt->name,
					NULL);
			if (!ct) continue;

			if (!mi) {
				mi = new_menu(7);
			       	if (!mi) return;
			}

			add_to_menu(&mi, ct, (unsigned char *) id_opt->ptr,
				    "", MENU_FUNC fn, (void *) ct, 0);
		}
	}

	if (!mi)
		do_menu(term, mi_no_assoc, xxx);
	else
		do_menu(term, mi, xxx);
}

#endif



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
free_translated(void *fcp)
{
	mem_free(fcp);
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
	unsigned char *translated = stracpy((unsigned char *) fcp);
	struct option *opt;
	unsigned char *str;
	int strl;

	if (translated) {
		int i;
		
		for (i = strlen(translated) - 1; i >= 0; i--)
			if (translated[i] == '.')
				translated[i] = '-';
	} else {
		mem_free(fcp);
		return;
	}

	opt = get_real_opt("mime.extension", translated);
	if (!opt) { mem_free(fcp); return; }

	str = init_str();
	if (!str) { mem_free(fcp); return; }
	strl = 0;
	add_to_str(&str, &strl, (unsigned char *) fcp);
	add_to_str(&str, &strl, " -> ");
	add_to_str(&str, &strl, (unsigned char *) opt->ptr);

	msg_box(term, getml(str, NULL),
		TEXT(T_DELETE_EXTENSION), AL_CENTER | AL_EXTD_TEXT,
		TEXT(T_DELETE_EXTENSION), " ", str, "?", NULL,
		translated, 2,
		TEXT(T_YES), really_del_ext, B_ENTER,
		TEXT(T_NO), free_translated, B_ESC);

	mem_free(fcp);
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
	unsigned char *translated = stracpy(ext->ext);
	unsigned char *name;

	if (translated) {
		int i;

		for (i = strlen(translated) - 1; i >= 0; i--)
			if (translated[i] == '.')
				translated[i] = '-';
	} else return;

	name = straconcat("mime.extension", ".", translated, NULL);
	if (!name) return;
	mem_free(translated);

	really_del_ext(ext->ext_orig); /* ..or rename ;) */
	safe_strncpy(get_opt_str(name), ext->ct, MAX_STR_LEN);
	mem_free(name);
}

void
menu_add_ext(struct terminal *term, void *fcp, void *xxx2)
{
	unsigned char *translated = stracpy((unsigned char *) fcp);
	struct option *opt = NULL;
	struct extension *new;
	unsigned char *ext;
	unsigned char *ct;
	unsigned char *ext_orig;
	struct dialog *d;

	if (translated) {
		int i;

		for (i = strlen(translated) - 1; i >= 0; i--)
			if (translated[i] == '.')
				translated[i] = '-';
	}

	if (translated) opt = get_real_opt("mime.extension", translated);

#define DIALOG_MEMSIZE sizeof(struct dialog) + 5 * sizeof(struct dialog_item) \
		       + sizeof(struct extension) + 3 * MAX_STR_LEN

	d = mem_alloc(DIALOG_MEMSIZE);
	if (!d) { mem_free(fcp); return; }
	memset(d, 0, DIALOG_MEMSIZE);

#undef DIALOG_MEMSIZE

	new = (struct extension *) &d->items[5];
	new->ext = ext = (unsigned char *) (new + 1);
	new->ct = ct = ext + MAX_STR_LEN;
	new->ext_orig = ext_orig = ct + MAX_STR_LEN;

	if (opt) {
		safe_strncpy(ext, (unsigned char *) fcp, MAX_STR_LEN);
		safe_strncpy(ct, (unsigned char *) opt->ptr, MAX_STR_LEN);
		safe_strncpy(ext_orig, translated ? translated
						  : (unsigned char *) "",
			     MAX_STR_LEN);
	}

	if (translated) mem_free(translated);

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

	if (fcp) mem_free(fcp);
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
	
	opt_tree = (struct list_head *) get_opt_ptr("mime.extension");

	foreachback (opt, *opt_tree) {
		unsigned char *translated;

		if (!strcmp(opt->name, "_template_")) continue;

		translated = stracpy(opt->name);
		if (translated) {
			int i;

			for (i = strlen(translated) - 1; i >= 0; i--)
				if (translated[i] == '-')
					translated[i] = '.';
		} else continue;

		if (!mi) {
			mi = new_menu(15);
		       	if (!mi) { mem_free(translated); return; }
		}
		add_to_menu(&mi, translated,
			    stracpy((unsigned char *) opt->ptr),
			    "", MENU_FUNC fn, stracpy(translated), 0);
	}

	if (!mi)
		do_menu(term, mi_no_ext, xxx);
	else
		do_menu(term, mi, xxx);
}




unsigned char *
get_prog(struct terminal *term, unsigned char *progid)
{
	struct option *opt;
	unsigned char *system_str = get_system_str(term->environment & ENV_XWIN);
	unsigned char *name;

	if (!system_str) return NULL;
	name = straconcat("protocol.user", ".", progid, ".",
			  system_str, NULL);
	mem_free(system_str);
	if (!name) return NULL;

	opt = get_opt_rec_real(root_options, name);

	mem_free(name);
	return (unsigned char *) (opt ? opt->ptr : NULL);
}
