/* Option variables types handlers */
/* $Id: opttypes.c,v 1.57 2003/07/21 06:05:54 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "elinks.h"

#include "bfu/listbox.h"
#include "config/options.h"
#include "config/opttypes.h"
#include "document/html/colors.h"
#include "intl/charsets.h"
#include "intl/gettext/libintl.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


/* Commandline handlers. */

/* TAKE CARE! Remember that your _rd handler can be used for commandline
 * parameters as well - probably, you don't want to be so syntactically
 * strict, and _ESPECIALLY_ you don't want to move any file pointers ahead,
 * since you will parse the commandline _TWO TIMES_! Remember! :-) */
int commandline = 0;

static unsigned char *
gen_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	unsigned char *err;
	unsigned char *str;

	if (!*argc) return gettext("Parameter expected");

	/* FIXME!! We will modify argv! (maybe) */
	commandline = 1;
	str = option_types[o->type].read(o, *argv - 0);
	commandline = 0;
	if (str) {
		/* We ate parameter */
		(*argv)++; (*argc)--;
		if (option_types[o->type].set(o, str)) {
			mem_free(str);
			return NULL;
		}
		mem_free(str);
	}
	err = gettext("Read error");

	return err;
}

/* If 0 follows, disable option and eat 0. If 1 follows, enable option and
 * eat 1. If anything else follow, enable option and don't eat anything. */
static unsigned char *
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

static unsigned char *
exec_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	return ((unsigned char *(*)(struct option *, unsigned char ***, int *)) o->ptr)(o, argv, argc);
}


/* Wrappers for OPT_ALIAS. */
/* Note that they can wrap only to config_options now.  I don't think it could be
 * a problem, but who knows.. however, changing that will be pretty tricky -
 * possibly changing ptr to structure containing target name and pointer to
 * options list? --pasky */

static unsigned char *
redir_cmd(struct option *opt, unsigned char ***argv, int *argc)
{
	struct option *real = get_opt_rec(config_options, opt->ptr);

	if (!real) {
		internal("Alias %s leads to unknown option %s!",
			 opt->name, opt->ptr);
		return NULL;
	}

	if (option_types[real->type].cmdline)
		return option_types[real->type].cmdline(real, argv, argc);

	return NULL;
}

static unsigned char *
redir_rd(struct option *opt, unsigned char **file)
{
	struct option *real = get_opt_rec(config_options, opt->ptr);

	if (!real) {
		internal("Alias %s leads to unknown option %s!",
			 opt->name, opt->ptr);
		return NULL;
	}

	if (option_types[real->type].read)
		return option_types[real->type].read(real, file);

	return NULL;
}

static int
redir_set(struct option *opt, unsigned char *str)
{
	struct option *real = get_opt_rec(config_options, opt->ptr);

	if (!real) {
		internal("Alias %s leads to unknown option %s!",
			 opt->name, opt->ptr);
		return 0;
	}

	if (option_types[real->type].set)
		return option_types[real->type].set(real, str);

	return 0;
}

static int
redir_add(struct option *opt, unsigned char *str)
{
	struct option *real = get_opt_rec(config_options, opt->ptr);

	if (!real) {
		internal("Alias %s leads to unknown option %s!",
			 opt->name, opt->ptr);
		return 0;
	}

	if (option_types[real->type].add)
		return option_types[real->type].add(real, str);

	return 0;
}

static int
redir_remove(struct option *opt, unsigned char *str)
{
	struct option *real = get_opt_rec(config_options, opt->ptr);

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

static void
add_optstring_to_string(struct string *s, unsigned char *q, int qlen)
{
 	if (!commandline) add_char_to_string(s, '"');
	add_quoted_to_string(s, q, qlen);
	if (!commandline) add_char_to_string(s, '"');
}

/* Config file handlers. */

static unsigned char *
num_rd(struct option *opt, unsigned char **file)
{
	unsigned char *end = *file;
	long *value = mem_alloc(sizeof(long));

	if (!value) return NULL;

	/* We don't want to move file if (commandline), but strtolx() second
	 * parameter must not be NULL. */
	*value = strtolx(*file, &end);
	if (!commandline) *file = end;

	/* Another trap for unwary - we need to check *end, not **file - reason
	 * is left as an exercise to the reader. */
	if ((*end != 0 && (commandline || (!WHITECHAR(*end) && *end != '#')))
	    || (*value < opt->min || *value > opt->max)) {
		mem_free(value);
		return NULL;
	}

	return (unsigned char *) value;
}

static int
int_set(struct option *opt, unsigned char *str)
{
	*((int *) opt->ptr) = *((long *) str);
	return 1;
}

static int
long_set(struct option *opt, unsigned char *str)
{
	*((long *) opt->ptr) = *((long *) str);
	return 1;
}

static void
num_wr(struct option *option, struct string *string)
{
	add_knum_to_string(string, *((int *) option->ptr));
}

static void *
int_dup(struct option *opt, struct option *template)
{
	int *new = mem_alloc(sizeof(int));

	if (new) memcpy(new, template->ptr, sizeof(int));
	return new;
}

static void *
long_dup(struct option *opt, struct option *template)
{
	long *new = mem_alloc(sizeof(long));

	if (new) memcpy(new, template->ptr, sizeof(long));
	return new;
}


static unsigned char *
str_rd(struct option *opt, unsigned char **file)
{
	unsigned char *str = *file;
	unsigned char *str2 = init_str();
	int str2l = 0;

	if (!str2) return NULL;

	/* We're getting used in some parser functions in conf.c as well, and
	 * that's w/ opt == NULL; so don't rely on opt to point anywhere. */
	if (!commandline) {
		if (*str != '"') {
			mem_free(str2);
			return NULL;
		}
		str++;
	}

	while (*str && (commandline || *str != '"')) {
		if (*str == '\\') {
			/* FIXME: This won't work on crlf systems. */
			if (str[1] == '\n') { str[1] = ' '; str++; }
			/* When there's '"', we will just move on there, thus
			 * we will never test for it in while () condition and
			 * we will treat it just as '"', ignoring the backslash
			 * itself. */
			if (str[1] == '"') str++;
			/* \\ means \. */
			if (str[1] == '\\') str++;
		}
		add_chr_to_str(&str2, &str2l, *str);
		str++;
	}

	if (!commandline && !*str) {
		mem_free(str2);
		*file = str;
		return NULL;
	}

	str++; /* Skip the quote. */
	if (!commandline) *file = str;

	if (opt && opt->max && str2l >= opt->max) {
		mem_free(str2);
		return NULL;
	}

	return str2;
}

static int
str_set(struct option *opt, unsigned char *str)
{
	safe_strncpy(opt->ptr, str, MAX_STR_LEN);
	return 1;
}

static void
str_wr(struct option *o, struct string *s)
{
	int len = strlen(o->ptr);

	add_optstring_to_string(s, o->ptr, (len >= o->max) ? o->max - 1 : len);
}

static void *
str_dup(struct option *opt, struct option *template)
{
	unsigned char *new = mem_alloc(MAX_STR_LEN);

	if (new) safe_strncpy(new, template->ptr, MAX_STR_LEN);
	return new;
}


static int
cp_set(struct option *opt, unsigned char *str)
{
	int ret;

	ret = get_cp_index(str);
	if (ret < 0) return 0;

	*((int *) opt->ptr) = ret;
	return 1;
}

static void
cp_wr(struct option *o, struct string *s)
{
	unsigned char *mime_name = get_cp_mime_name(*((int *) o->ptr));

	add_optstring_to_string(s, mime_name, strlen(mime_name));
}


static int
lang_set(struct option *opt, unsigned char *str)
{
#ifdef ENABLE_NLS
	int i = name_to_language(str);

	*((int *) opt->ptr) = i;
	set_language(i);
#endif
	return 1;
}

static void
lang_wr(struct option *o, struct string *s)
{
#ifdef ENABLE_NLS
	unsigned char *lang = language_to_name(current_language);

	add_optstring_to_string(s, lang, strlen(lang));
#endif
}


/* XXX: The extern prototype should be at a proper place at least! --pasky */
int
color_set(struct option *opt, unsigned char *str)
{
	int ret;

	ret = decode_color(str, (struct rgb *) opt->ptr);

	if (ret) return 0;

	return 1;
}

static void
color_wr(struct option *opt, struct string *str)
{
	struct rgb *color = (struct rgb *) opt->ptr;
	unsigned char *strcolor = get_color_name(color);
	unsigned char hexcolor[8];

	if (!strcolor) {
		color_to_string(color, hexcolor);
		strcolor = hexcolor;
	}

	add_optstring_to_string(str, strcolor, strlen(strcolor));

	if (strcolor != hexcolor) mem_free(strcolor);
}

static void *
color_dup(struct option *opt, struct option *template)
{
	struct rgb *new = mem_alloc(sizeof(struct rgb));

	if (new) memcpy(new, template->ptr, sizeof(struct rgb));
	return new;
}

static void *
tree_dup(struct option *opt, struct option *template)
{
	struct list_head *new = mem_alloc(sizeof(struct list_head));
	struct list_head *tree = (struct list_head *) template->ptr;
	struct option *option;

	if (!new) return NULL;
	init_list(*new);

	foreachback (option, *tree) {
		struct option *new_opt = copy_option(option);

		if (!new_opt) continue;
		add_at_pos((struct option *) new->prev, new_opt);

		if (!new_opt->box_item) continue;

		if (new_opt->name && !strcmp(new_opt->name, "_template_"))
			new_opt->box_item->visible = get_opt_int("config.show_template");

		if (opt->box_item) {
			add_at_pos((struct listbox_item *)
					opt->box_item->child.prev,
					new_opt->box_item);
		} else {
			add_at_pos((struct listbox_item *)
					config_option_box_items.prev,
					new_opt->box_item);
		}
		new_opt->box_item->root = opt->box_item;
	}

	return new;
}


struct option_type_info option_types[] = {
	{ N_("Boolean"), bool_cmd, num_rd, num_wr, int_dup, int_set, NULL, NULL, N_("[0|1]") },
	{ N_("Integer"), gen_cmd, num_rd, num_wr, int_dup, int_set, NULL, NULL, N_("<num>") },
	{ N_("Longint"), gen_cmd, num_rd, num_wr, long_dup, long_set, NULL, NULL, N_("<num>") },
	{ N_("String"), gen_cmd, str_rd, str_wr, str_dup, str_set, NULL, NULL, N_("<str>") },

	{ N_("Codepage"), gen_cmd, str_rd, cp_wr, int_dup, cp_set, NULL, NULL, N_("<codepage>") },
	{ N_("Language"), gen_cmd, str_rd, lang_wr, NULL, lang_set, NULL, NULL, N_("<language>") },
	{ N_("Color"), gen_cmd, str_rd, color_wr, color_dup, color_set, NULL, NULL, N_("<color|#rrggbb>") },

	{ N_("Special"), exec_cmd, NULL, NULL, NULL, NULL, NULL, NULL, "" },

	{ N_("Alias"), redir_cmd, redir_rd, NULL, NULL, redir_set, redir_add, redir_remove, "" },

	/* tree */
	{ N_("Folder"), NULL, NULL, NULL, tree_dup, NULL, NULL, NULL, "" },
};
