/* Config file manipulation */
/* $Id: conf.c,v 1.88 2003/07/21 14:18:57 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
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


/* Skip block of whitespaces. */
static unsigned char *
skip_white(unsigned char *start, int *line)
{
	while (*start) {
		while (WHITECHAR(*start)) {
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

enum parse_error {
	ERROR_NONE,
	ERROR_PARSE,
	ERROR_COMMAND,
	ERROR_OPTION,
	ERROR_VALUE,
	ERROR_NOMEM,
};

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
	while (isA(**file) || **file == '*' || **file == '.') (*file)++;

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

		opt = get_opt_rec(opt_tree, optname);
		mem_free(optname);

		if (!opt || (opt->flags & OPT_HIDDEN))
			return ERROR_OPTION;

		if (!option_types[opt->type].read)
			return ERROR_VALUE;

		val = option_types[opt->type].read(opt, file);
		if (!val) {
			return ERROR_VALUE;
		} else if (mirror) {
			opt->flags |= OPT_WATERMARK;
			if (option_types[opt->type].write)
				option_types[opt->type].write(opt, mirror);
		} else if (!val || !option_types[opt->type].set
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
parse_bind(struct option *opt_tree, unsigned char **file, int *line,
	   struct string *mirror)
{
	unsigned char *orig_pos = *file, *next_pos;
	unsigned char *keymap, *keystroke, *action;
	enum parse_error err = ERROR_NONE;

	*file = skip_white(*file, line);
	if (!*file) return ERROR_PARSE;

	/* Keymap */
	keymap = option_types[OPT_STRING].read(NULL, file);
	*file = skip_white(*file, line);
	if (!keymap || !**file)
		return ERROR_OPTION;

	/* Keystroke */
	keystroke = option_types[OPT_STRING].read(NULL, file);
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
	action = option_types[OPT_STRING].read(NULL, file);
	if (!action) {
		mem_free(keymap);
		return ERROR_VALUE;
	}

	if (mirror) {
		/* Mirror what we already have */
		unsigned char *act_str = bind_act(keymap, keystroke);

		if (act_str) {
			add_bytes_to_string(mirror, orig_pos, next_pos - orig_pos);
			add_to_string(mirror, act_str);
			mem_free(act_str);
		} else {
			err = ERROR_VALUE;
		}
	} else {
		err = bind_do(keymap, keystroke, action) ? ERROR_VALUE : ERROR_NONE;
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
	fname = option_types[OPT_STRING].read(NULL, file);
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
	{ "bind", parse_bind },
	{ "include", parse_include },
	{ NULL, NULL }
};


void
parse_config_file(struct option *options, unsigned char *name,
		  unsigned char *file, struct string *mirror)
{
	int line = 1;
	int error_occured = 0;
	enum parse_error err = 0;
	unsigned char error_msg[][80] = {
		"no error",
		"parse error",
		"unknown command",
		"unknown option",
		"bad value",
		"no memory left",
	};

	while (file && *file) {
		unsigned char *orig_pos = file;

		/* Skip all possible comments and whitespaces. */
		file = skip_white(file, &line);

		/* Mirror what we already have */
		if (mirror) add_bytes_to_string(mirror, orig_pos, file - orig_pos);

		/* Second chance to escape from the hell. */
		if (!*file) break;

		{
			struct parse_handler *handler;

			for (handler = parse_handlers; handler->command;
			     handler++) {
				int cmdlen = strlen(handler->command);

				if (!strncmp(file, handler->command, cmdlen)
				    && WHITECHAR(file[cmdlen])) {
					struct string mirror2 = NULL_STRING;
					struct string *m2;

					/* Mirror what we already have */
					if (mirror && init_string(&mirror2)) {
						m2 = &mirror2;
						add_bytes_to_string(m2,
								    file, cmdlen);
					} else {
						m2 = NULL;
					}


					file += cmdlen;
					err = handler->handler(options,
							       &file, &line,
							       mirror?m2:NULL);
					if (!err && mirror && m2) {
						add_to_string(mirror, m2->source);
					}
					if (m2)	done_string(m2);
					goto test_end;
				}
			}
		}

		err = ERROR_COMMAND;
		orig_pos = file;
		/* Jump over this crap we can't understand. */
		while (!WHITECHAR(*file) && *file != '#' && *file)
			file++;

		/* Mirror what we already have */
		if (mirror) add_bytes_to_string(mirror, orig_pos, file - orig_pos);

test_end:

		if (!mirror && err) {
			/* TODO: Make this a macro and report error directly
			 * as it's stumbled upon; line info may not be accurate
			 * anymore now (?). --pasky */
			errfile = name, errline = line;
			elinks_error("%s",  error_msg[err]);
			error_occured = 1;
			err = 0;
		}
	}

	if (error_occured) {
		sleep(1);
	}
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

	while ((r = read(fd, cfg_buffer, FILE_BUF)) > 0) {
		int i;

		/* Clear problems ;). */
		for (i = 0; i < r; i++)
			if (!cfg_buffer[i])
				cfg_buffer[i] = ' ';

		add_bytes_to_string(&string, cfg_buffer, r);
	}

	if (r < 0)
		done_string(&string);

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

void
load_config(void)
{
	static unsigned char *cf = "elinks.conf";

	load_config_file(CONFDIR, cf, config_options, NULL);
	load_config_file(elinks_home ? elinks_home : (unsigned char *) "",
			 cf, config_options, NULL);
}


static int indentation = 2;
/* 0 -> none, 1 -> only option full name+type, 2 -> only desc, 3 -> both */
static int comments = 3;
static int touching = 0;

static void
smart_config_output_fn(struct string *string, struct option *option,
		       unsigned char *path, int depth, int do_print_comment,
		       int action)
{
	int i, l;

	/* When we're OPT_TREE, we won't get called with action 2 anyway and
	 * we want to pop out a comment. */
	if (option->type != OPT_TREE && !option_types[option->type].write)
		return;

	/* XXX: OPT_LANGUAGE shouldn't have any bussiness here, but we're just
	 * weird in that area. */
	if (touching && !(option->flags & OPT_TOUCHED) && option->type != OPT_LANGUAGE)
		return;

	switch (action) {
		case 0:
			if (!(comments & 1)) break;
			if (depth)
				add_xchar_to_string(string, ' ', depth * indentation);

			add_to_string(string, "## ");
			if (path) {
				add_to_string(string, path);
				add_char_to_string(string, '.');
			}
			add_to_string(string, option->name);
			add_char_to_string(string, ' ');
			add_to_string(string, option_types[option->type].help_str);
			add_to_string(string, NEWLINE);
			break;

		case 1:
			if (!(comments & 2)) break;

			if (!option->desc || !do_print_comment)
				break;

			l = strlen(option->desc);

			if (depth)
				add_xchar_to_string(string, ' ', depth * indentation);
			add_to_string(string, "# ");

			for (i = 0; i < l; i++) {
				if (option->desc[i] == '\n') {
					add_to_string(string, NEWLINE);
					if (depth)
						add_xchar_to_string(string, ' ',
								    depth * indentation);
					add_to_string(string, "# ");
				} else {
					add_char_to_string(string,
							   option->desc[i]);
				}
			}

			add_to_string(string, NEWLINE);
			break;

		case 2:
			if (depth)
				add_xchar_to_string(string, ' ', depth * indentation);
			add_to_string(string, "set ");
			if (path) {
				add_to_string(string, path);
				add_char_to_string(string, '.');
			}
			add_to_string(string, option->name);
			add_to_string(string, " = ");
			option_types[option->type].write(option, string);
			add_to_string(string, NEWLINE);
			if (do_print_comment) add_to_string(string, NEWLINE);
			break;

		case 3:
			if (do_print_comment < 2)
				add_to_string(string, NEWLINE);
			break;
	}
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

	if (!init_string(&config)) return NULL;

	if (savestyle == 3) {
		touching = 1;
		savestyle = 1;
	} else {
		touching = 0;
	}

	/* Scaring. */
	if (savestyle == 2
	    || (savestyle < 2
		&& (load_config_file(prefix, name, options, &config)
		    || !config.length))) {
		unsigned char headings[3][1024] = {
			"## This is ELinks configuration file. You can edit it manually," NEWLINE
			"## if you wish so; this file is edited by ELinks when you save" NEWLINE
			"## options through UI, however only option values will be altered" NEWLINE
			"## and all your formatting, own comments etc will be kept as-is." NEWLINE,

			"## This is ELinks configuration file. You can edit it manually," NEWLINE
			"## if you wish so; this file is edited by ELinks when you save" NEWLINE
			"## options through UI, however only option values will be altered" NEWLINE
			"## and missing options will be added at the end of file; if option" NEWLINE
			"## is not written in this file, but in some file included from it," NEWLINE
			"## it is NOT counted as missing. Note that all your formatting," NEWLINE
			"## own comments and so on will be kept as-is." NEWLINE,

			"## This is ELinks configuration file. You can edit it manually," NEWLINE
			"## if you wish so, but keep in mind that this file is overwritten" NEWLINE
			"## by ELinks when you save options through UI and you are out of" NEWLINE
			"## luck with your formatting and own comments then, so beware." NEWLINE,
		};

		add_to_string(&config, headings[savestyle]);
		add_to_string(&config,
			"##" NEWLINE
			"## Obviously, if you don't like what ELinks is going to do with" NEWLINE
			"## this file, you can change it by altering the config.saving_style" NEWLINE
			"## option. Come on, aren't we friendly guys after all?" NEWLINE);
	}

	if (savestyle == 0)
		goto get_me_out;

	indentation = get_opt_int("config.indentation");
	comments = get_opt_int("config.comments");

	if (!init_string(&tmpstring)) goto get_me_out;

	add_to_string(&tmpstring, NEWLINE NEWLINE NEWLINE);
	add_to_string(&tmpstring, "#####################################" NEWLINE);
	add_to_string(&tmpstring, "# Automatically saved options" NEWLINE);
	add_to_string(&tmpstring, "#" NEWLINE);
	add_to_string(&tmpstring, NEWLINE);

	origlen = tmpstring.length;
	smart_config_string(&tmpstring, 2, options->ptr, NULL, 0, smart_config_output_fn);
	if (tmpstring.length > origlen)
		add_bytes_to_string(&config, tmpstring.source, tmpstring.length);
	done_string(&tmpstring);

	if (!init_string(&tmpstring)) goto get_me_out;

	add_to_string(&tmpstring, NEWLINE NEWLINE NEWLINE);
	add_to_string(&tmpstring, "#####################################" NEWLINE);
	add_to_string(&tmpstring, "# Automatically saved keybindings" NEWLINE);
	add_to_string(&tmpstring, "#" NEWLINE);

	origlen = tmpstring.length;
	bind_config_string(&tmpstring);
	if (tmpstring.length > origlen)
		add_bytes_to_string(&config, tmpstring.source, tmpstring.length);
	done_string(&tmpstring);

get_me_out:
	unmark_options_tree(options->ptr);

	return config.source;
}

/* TODO: The error condition should be handled somewhere else. */
static int
write_config_file(unsigned char *prefix, unsigned char *name,
		  struct option *options, struct terminal *term)
{
	int ret = 0;
	struct secure_save_info *ssi;
	unsigned char *config_file;
	unsigned char *cfg_str = create_config_string(prefix, name, options);

	if (!cfg_str) return -1;

	config_file = straconcat(prefix, "/", name, NULL);
	if (!config_file) goto free_cfg_str;

	ssi = secure_open(config_file, 0177);
	if (ssi) {
		secure_fputs(ssi, cfg_str);
		ret = secure_close(ssi);
	}

	if (term && (secsave_errno != SS_ERR_NONE || ret)) {
		unsigned char *errmsg = NULL;
		unsigned char *strerr;

		switch (secsave_errno) {
			case SS_ERR_DISABLED:
				strerr = _("File saving disabled by option", term);
				break;
			case SS_ERR_OUT_OF_MEM:
				strerr = _("Out of memory", term);
				break;
			case SS_ERR_OPEN_WRITE:
				strerr = _("Cannot write the file", term);
				break;
			default:
				strerr = _("Secure open failed", term);
				break;
		}

		if (ret > 0) errmsg = straconcat(strerr, " (", strerror(ret), ")", NULL);

		if (errmsg) {
			write_config_error(term, getml(config_file, errmsg, NULL),
					   config_file, errmsg);
		} else {
			write_config_error(term, getml(config_file, NULL),
					   config_file, strerr);
		}

		goto free_cfg_str;
	}

	mem_free(config_file);

free_cfg_str:
	mem_free(cfg_str);

	return ret;
}

void
write_config(struct terminal *term)
{
	write_config_file(elinks_home, "elinks.conf", config_options, term);
}
