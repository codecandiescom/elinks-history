/* Option variables types handlers */
/* $Id: opttypes.c,v 1.17 2002/06/17 11:23:45 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "links.h"

#include "config/kbdbind.h"
#include "config/options.h"
#include "config/opttypes.h"
#include "document/html/colors.h"
#include "intl/charsets.h"
#include "intl/language.h"
#include "protocol/types.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


/* Commandline handlers. */

unsigned char *
gen_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	unsigned char *r;
	if (!*argc) return "Parameter expected";
	(*argv)++; (*argc)--;
	/* FIXME!! We will modify argv! */
	if (!option_types[o->type].read(o, *argv - 1)) r = "Read error"; else return NULL;
	(*argv)--; (*argc)++;
	return r;
}

/* If 0 follows, disable option and eat 0. If 1 follows, enable option and
 * eat 1. If anything else follow, enable option and don't eat anything. */
unsigned char *
bool_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	*((int *) o->ptr) = 1;

	if (!*argc) return NULL;

	/* Argument is empty or longer than 1 char.. */
	if (!(*argv)[0][0] || (*argv)[0][1]) return NULL;

	switch ((*argv)[0][0]) {
		case '0': *((int *) o->ptr) = 0; break;
		case '1': *((int *) o->ptr) = 1; break;
		default: return NULL;
	}

	/* We ate parameter */
	(*argv)++; (*argc)--;
	return NULL;
}

unsigned char *
exec_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	return ((unsigned char *(*)(struct option *, unsigned char ***, int *)) o->ptr)(o, argv, argc);
}


/* Wrappers for OPT_ALIAS. */
/* Note that they can wrap only to root_options now.  I don't think it could be
 * a problem, but who knows.. however, changing that will be pretty tricky -
 * possibly changing ptr to structure containing target name and pointer to
 * options list? --pasky */

unsigned char *
redir_cmd(struct option *opt, unsigned char ***argv, int *argc)
{
	struct option *real = get_opt_rec(root_options, opt->ptr);

	if (!real) {
		internal("Alias %s leads to unknown option %s!",
			 opt->name, opt->ptr);
		return NULL;
	}

	if (option_types[real->type].cmdline)
		return option_types[real->type].cmdline(real, argv, argc);

	return NULL;
}

int
redir_rd(struct option *opt, unsigned char **file)
{
	struct option *real = get_opt_rec(root_options, opt->ptr);

	if (!real) {
		internal("Alias %s leads to unknown option %s!",
			 opt->name, opt->ptr);
		return 0;
	}

	if (option_types[real->type].read)
		return option_types[real->type].read(real, file);

	return 0;
}


/* Support functions for config file parsing. */

void
add_quoted_to_str(unsigned char **s, int *l, unsigned char *q)
{
	add_chr_to_str(s, l, '"');
	while (*q) {
		if (*q == '"' || *q == '\\') add_chr_to_str(s, l, '\\');
		add_chr_to_str(s, l, *q);
		q++;
	}
	add_chr_to_str(s, l, '"');
}


/* Config file handlers. */

int
num_rd(struct option *opt, unsigned char **file)
{
	long value;

	value = strtolx(*file, file);

	if (!WHITECHAR(**file) && **file != '#') return 0;

	if (value < opt->min || value > opt->max) return 0;

	*((int *) opt->ptr) = value;

	return 1;
}

void
num_wr(struct option *o, unsigned char **s, int *l)
{
	add_knum_to_str(s, l, *((int *) o->ptr));
}

void *
int_dup(struct option *opt)
{
	int *new = mem_alloc(sizeof(int));

	memcpy(new, opt->ptr, sizeof(int));
	return new;
}

void *
long_dup(struct option *opt)
{
	long *new = mem_alloc(sizeof(long));

	memcpy(new, opt->ptr, sizeof(long));
	return new;
}


int
str_rd(struct option *opt, unsigned char **file)
{
	unsigned char *str = *file;
	unsigned char *str2 = init_str();
	int str2l = 0;

	if (*str != '"') return 0;
	str++;

	while (*str && *str != '"') {
		if (*str == '\\') {
			/* FIXME: This won't work on crlf systems. */
			if (str[1] == '\n') { str[1] = ' '; str++; }
			/* When there's '"', we will just move on there, thus
			 * we will never test for it in while() condition and
			 * we will treat it just as '"', ignoring the backslash
			 * itself. */
			if (str[1] == '"') str++;
			/* \\ means \. */
			if (str[1] == '\\') str++;
		}
		add_chr_to_str(&str2, &str2l, *str);
		str++;
	}

	if (!*str) { mem_free(str2); *file = str; return 0; }

	str++; /* Skip the quote. */
	*file = str;

	if (opt->max && str2l >= opt->max) { mem_free(str2); return 0; }

	safe_strncpy(opt->ptr, str2, MAX_STR_LEN);
	mem_free(str2);

	return 1;
}

void
str_wr(struct option *o, unsigned char **s, int *l)
{
	if (strlen(o->ptr) >= o->max) {
		unsigned char *s1 = init_str();
		int l1 = 0;

		add_bytes_to_str(&s1, &l1, o->ptr, o->max - 1);
		add_quoted_to_str(s, l, s1);
		mem_free(s1);
	} else {
		add_quoted_to_str(s, l, o->ptr);
	}
}

void *
str_dup(struct option *opt)
{
	unsigned char *new = mem_alloc(MAX_STR_LEN);

	safe_strncpy(new, opt->ptr, MAX_STR_LEN);
	return new;
}


int
cp_rd(struct option *opt, unsigned char **str)
{
	unsigned char buf[MAX_STR_LEN];
	void *ptr;
	int ret;

	/* XXX: We run string parser on this, simulating that this is a string
	 * option. */

	ptr = opt->ptr;
	opt->ptr = buf;
	ret = str_rd(opt, str);
	if (!ret) {
		opt->ptr = ptr;
		return 0;
	}

	ret = get_cp_index(opt->ptr);
	opt->ptr = ptr;

	if (ret < 0) {
		*((int *) opt->ptr) = ret;
		return 0;
	}

	*((int *) opt->ptr) = ret;

	return 1;
}

void
cp_wr(struct option *o, unsigned char **s, int *l)
{
	add_quoted_to_str(s, l, get_cp_mime_name(*((int *) o->ptr)));
}


int
lang_rd(struct option *opt, unsigned char **str)
{
	unsigned char buf[MAX_STR_LEN];
	void *ptr;
	int ret;

	/* XXX: We run string parser on this, simulating that this is a string
	 * option. */

	ptr = opt->ptr;
	opt->ptr = buf;
	ret = str_rd(opt, str);
	if (!ret) {
		ptr = opt->ptr;
		return 0;
	}

	for (ret = 0; ret < n_languages(); ret++)
		if (!strcasecmp(language_name(ret), opt->ptr)) {
			opt->ptr = ptr;
			*((int *) opt->ptr) = ret;
			set_language(ret);
			return 1;
		}

	opt->ptr = ptr;

	*((int *) opt->ptr) = -1;

	return 0;
}

void
lang_wr(struct option *o, unsigned char **s, int *l)
{
	add_quoted_to_str(s, l, language_name(current_language));
}


int
type_rd(struct option *o, unsigned char **c)
{
#if 0
	unsigned char *err = "Error reading association specification";
	struct assoc new;
	unsigned char *w;
	int n;
	memset(&new, 0, sizeof(struct assoc));
	if (!(new.label = get_token(&c))) goto err;
	if (!(new.ct = get_token(&c))) goto err;
	if (!(new.prog = get_token(&c))) goto err;
	if (!(w = get_token(&c))) goto err;
	if (getnum(w, &n, 0, 32)) goto err_f;
	mem_free(w);
	new.cons = !!(n & 1);
	new.xwin = !!(n & 2);
	new.ask = !!(n & 4);
	if ((n & 8) || (n & 16)) new.block = !!(n & 16);
	else new.block = !new.xwin || new.cons;
	if (!(w = get_token(&c))) goto err;
	if (strlen(w) != 1 || w[0] < '0' || w[0] > '9') goto err_f;
	new.system = w[0] - '0';
	mem_free(w);
	update_assoc(&new);
	err = NULL;
	err:
	if (new.label) mem_free(new.label);
	if (new.ct) mem_free(new.ct);
	if (new.prog) mem_free(new.prog);
	return err;
	err_f:
	mem_free(w);
	goto err;
#endif
	return 0;
}

void
type_wr(struct option *o, unsigned char **s, int *l)
{
#if 0
	struct assoc *a;
	foreachback(a, assoc) {
		add_quoted_to_str(s, l, a->label);
		add_to_str(s, l, " ");
		add_quoted_to_str(s, l, a->ct);
		add_to_str(s, l, " ");
		add_quoted_to_str(s, l, a->prog);
		add_to_str(s, l, " ");
		add_num_to_str(s, l, (!!a->cons) + (!!a->xwin) * 2 + (!!a->ask) * 4 + (!a->block) * 8 + (!!a->block) * 16);
		add_to_str(s, l, " ");
		add_num_to_str(s, l, a->system);
	}
#endif
}


int
ext_rd(struct option *o, unsigned char **c)
{
#if 0
	unsigned char *err = "Error reading extension specification";
	struct extension new;
	memset(&new, 0, sizeof(struct extension));
	if (!(new.ext = get_token(&c))) goto err;
	if (!(new.ct = get_token(&c))) goto err;
	update_ext(&new);
	err = NULL;
	err:
	if (new.ext) mem_free(new.ext);
	if (new.ct) mem_free(new.ct);
	return err;
#endif
	return 0;
}

void
ext_wr(struct option *o, unsigned char **s, int *l)
{
#if 0
	struct extension *a;
	foreachback(a, extensions) {
		add_quoted_to_str(s, l, a->ext);
		add_to_str(s, l, " ");
		add_quoted_to_str(s, l, a->ct);
	}
#endif
}


int
prog_rd(struct option *o, unsigned char **c)
{
#if 0
	unsigned char *err = "Error reading program specification";
	unsigned char *prog, *w;
	if (!(prog = get_token(&c))) goto err_1;
	if (!(w = get_token(&c))) goto err_2;
	if (strlen(w) != 1 || w[0] < '0' || w[0] > '9') goto err_3;
	update_prog(o->ptr, prog, w[0] - '0');
	err = NULL;
	err_3:
	mem_free(w);
	err_2:
	mem_free(prog);
	err_1:
	return err;
#endif
	return 0;
}

void
prog_wr(struct option *o, unsigned char **s, int *l)
{
#if 0
	struct protocol_program *a;
	foreachback(a, *(struct list_head *)o->ptr) {
		if (!*a->prog) continue;
		add_quoted_to_str(s, l, a->prog);
		add_to_str(s, l, " ");
		add_num_to_str(s, l, a->system);
	}
#endif
}


int
color_rd(struct option *opt, unsigned char **str)
{
	unsigned char buf[MAX_STR_LEN];
	void *ptr;
	int ret;
	struct rgb color;

	/* XXX: We run string parser on this, simulating that this is a string
	 * option. */

	ptr = opt->ptr;
	opt->ptr = buf;
	ret = str_rd(opt, str);
	if (!ret) {
		ptr = opt->ptr;
		return 0;
	}

	ret = decode_color(opt->ptr, &color);
	opt->ptr = ptr;
	*((struct rgb *) opt->ptr) = color;

	if (ret) return 0;

	return 1;
}

void
color_wr(struct option *opt, unsigned char **str, int *len)
{
	struct rgb *color = (struct rgb *) opt->ptr;
	unsigned char strcolor[8];

	/* TODO: We should be clever boys and try to save color as name, not
	 * always as RGB. --pasky */

	snprintf(strcolor, 8, "#%02x%02x%02x", color->r, color->g, color->b);

	add_quoted_to_str(str, len, strcolor);
}

void *
rgb_dup(struct option *opt)
{
	struct rgb *new = mem_alloc(sizeof(struct rgb));

	memcpy(new, opt->ptr, sizeof(struct rgb));
	return new;
}

void *
tree_dup(struct option *opt)
{
	struct list_head *new = mem_alloc(sizeof(struct list_head));
	struct list_head *tree = (struct list_head *) opt->ptr;
	struct option *option;

	if (!new) return NULL;
	init_list(*new);

	foreachback (option, *tree) {
		struct option *new_opt = copy_option(option);

		if (new_opt) add_to_list(*new, new_opt);
	}

	return new;
}


struct option_type_info option_types[] = {
	{ bool_cmd, num_rd, num_wr, int_dup, "[0|1]" },
	{ gen_cmd, num_rd, num_wr, int_dup, "<num>" },
	{ gen_cmd, num_rd, num_wr, long_dup, "<num>" },
	{ gen_cmd, str_rd, str_wr, str_dup, "<str>" },

	{ gen_cmd, cp_rd, cp_wr, int_dup, "<codepage>" },
	{ gen_cmd, lang_rd, lang_wr, NULL, "<language>" },
	{ NULL, prog_rd, NULL /*prog_wr*/, NULL, "" },
	{ gen_cmd, color_rd, color_wr, rgb_dup, "<color|#rrggbb>" },

	{ exec_cmd, NULL, NULL, NULL, "[<...>]" },

	{ redir_cmd, redir_rd, NULL, NULL, "" },

	/* tree */
	{ NULL, NULL, NULL, tree_dup, "" },
};
