/* Config file manipulation */
/* $Id: conf.c,v 1.154 2005/04/17 16:37:42 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h> /* OS/2 needs this after sys/types.h */
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "config/conf.h"
#include "config/dialogs.h"
#include "config/kbdbind.h"
#include "config/options.h"
#include "config/opttypes.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/home.h"
#include "osdep/osdep.h"
#include "terminal/terminal.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/secsave.h"
#include "util/string.h"


/* Config file has only very simple grammar:
 *
 * /set *option *= *value/
 * /bind *keymap *keystroke *= *action/
 * /include *file/
 * /#.*$/
 *
 * Where option consists from any number of categories separated by dots and
 * name of the option itself. Both category and option name consists from
 * [a-zA-Z0-9_-*] - using uppercase letters is not recommended, though. '*' is
 * reserved and is used only as escape character in place of '.' originally in
 * option name.
 *
 * Value can consist from:
 * - number (it will be converted to int/long)
 * - enum (like on, off; true, fake, last_url; etc ;) - in planning state yet
 * - string - "blah blah" (keymap, keystroke and action and file looks like that too)
 *
 * "set" command is parsed first, and then type-specific function is called,
 * with option as one parameter and value as a second. Usually it just assigns
 * value to an option, but sometimes you may want to first create the option
 * ;). Then this will come handy. */


/* Skip comments and whitespace,
 * setting *@line to the number of lines skipped. */
static unsigned char *
skip_white(unsigned char *start, int *line)
{
	while (*start) {
		while (isspace(*start)) {
			if (*start == '\n') {
				(*line)++;
			}
			start++;
		}

		if (*start == '#') {
			start += strcspn(start, "\n");
		} else {
			return start;
		}
	}

	return start;
}

/* Parse a command. Returns error code. */
/* If dynamic string credentials are supplied, we will mirror the command at
 * the end of the string; however, we won't load the option value to the tree,
 * and we will even write option value from the tree to the output string. We
 * will only possibly set OPT_WATERMARK flag to the option (if enabled). */

static enum parse_error
parse_set(struct option *opt_tree, unsigned char **file, int *line,
	  struct string *mirror)
{
	unsigned char *orig_pos = *file;
	unsigned char *optname;
	unsigned char bin;

	*file = skip_white(*file, line);
	if (!**file) return ERROR_PARSE;

	/* Option name */
	optname = *file;
	while (isident(**file) || **file == '*' || **file == '.') (*file)++;

	bin = **file;
	**file = '\0';
	optname = stracpy(optname);
	if (!optname) return ERROR_NOMEM;
	**file = bin;

	*file = skip_white(*file, line);

	/* Equal sign */
	if (**file != '=') { mem_free(optname); return ERROR_PARSE; }
	(*file)++; /* '=' */
	*file = skip_white(*file, line);
	if (!**file) { mem_free(optname); return ERROR_VALUE; }

	/* Mirror what we already have */
	if (mirror) add_bytes_to_string(mirror, orig_pos, *file - orig_pos);

	/* Option value */
	{
		struct option *opt;
		unsigned char *val;

		opt = mirror ? get_opt_rec_real(opt_tree, optname) : get_opt_rec(opt_tree, optname);
		mem_free(optname);

		if (!opt || (opt->flags & OPT_HIDDEN))
			return ERROR_OPTION;

		if (!option_types[opt->type].read)
			return ERROR_VALUE;

		val = option_types[opt->type].read(opt, file, line);
		if (!val) return ERROR_VALUE;

		if (mirror) {
			if (opt->flags & OPT_DELETED)
				opt->flags &= ~OPT_WATERMARK;
			else
				opt->flags |= OPT_WATERMARK;
			if (option_types[opt->type].write) {
				option_types[opt->type].write(opt, mirror);
			}
		} else if (!option_types[opt->type].set
			   || !option_types[opt->type].set(opt, val)) {
			mem_free(val);
			return ERROR_VALUE;
		}
		/* This is not needed since this will be WATERMARK'd when
		 * saving it. We won't need to save it as touched. */
		/* if (!str) opt->flags |= OPT_TOUCHED; */
		mem_free(val);
	}

	return ERROR_NONE;
}

static enum parse_error
parse_unset(struct option *opt_tree, unsigned char **file, int *line,
	    struct string *mirror)
{
	unsigned char *orig_pos = *file;
	unsigned char *optname;
	unsigned char bin;

	/* XXX: This does not handle the autorewriting well and is mostly a
	 * quick hack than anything now. --pasky */

	*file = skip_white(*file, line);
	if (!**file) return ERROR_PARSE;

	/* Option name */
	optname = *file;
	while (isident(**file) || **file == '*' || **file == '.') (*file)++;

	bin = **file;
	**file = '\0';
	optname = stracpy(optname);
	if (!optname) return ERROR_NOMEM;
	**file = bin;

	/* Mirror what we have */
	if (mirror) add_bytes_to_string(mirror, orig_pos, *file - orig_pos);

	{
		struct option *opt;

		opt = get_opt_rec_real(opt_tree, optname);
		mem_free(optname);

		if (!opt || (opt->flags & OPT_HIDDEN))
			return ERROR_OPTION;

		if (!mirror) {
			if (opt->flags & OPT_ALLOC) delete_option(opt);
		} else {
			if (opt->flags & OPT_DELETED)
				opt->flags |= OPT_WATERMARK;
			else
				opt->flags &= ~OPT_WATERMARK;
		}
	}

	return ERROR_NONE;
}

static enum parse_error
parse_bind(struct option *opt_tree, unsigned char **file, int *line,
	   struct string *mirror)
{
	unsigned char *orig_pos = *file, *next_pos;
	unsigned char *keymap, *keystroke, *action;
	enum parse_error err = ERROR_NONE;

	*file = skip_white(*file, line);
	if (!*file) return ERROR_PARSE;

	/* Keymap */
	keymap = option_types[OPT_STRING].read(NULL, file, line);
	*file = skip_white(*file, line);
	if (!keymap || !**file)
		return ERROR_OPTION;

	/* Keystroke */
	keystroke = option_types[OPT_STRING].read(NULL, file, line);
	*file = skip_white(*file, line);
	if (!keystroke || !**file) {
		mem_free(keymap);
		return ERROR_OPTION;
	}

	/* Equal sign */
	*file = skip_white(*file, line);
	if (**file != '=') {
		mem_free(keymap); mem_free(keystroke);
		return ERROR_PARSE;
	}
	(*file)++; /* '=' */

	*file = skip_white(*file, line);
	if (!**file) {
		mem_free(keymap); mem_free(keystroke);
		return ERROR_PARSE;
	}

	/* Action */
	next_pos = *file;
	action = option_types[OPT_STRING].read(NULL, file, line);
	if (!action) {
		mem_free(keymap);
		return ERROR_VALUE;
	}

	if (mirror) {
		/* Mirror what we already have */
		unsigned char *act_str = bind_act(keymap, keystroke);

		if (act_str) {
			add_bytes_to_string(mirror, orig_pos,
					    next_pos - orig_pos);
			add_to_string(mirror, act_str);
			mem_free(act_str);
		} else {
			err = ERROR_VALUE;
		}
	} else {
		/* We don't bother to bind() if -default-keys. */
		if (!get_cmd_opt_bool("default-keys")
		    && bind_do(keymap, keystroke, action)) {
			/* bind_do() tried but failed. */
			err = ERROR_VALUE;
		} else {
			err = ERROR_NONE;
		}
	}
	mem_free(keymap); mem_free(keystroke); mem_free(action);
	return err;
}

static int load_config_file(unsigned char *, unsigned char *, struct option *,
			    struct string *);

static enum parse_error
parse_include(struct option *opt_tree, unsigned char **file, int *line,
	      struct string *mirror)
{
	unsigned char *orig_pos = *file;
	unsigned char *fname;
	struct string dumbstring;

	if (!init_string(&dumbstring)) return ERROR_NOMEM;

	*file = skip_white(*file, line);
	if (!*file) return ERROR_PARSE;

	/* File name */
	fname = option_types[OPT_STRING].read(NULL, file, line);
	if (!fname)
		return ERROR_VALUE;

	/* Mirror what we already have */
	if (mirror) add_bytes_to_string(mirror, orig_pos, *file - orig_pos);

	/* We want load_config_file() to watermark stuff, but not to load
	 * anything, polluting our beloved options tree - thus, we will feed it
	 * with some dummy string which we will destroy later; still better
	 * than cloning whole options tree or polluting interface with another
	 * rarely-used option ;). */
	/* XXX: We should try CONFDIR/<file> when proceeding
	 * CONFDIR/<otherfile> ;). --pasky */
	if (load_config_file(fname[0] == '/' ? (unsigned char *) ""
					     : elinks_home,
			     fname, opt_tree, &dumbstring)) {
		done_string(&dumbstring);
		mem_free(fname);
		return ERROR_VALUE;
	}

	done_string(&dumbstring);
	mem_free(fname);
	return ERROR_NONE;
}


struct parse_handler {
	unsigned char *command;
	enum parse_error (*handler)(struct option *opt_tree,
				    unsigned char **file, int *line,
				    struct string *mirror);
};

static struct parse_handler parse_handlers[] = {
	{ "set", parse_set },
	{ "unset", parse_unset },
	{ "bind", parse_bind },
	{ "include", parse_include },
	{ NULL, NULL }
};


enum parse_error
parse_config_command(struct option *options, unsigned char **file, int *line,
		     struct string *mirror)
{
	struct parse_handler *handler;

	for (handler = parse_handlers; handler->command;
	     handler++) {
		int cmdlen = strlen(handler->command);

		if (!strncmp(*file, handler->command, cmdlen)
		    && isspace((*file)[cmdlen])) {
			enum parse_error err;
			struct string mirror2 = NULL_STRING;
			struct string *m2 = NULL;

			/* Mirror what we already have */
			if (mirror && init_string(&mirror2)) {
				m2 = &mirror2;
				add_bytes_to_string(m2, *file, cmdlen);
			}


			*file += cmdlen;
			err = handler->handler(options, file, line, m2);
			if (!err && mirror && m2) {
				add_string_to_string(mirror, m2);
			}
			if (m2)	done_string(m2);
			return err;
		}
	}

	return ERROR_COMMAND;
}

void
parse_config_file(struct option *options, unsigned char *name,
		  unsigned char *file, struct string *mirror)
{
	int line = 1;
	int error_occured = 0;
	enum parse_error err = 0;
	enum verbose_level verbose = get_cmd_opt_int("verbose");
	unsigned char error_msg[][40] = {
		"no error",
		"parse error",
		"unknown command",
		"unknown option",
		"bad value",
		"no memory left",
	};

	while (file && *file) {
		unsigned char *orig_pos = file;

		/* Skip all possible comments and whitespace. */
		file = skip_white(file, &line);

		/* Mirror what we already have */
		if (mirror)
			add_bytes_to_string(mirror, orig_pos, file - orig_pos);

		/* Second chance to escape from the hell. */
		if (!*file) break;

		err = parse_config_command(options, &file, &line, mirror);

		if (err == ERROR_COMMAND) {
			orig_pos = file;
			/* Jump over this crap we can't understand. */
			while (!isspace(*file) && *file != '#' && *file)
				file++;

			/* Mirror what we already have */
			if (mirror) add_bytes_to_string(mirror, orig_pos,
							file - orig_pos);
		}

		if (!mirror && err) {
			/* TODO: Make this a macro and report error directly
			 * as it's stumbled upon; line info may not be accurate
			 * anymore now (?). --pasky */
			if (verbose >= VERBOSE_WARNINGS) {
				fprintf(stderr, "%s:%d: %s\n",
					name, line, error_msg[err]);
				error_occured = 1;
			}
			err = 0;
		}
	}

	if (!error_occured) return;

	/* If an error occurred make sure that the user is notified and is able
	 * to see it. First sound the bell. Then, if the text viewer is going to
	 * be started, sleep for a while so the message will be visible before
	 * ELinks starts drawing to on the screen and overwriting any error
	 * messages. This should not be necessary for terminals supporting the
	 * alternate screen mode but better to be safe. (debian bug 305017) */

	fputc('\a', stderr);

	if (get_cmd_opt_bool("dump")
	    || get_cmd_opt_bool("source"))
		return;

	sleep(1);
}



static unsigned char *
read_config_file(unsigned char *name)
{
#define FILE_BUF	1024
	unsigned char cfg_buffer[FILE_BUF];
	struct string string;
	int fd, r;

	fd = open(name, O_RDONLY | O_NOCTTY);
	if (fd < 0) return NULL;
	set_bin(fd);

	if (!init_string(&string)) return NULL;

	while ((r = safe_read(fd, cfg_buffer, FILE_BUF)) > 0) {
		int i;

		/* Clear problems ;). */
		for (i = 0; i < r; i++)
			if (!cfg_buffer[i])
				cfg_buffer[i] = ' ';

		add_bytes_to_string(&string, cfg_buffer, r);
	}

	if (r < 0) done_string(&string);
	close(fd);

	return string.source;
#undef FILE_BUF
}

/* Return 0 on success. */
static int
load_config_file(unsigned char *prefix, unsigned char *name,
		 struct option *options, struct string *mirror)
{
	unsigned char *config_str, *config_file;

	config_file = straconcat(prefix, "/", name, NULL);
	if (!config_file) return 1;

	config_str = read_config_file(config_file);
	if (!config_str) {
		mem_free(config_file);
		config_file = straconcat(prefix, "/.", name, NULL);
		if (!config_file) return 2;

		config_str = read_config_file(config_file);
		if (!config_str) {
			mem_free(config_file);
			return 3;
		}
	}

	parse_config_file(options, config_file, config_str, mirror);

	mem_free(config_str);
	mem_free(config_file);

	return 0;
}

static void
load_config_from(unsigned char *file, struct option *tree)
{
	load_config_file(CONFDIR, file, tree, NULL);
	load_config_file(empty_string_or_(elinks_home), file, tree, NULL);
}

void
load_config(void)
{
	load_config_from(get_cmd_opt_str("config-file"),
			 config_options);
}


static int indentation = 2;
/* 0 -> none, 1 -> only option full name+type, 2 -> only desc, 3 -> both */
static int comments = 3;
static int touching = 0;

static inline unsigned char *
conf_i18n(unsigned char *s, int i18n)
{
	if (i18n) return gettext(s);
	return s;
}

static void
add_indent_to_string(struct string *string, int depth)
{
	if (!depth) return;

	add_xchar_to_string(string, ' ', depth * indentation);
}

static void
smart_config_output_fn(struct string *string, struct option *option,
		       unsigned char *path, int depth, int do_print_comment,
		       int action, int i18n)
{
	unsigned char *desc_i18n;

	if (option->type == OPT_ALIAS)
		return;

	/* XXX: OPT_LANGUAGE shouldn't have any bussiness here, but we're just
	 * weird in that area. */
	if (touching && !(option->flags & OPT_TOUCHED)
	    && option->type != OPT_LANGUAGE)
		return;

	switch (action) {
		case 0:
			if (!(comments & 1)) break;

			add_indent_to_string(string, depth);

			add_to_string(string, "## ");
			if (path) {
				add_to_string(string, path);
				add_char_to_string(string, '.');
			}
			add_to_string(string, option->name);
			add_char_to_string(string, ' ');
			add_to_string(string, option_types[option->type].help_str);
			add_char_to_string(string, '\n');
			break;

		case 1:
			if (!(comments & 2)) break;

			if (!option->desc || !do_print_comment)
				break;

			desc_i18n = conf_i18n(option->desc, i18n);

			if (!*desc_i18n) break;

			add_indent_to_string(string, depth);
			add_to_string(string, "#  ");
			{
				unsigned char *i = desc_i18n;
				unsigned char *j = i;
				unsigned char *last_space = NULL;
				int config_width = 80;
				int n = depth * indentation + 3;

				for (; *i; i++, n++) {
					if (*i == '\n') {
						last_space = i;
						goto split;
					}

					if (*i == ' ') last_space = i;

					if (n >= config_width && last_space) {
split:
						add_bytes_to_string(string, j, last_space - j);
						add_char_to_string(string, '\n');
						add_indent_to_string(string, depth);
						add_to_string(string, "#  ");
						j = last_space + 1;
						n = depth * indentation + 2 + i - last_space;
						last_space = NULL;
					}
				}
				add_to_string(string, j);
			}
			add_char_to_string(string, '\n');
			break;

		case 2:

			add_indent_to_string(string, depth);
			if (option->flags & OPT_DELETED) {
				add_to_string(string, "un");
			}
			add_to_string(string, "set ");
			if (path) {
				add_to_string(string, path);
				add_char_to_string(string, '.');
			}
			add_to_string(string, option->name);
			if (!(option->flags & OPT_DELETED)) {
				add_to_string(string, " = ");
				/* OPT_ALIAS won't ever. OPT_TREE won't reach action 2.
				 * OPT_SPECIAL makes no sense in the configuration
				 * context. */
				assert(option_types[option->type].write);
				option_types[option->type].write(option, string);
			}
			add_char_to_string(string, '\n');
			if (do_print_comment) add_char_to_string(string, '\n');
			break;

		case 3:
			if (do_print_comment < 2)
				add_char_to_string(string, '\n');
			break;
	}
}


static void
add_cfg_header_to_string(struct string *string, unsigned char *text)
{
	int n = strlen(text) + 2;

	int_bounds(&n, 10, 80);

	add_to_string(string, "\n\n\n");
	add_xchar_to_string(string, '#', n);
	add_to_string(string, "\n# ");
	add_to_string(string, text);
	add_to_string(string, "#\n\n");
}

static unsigned char *
create_config_string(unsigned char *prefix, unsigned char *name,
		     struct option *options)
{
	struct string config;
	/* Don't write headers if nothing will be added anyway. */
	struct string tmpstring;
	int origlen;
	int savestyle = get_opt_int("config.saving_style");
	int i18n = get_opt_bool("config.i18n");

	if (!init_string(&config)) return NULL;

	if (savestyle == 3) {
		touching = 1;
		savestyle = 1;
	} else {
		touching = 0;
	}

	if (savestyle == 2) watermark_deleted_options(options->value.tree);

	/* Scaring. */
	if (savestyle == 2
	    || (savestyle < 2
		&& (load_config_file(prefix, name, options, &config)
		    || !config.length))) {
		/* At first line, and in English, write ELinks version, may be
		 * of some help in future. Please keep that format for it.
		 * --Zas */
		add_to_string(&config, "## ELinks " VERSION " configuration file\n\n");
		assert(savestyle >= 0  && savestyle <= 2);
		switch (savestyle) {
		case 0:
			add_to_string(&config, conf_i18n(N_(
			"## This is ELinks configuration file. You can edit it manually,\n"
			"## if you wish so; this file is edited by ELinks when you save\n"
			"## options through UI, however only option values will be altered\n"
			"## and all your formatting, own comments etc will be kept as-is.\n"),
			i18n));
			break;
		case 1:
			add_to_string(&config, conf_i18n(N_(
			"## This is ELinks configuration file. You can edit it manually,\n"
			"## if you wish so; this file is edited by ELinks when you save\n"
			"## options through UI, however only option values will be altered\n"
			"## and missing options will be added at the end of file; if option\n"
			"## is not written in this file, but in some file included from it,\n"
			"## it is NOT counted as missing. Note that all your formatting,\n"
			"## own comments and so on will be kept as-is.\n"), i18n));
			break;
		case 2:
			add_to_string(&config, conf_i18n(N_(
			"## This is ELinks configuration file. You can edit it manually,\n"
			"## if you wish so, but keep in mind that this file is overwritten\n"
			"## by ELinks when you save options through UI and you are out of\n"
			"## luck with your formatting and own comments then, so beware.\n"),
			i18n));
			break;
		}

		add_to_string(&config, "##\n");

		add_to_string(&config, conf_i18n(N_(
			"## Obviously, if you don't like what ELinks is going to do with\n"
			"## this file, you can change it by altering the config.saving_style\n"
			"## option. Come on, aren't we friendly guys after all?\n"), i18n));
	}

	if (savestyle == 0) goto get_me_out;

	indentation = get_opt_int("config.indentation");
	comments = get_opt_int("config.comments");

	if (!init_string(&tmpstring)) goto get_me_out;

	add_cfg_header_to_string(&tmpstring,
				 conf_i18n(N_("Automatically saved options\n"), i18n));

	origlen = tmpstring.length;
	smart_config_string(&tmpstring, 2, i18n, options->value.tree, NULL, 0,
			    smart_config_output_fn);
	if (tmpstring.length > origlen)
		add_bytes_to_string(&config, tmpstring.source, tmpstring.length);
	done_string(&tmpstring);

	if (!init_string(&tmpstring)) goto get_me_out;

	add_cfg_header_to_string(&tmpstring,
				 conf_i18n(N_("Automatically saved keybindings\n"), i18n));

	origlen = tmpstring.length;
	bind_config_string(&tmpstring);
	if (tmpstring.length > origlen)
		add_bytes_to_string(&config, tmpstring.source, tmpstring.length);
	done_string(&tmpstring);

get_me_out:
	unmark_options_tree(options->value.tree);

	return config.source;
}

static int
write_config_file(unsigned char *prefix, unsigned char *name,
		  struct option *options, struct terminal *term)
{
	int ret = -1;
	struct secure_save_info *ssi;
	unsigned char *config_file = NULL;
	unsigned char *cfg_str = create_config_string(prefix, name, options);
	int prefixlen = strlen(prefix);
	int prefix_has_slash = (prefixlen && dir_sep(prefix[prefixlen - 1]));
	int name_has_slash = dir_sep(name[0]);
	unsigned char *slash = name_has_slash || prefix_has_slash ? "" : "/";

	if (!cfg_str) return -1;

	if (name_has_slash && prefix_has_slash) name++;

	config_file = straconcat(prefix, slash, name, NULL);
	if (!config_file) goto free_cfg_str;

	ssi = secure_open(config_file, 0177);
	if (ssi) {
		secure_fputs(ssi, cfg_str);
		ret = secure_close(ssi);
	}

	write_config_dialog(term, config_file, secsave_errno, ret);
	mem_free(config_file);

free_cfg_str:
	mem_free(cfg_str);

	return ret;
}

int
write_config(struct terminal *term)
{
	assert(term);

	if (!elinks_home) {
		write_config_dialog(term, get_cmd_opt_str("config-file"),
				    SS_ERR_DISABLED, 0);
		return -1;
	}

	return write_config_file(elinks_home, get_cmd_opt_str("config-file"),
				 config_options, term);
}
