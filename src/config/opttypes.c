/* Option variables types handlers */
/* $Id: opttypes.c,v 1.64 2003/10/22 19:57:32 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "elinks.h"

#include "bfu/listbox.h"
#include "config/options.h"
#include "config/opttypes.h"
#include "intl/charsets.h"
#include "intl/gettext/libintl.h"
#include "util/color.h"
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
	o->value.number = 1;

	if (!*argc) return NULL;

	/* Argument is empty or longer than 1 char.. */
	if (!(*argv)[0][0] || (*argv)[0][1]) return NULL;

	switch ((*argv)[0][0]) {
		case '0': o->value.number = 0; break;
		case '1': o->value.number = 1; break;
		default: return NULL;
	}

	/* We ate parameter */
	(*argv)++; (*argc)--;
	return NULL;
}

static unsigned char *
exec_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	return o->value.command(o, argv, argc);
}


/* Wrappers for OPT_ALIAS. */
/* Note that they can wrap only to config_options now.  I don't think it could be
 * a problem, but who knows.. however, changing that will be pretty tricky -
 * possibly changing ptr to structure containing target name and pointer to
 * options list? --pasky */

static unsigned char *
redir_cmd(struct option *opt, unsigned char ***argv, int *argc)
{
	struct option *real = get_opt_rec(config_options, opt->value.string);

	assertm(real, "%s aliased to unknown option %s!", opt->name, opt->value.string);
	if_assert_failed { return NULL; }

	if (option_types[real->type].cmdline)
		return option_types[real->type].cmdline(real, argv, argc);

	return NULL;
}

static unsigned char *
redir_rd(struct option *opt, unsigned char **file)
{
	struct option *real = get_opt_rec(config_options, opt->value.string);

	assertm(real, "%s aliased to unknown option %s!", opt->name, opt->value.string);
	if_assert_failed { return NULL; }

	if (option_types[real->type].read)
		return option_types[real->type].read(real, file);

	return NULL;
}

static int
redir_set(struct option *opt, unsigned char *str)
{
	struct option *real = get_opt_rec(config_options, opt->value.string);

	assertm(real, "%s aliased to unknown option %s!", opt->name, opt->value.string);
	if_assert_failed { return 0; }

	if (option_types[real->type].set)
		return option_types[real->type].set(real, str);

	return 0;
}

static int
redir_add(struct option *opt, unsigned char *str)
{
	struct option *real = get_opt_rec(config_options, opt->value.string);

	assertm(real, "%s aliased to unknown option %s!", opt->name, opt->value.string);
	if_assert_failed { return 0; }

	if (option_types[real->type].add)
		return option_types[real->type].add(real, str);

	return 0;
}

static int
redir_remove(struct option *opt, unsigned char *str)
{
	struct option *real = get_opt_rec(config_options, opt->value.string);

	assertm(real, "%s aliased to unknown option %s!", opt->name, opt->value.string);
	if_assert_failed { return 0; }

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
	opt->value.number = *((long *) str);
	return 1;
}

static int
long_set(struct option *opt, unsigned char *str)
{
	opt->value.number = *((long *) str);
	return 1;
}

static void
num_wr(struct option *option, struct string *string)
{
	add_knum_to_string(string, option->value.number);
}


static unsigned char *
str_rd(struct option *opt, unsigned char **file)
{
	unsigned char *str = *file;
	struct string str2;

	if (!init_string(&str2)) return NULL;

	/* We're getting used in some parser functions in conf.c as well, and
	 * that's w/ opt == NULL; so don't rely on opt to point anywhere. */
	if (!commandline) {
		if (*str != '"') {
			done_string(&str2);
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
		add_char_to_string(&str2, *str);
		str++;
	}

	if (!commandline && !*str) {
		done_string(&str2);
		*file = str;
		return NULL;
	}

	str++; /* Skip the quote. */
	if (!commandline) *file = str;

	if (opt && opt->max && str2.length >= opt->max) {
		done_string(&str2);
		return NULL;
	}

	return str2.source;
}

static int
str_set(struct option *opt, unsigned char *str)
{
	safe_strncpy(opt->value.string, str, MAX_STR_LEN);
	return 1;
}

static void
str_wr(struct option *o, struct string *s)
{
	int len = strlen(o->value.string);

	int_upper_bound(&len, o->max - 1);
	add_optstring_to_string(s, o->value.string, len);
}

static void
str_dup(struct option *opt, struct option *template)
{
	unsigned char *new = mem_alloc(MAX_STR_LEN);

	if (new) safe_strncpy(new, template->value.string, MAX_STR_LEN);
	opt->value.string = new;
}


static int
cp_set(struct option *opt, unsigned char *str)
{
	int ret;

	ret = get_cp_index(str);
	if (ret < 0) return 0;

	opt->value.number = ret;
	return 1;
}

static void
cp_wr(struct option *o, struct string *s)
{
	unsigned char *mime_name = get_cp_mime_name(o->value.number);

	add_optstring_to_string(s, mime_name, strlen(mime_name));
}


static int
lang_set(struct option *opt, unsigned char *str)
{
#ifdef ENABLE_NLS
	int i = name_to_language(str);

	opt->value.number = i;
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


static int
color_set(struct option *opt, unsigned char *str)
{
	int ret = decode_color(str, &opt->value.color);

	if (ret) return 0;

	return 1;
}

static void
color_wr(struct option *opt, struct string *str)
{
	color_t color = opt->value.color;
	unsigned char *strcolor = get_color_name(color);
	unsigned char hexcolor[8];

	if (!strcolor) {
		color_to_string(color, hexcolor);
		strcolor = hexcolor;
	}

	add_optstring_to_string(str, strcolor, strlen(strcolor));

	if (strcolor != hexcolor) mem_free(strcolor);
}

static void
tree_dup(struct option *opt, struct option *template)
{
	struct list_head *new = mem_alloc(sizeof(struct list_head));
	struct list_head *tree = template->value.tree;
	struct option *option;

	if (!new) return;
	init_list(*new);
	opt->value.tree = new;

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
}


struct option_type_info option_types[] = {
	{ N_("Boolean"), bool_cmd, num_rd, num_wr, NULL, int_set, NULL, NULL, N_("[0|1]") },
	{ N_("Integer"), gen_cmd, num_rd, num_wr, NULL, int_set, NULL, NULL, N_("<num>") },
	{ N_("Longint"), gen_cmd, num_rd, num_wr, NULL, long_set, NULL, NULL, N_("<num>") },
	{ N_("String"), gen_cmd, str_rd, str_wr, str_dup, str_set, NULL, NULL, N_("<str>") },

	{ N_("Codepage"), gen_cmd, str_rd, cp_wr, NULL, cp_set, NULL, NULL, N_("<codepage>") },
	{ N_("Language"), gen_cmd, str_rd, lang_wr, NULL, lang_set, NULL, NULL, N_("<language>") },
	{ N_("Color"), gen_cmd, str_rd, color_wr, NULL, color_set, NULL, NULL, N_("<color|#rrggbb>") },

	{ N_("Special"), exec_cmd, NULL, NULL, NULL, NULL, NULL, NULL, "" },

	{ N_("Alias"), redir_cmd, redir_rd, NULL, NULL, redir_set, redir_add, redir_remove, "" },

	/* tree */
	{ N_("Folder"), NULL, NULL, NULL, tree_dup, NULL, NULL, NULL, "" },
};
