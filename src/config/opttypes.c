/* Option variables types handlers */
/* $Id: opttypes.c,v 1.24 2002/08/30 15:00:01 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "links.h"

#include "config/options.h"
#include "config/opttypes.h"
#include "document/html/colors.h"
#include "intl/charsets.h"
#include "intl/language.h"
#include "protocol/mime.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


/* Commandline handlers. */

unsigned char *
gen_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	unsigned char *error;
	unsigned char *str;

	if (!*argc) return "Parameter expected";

	/* FIXME!! We will modify argv! (maybe) */
	str = option_types[o->type].read(o, *argv - 0);
	if (str) {
		if (option_types[o->type].set(o, str)) {
			mem_free(str);
			return NULL;
		}
		mem_free(str);
	}
	error = "Read error";

	return error;
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

unsigned char *
redir_rd(struct option *opt, unsigned char **file)
{
	struct option *real = get_opt_rec(root_options, opt->ptr);

	if (!real) {
		internal("Alias %s leads to unknown option %s!",
			 opt->name, opt->ptr);
		return NULL;
	}

	if (option_types[real->type].read)
		return option_types[real->type].read(real, file);

	return NULL;
}

int
redir_set(struct option *opt, unsigned char *str)
{
	struct option *real = get_opt_rec(root_options, opt->ptr);

	if (!real) {
		internal("Alias %s leads to unknown option %s!",
			 opt->name, opt->ptr);
		return 0;
	}

	if (option_types[real->type].set)
		return option_types[real->type].set(real, str);

	return 0;
}

int
redir_add(struct option *opt, unsigned char *str)
{
	struct option *real = get_opt_rec(root_options, opt->ptr);

	if (!real) {
		internal("Alias %s leads to unknown option %s!",
			 opt->name, opt->ptr);
		return 0;
	}

	if (option_types[real->type].add)
		return option_types[real->type].add(real, str);

	return 0;
}

int
redir_remove(struct option *opt, unsigned char *str)
{
	struct option *real = get_opt_rec(root_options, opt->ptr);

	if (!real) {
		internal("Alias %s leads to unknown option %s!",
			 opt->name, opt->ptr);
		return 0;
	}

	if (option_types[real->type].remove)
		return option_types[real->type].remove(real, str);

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

unsigned char *
num_rd(struct option *opt, unsigned char **file)
{
	long *value = mem_alloc(sizeof(long));

	*value = strtolx(*file, file);

	if ((**file != 0 && !WHITECHAR(**file) && **file != '#')
	    || (*value < opt->min || *value > opt->max)) {
		mem_free(value);
		return NULL;
	}

	return (unsigned char *) value;
}

int
int_set(struct option *opt, unsigned char *str)
{
	*((int *) opt->ptr) = *((long *) str);
	return 1;
}

int
long_set(struct option *opt, unsigned char *str)
{
	*((long *) opt->ptr) = *((long *) str);
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


unsigned char *
str_rd(struct option *opt, unsigned char **file)
{
	unsigned char *str = *file;
	unsigned char *str2 = init_str();
	int str2l = 0;

	/* We're getting used in some parser functions in conf.c as well, and
	 * that's w/ opt == NULL; so don't rely on opt to point anywhere. */
	if (*str != '"') return NULL;
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

	if (!*str) {
		mem_free(str2);
		*file = str;
		return NULL;
	}

	str++; /* Skip the quote. */
	*file = str;

	if (opt && opt->max && str2l >= opt->max) {
		mem_free(str2);
		return NULL;
	}

	return str2;
}

int
str_set(struct option *opt, unsigned char *str)
{
	safe_strncpy(opt->ptr, str, MAX_STR_LEN);
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
cp_set(struct option *opt, unsigned char *str)
{
	int ret;

	*((int *) opt->ptr) = ret = get_cp_index(str);

	if (ret < 0) return 0;

	return 1;
}

void
cp_wr(struct option *o, unsigned char **s, int *l)
{
	add_quoted_to_str(s, l, get_cp_mime_name(*((int *) o->ptr)));
}


int
lang_set(struct option *opt, unsigned char *str)
{
	int i;

	for (i = 0; i < n_languages(); i++)
		if (!strcasecmp(language_name(i), str)) {
			*((int *) opt->ptr) = i;
			set_language(i);
			return 1;
		}

	*((int *) opt->ptr) = -1;

	return 0;
}

void
lang_wr(struct option *o, unsigned char **s, int *l)
{
	add_quoted_to_str(s, l, language_name(current_language));
}


int
color_set(struct option *opt, unsigned char *str)
{
	int ret;

	ret = decode_color(str, (struct rgb *) opt->ptr);

	if (ret) return 0;

	return 1;
}

void
color_wr(struct option *opt, unsigned char **str, int *len)
{
	struct rgb *color = (struct rgb *) opt->ptr;
	unsigned char *strcolor = get_color_name(color);
	
	if (!strcolor) {
		strcolor = (unsigned char *) mem_alloc(8 * sizeof(unsigned char));
		if (!strcolor) return;
		snprintf(strcolor, 8, "#%02x%02x%02x", color->r, color->g, color->b);
	}

	add_quoted_to_str(str, len, strcolor);

	mem_free(strcolor);	
}

void *
color_dup(struct option *opt)
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
	{ bool_cmd, num_rd, num_wr, int_dup, int_set, NULL, NULL, "[0|1]" },
	{ gen_cmd, num_rd, num_wr, int_dup, int_set, NULL, NULL, "<num>" },
	{ gen_cmd, num_rd, num_wr, long_dup, long_set, NULL, NULL, "<num>" },
	{ gen_cmd, str_rd, str_wr, str_dup, str_set, NULL, NULL, "<str>" },

	{ gen_cmd, str_rd, cp_wr, int_dup, cp_set, NULL, NULL, "<codepage>" },
	{ gen_cmd, str_rd, lang_wr, NULL, lang_set, NULL, NULL, "<language>" },
	{ gen_cmd, str_rd, color_wr, color_dup, color_set, NULL, NULL, "<color|#rrggbb>" },

	{ exec_cmd, NULL, NULL, NULL, NULL, NULL, NULL, "[<...>]" },

	{ redir_cmd, redir_rd, NULL, NULL, redir_set, redir_add, redir_remove, "" },

	/* tree */
	{ NULL, NULL, NULL, tree_dup, NULL, NULL, NULL, "" },
};
