/* Config file manipulation */
/* $Id: conf.c,v 1.61 2002/12/08 20:41:32 pasky Exp $ */

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
#include "lowlevel/home.h"
#include "lowlevel/terminal.h"
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
	  unsigned char **str, int *len)
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
	if (str) add_bytes_to_str(str, len, orig_pos, *file - orig_pos);

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
		if (str) {
			opt->flags |= OPT_WATERMARK;
			if (option_types[opt->type].write)
				option_types[opt->type].write(opt, str, len);
		} else if (!val || !option_types[opt->type].set
			   || !option_types[opt->type].set(opt, val)) {
			if (val) mem_free(val);
			return ERROR_VALUE;
		}
		if (val) mem_free(val);
	}

	return ERROR_NONE;
}

static enum parse_error
parse_bind(struct option *opt_tree, unsigned char **file, int *line,
	   unsigned char **str, int *len)
{
	unsigned char *orig_pos = *file;
	unsigned char *keymap, *keystroke, *action;
	enum parse_error error = ERROR_NONE;

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

	/* Mirror what we already have */
	if (str) add_bytes_to_str(str, len, orig_pos, *file - orig_pos);

	/* Action */
	action = option_types[OPT_STRING].read(NULL, file);
	if (!action) {
		mem_free(keymap);
		return ERROR_VALUE;
	}

	if (str) {
		bind_act(str, len, keymap, keystroke);
	} else {
		error = bind_do(keymap, keystroke, action) ? ERROR_VALUE : ERROR_NONE;
	}
	mem_free(keymap); mem_free(keystroke); mem_free(action);
	return error;
}

static
int load_config_file(unsigned char *, unsigned char *, struct option *,
		     unsigned char **, int *);

static enum parse_error
parse_include(struct option *opt_tree, unsigned char **file, int *line,
	      unsigned char **str, int *len)
{
	unsigned char *orig_pos = *file;
	unsigned char *fname;
	unsigned char *dumbstr = init_str();
	int dumblen = 0;

	if (!dumbstr) return ERROR_NOMEM;

	*file = skip_white(*file, line);
	if (!*file) return ERROR_PARSE;

	/* File name */
	fname = option_types[OPT_STRING].read(NULL, file);
	if (!fname)
		return ERROR_VALUE;

	/* Mirror what we already have */
	if (str) add_bytes_to_str(str, len, orig_pos, *file - orig_pos);

	/* We want load_config_file() to watermark stuff, but not to load
	 * anything, polluting our beloved options tree - thus, we will feed it
	 * with some dummy string which we will destroy later; still better
	 * than cloning whole options tree or polluting interface with another
	 * rarely-used option ;). */
	/* XXX: We should try CONFDIR/<file> when proceeding
	 * CONFDIR/<otherfile> ;). --pasky */
	if (load_config_file(fname[0] == '/' ? (unsigned char *) ""
					     : elinks_home,
			     fname, opt_tree, &dumbstr, &dumblen)) {
		mem_free(dumbstr);
		mem_free(fname);
		return ERROR_VALUE;
	}

	mem_free(dumbstr);
	mem_free(fname);
	return ERROR_NONE;
}


struct parse_handler {
	unsigned char *command;
	enum parse_error (*handler)(struct option *opt_tree,
				    unsigned char **file, int *line,
				    unsigned char **str, int *len);
};

static struct parse_handler parse_handlers[] = {
	{ "set", parse_set },
	{ "bind", parse_bind },
	{ "include", parse_include },
	{ NULL, NULL }
};


void
parse_config_file(struct option *options, unsigned char *name,
		  unsigned char *file, unsigned char **str, int *len)
{
	int line = 1;
	int error_occured = 0;
	enum parse_error error = 0;
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
		if (str) add_bytes_to_str(str, len, orig_pos, file - orig_pos);

		/* Second chance to escape from the hell. */
		if (!*file) break;

		{
			struct parse_handler *handler;

			for (handler = parse_handlers; handler->command;
			     handler++) {
				int cmdlen = strlen(handler->command);

				if (!strncmp(file, handler->command, cmdlen)
				    && WHITECHAR(file[cmdlen])) {
					/* Mirror what we already have */
					if (str)
						add_bytes_to_str(str, len,
								 file, cmdlen);

					file += cmdlen;
					error = handler->handler(options,
								 &file, &line,
								 str, len);
					goto test_end;
				}
			}
		}

		error = ERROR_COMMAND;
		orig_pos = file;
		/* Jump over this crap we can't understand. */
		while (!WHITECHAR(*file) && *file != '#' && *file)
			file++;

		/* Mirror what we already have */
		if (str) add_bytes_to_str(str, len, orig_pos, file - orig_pos);

test_end:

		if (!str && error) {
			/* TODO: Make this a macro and report error directly
			 * as it's stumbled upon; line info may not be accurate
			 * anymore now (?). --pasky */
			fprintf(stderr, "%s:%d: %s\n",
				name, line, error_msg[error]);
			error_occured = 1;
			error = 0;
		}
	}

	if (error_occured) {
		fprintf(stderr, "\007");
		sleep(1);
	}
}



static unsigned char *
read_config_file(unsigned char *name)
{
#define FILE_BUF	1024
	unsigned char cfg_buffer[FILE_BUF];
	unsigned char *s;
	int l = 0;
	int fd, r;

	fd = open(name, O_RDONLY | O_NOCTTY);
	if (fd < 0) return NULL;
	set_bin(fd);

	s = init_str();
	if (!s) return NULL;

	while ((r = read(fd, cfg_buffer, FILE_BUF)) > 0) {
		int i;

		/* Clear problems ;). */
		for (i = 0; i < r; i++)
			if (!cfg_buffer[i])
				cfg_buffer[i] = ' ';

		add_bytes_to_str(&s, &l, cfg_buffer, r);
	}

	if (r < 0) {
		mem_free(s);
		s = NULL;
	}

	close(fd);

	return s;
#undef FILE_BUF
}

/* Return 0 on success. */
static int
load_config_file(unsigned char *prefix, unsigned char *name,
		 struct option *options, unsigned char **str, int *len)
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

	parse_config_file(options, config_file, config_str, str, len);

	mem_free(config_str);
	mem_free(config_file);

	return 0;
}

void
load_config()
{
	load_config_file(CONFDIR, "elinks.conf",
			 &root_options, NULL, NULL);
	load_config_file(elinks_home, "elinks.conf",
			 &root_options, NULL, NULL);
}


static void
smart_config_output_fn(unsigned char **str, int *len, struct option *option,
		       unsigned char *path, int depth, int do_print_comment,
		       int action)
{
	int i, j, l;

	if (!option_types[option->type].write)
		return;

	switch (action) {
		case 0:
			for (i = 0; i < depth * 2; i++)
				add_chr_to_str(str, len, ' ');

			add_to_str(str, len, "## ");
			if (path) {
				add_to_str(str, len, path);
				add_to_str(str, len, ".");
			}
			add_to_str(str, len, option->name);
			add_to_str(str, len, " ");
			add_to_str(str, len, option_types[option->type].help_str);
			add_to_str(str, len, NEWLINE);
			break;

		case 1:
			if (!option->desc || !do_print_comment)
				break;

			l = strlen(option->desc);

			for (i = 0; i < depth * 2; i++)
				add_chr_to_str(str, len, ' ');
			add_to_str(str, len, "# ");

			for (i = 0; i < l; i++) {
				if (option->desc[i] == '\n') {
					add_to_str(str, len, NEWLINE);
					for (j = 0; j < depth * 2; j++)
						add_chr_to_str(str, len, ' ');
					add_to_str(str, len, "# ");
				} else {
					add_chr_to_str(str, len,
							option->desc[i]);
				}
			}

			add_to_str(str, len, NEWLINE);
			break;

		case 2:
			for (j = 0; j < depth * 2; j++)
				add_chr_to_str(str, len, ' ');
			add_to_str(str, len, "set ");
			if (path) {
				add_to_str(str, len, path);
				add_to_str(str, len, ".");
			}
			add_to_str(str, len, option->name);
			add_to_str(str, len, " = ");
			option_types[option->type].write(option, str, len);
			add_to_str(str, len, NEWLINE);
			if (do_print_comment) add_to_str(str, len, NEWLINE);
			break;

		case 3:
			if (do_print_comment < 2)
				add_to_str(str, len, NEWLINE);
			break;
	}
}

static unsigned char *
create_config_string(unsigned char *prefix, unsigned char *name,
		     struct option *options)
{
	unsigned char *str = init_str();
	int len = 0;
	/* Don't write headers if nothing will be added anyway. */
	unsigned char *tmpstr;
	int tmplen;
	int origlen;
	int savestyle = get_opt_int("config_saving_style");

	if (!str) return NULL;

	/* Scaring. */
	if (savestyle == 2 || (savestyle < 2
				&& (load_config_file(prefix, name, options,
						     &str, &len)
				    || !*str))) {
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

		add_to_str(&str, &len, headings[savestyle]);
		add_to_str(&str, &len,
			"##" NEWLINE
			"## Obviously, if you don't like what ELinks is going to do with" NEWLINE
			"## this file, you can change it by altering the config_saving_style" NEWLINE
			"## option. Come on, aren't we friendly guys after all?" NEWLINE);
	}

	if (savestyle == 0)
		goto get_me_out;

	tmpstr = init_str(); tmplen = 0;
	add_to_str(&tmpstr, &tmplen, NEWLINE NEWLINE NEWLINE);
	add_to_str(&tmpstr, &tmplen, "#####################################" NEWLINE);
	add_to_str(&tmpstr, &tmplen, "# Automatically saved options" NEWLINE);
	add_to_str(&tmpstr, &tmplen, "#" NEWLINE);
	add_to_str(&tmpstr, &tmplen, NEWLINE);

	origlen = tmplen;
	smart_config_string(&tmpstr, &tmplen, 2, options->ptr, NULL, 0, smart_config_output_fn);
	if (tmplen > origlen) add_bytes_to_str(&str, &len, tmpstr, tmplen);
	mem_free(tmpstr);

	tmpstr = init_str(); tmplen = 0;
	add_to_str(&tmpstr, &tmplen, NEWLINE NEWLINE NEWLINE);
	add_to_str(&tmpstr, &tmplen, "#####################################" NEWLINE);
	add_to_str(&tmpstr, &tmplen, "# Automatically saved keybindings" NEWLINE);
	add_to_str(&tmpstr, &tmplen, "#" NEWLINE);

	origlen = tmplen;
	bind_config_string(&tmpstr, &tmplen);
	if (tmplen > origlen) add_bytes_to_str(&str, &len, tmpstr, tmplen);
	mem_free(tmpstr);

get_me_out:
	unmark_options_tree(options->ptr);

	return str;
}

/* TODO: The error condition should be handled somewhere else. */
static int
write_config_file(unsigned char *prefix, unsigned char *name,
		  struct option *options, struct terminal *term)
{
	int ret = -1;
	struct secure_save_info *ssi;
	unsigned char *config_file;
	unsigned char *cfg_str;

	cfg_str = create_config_string(prefix, name, options);

	if (!cfg_str) return -1;

	config_file = straconcat(prefix, "/", name, NULL);
	if (!config_file) goto free_cfg_str;

	ssi = secure_open(config_file, 0177);
	if (!ssi) goto free_config_file;

	secure_fputs(ssi, cfg_str);
	ret = secure_close(ssi);

	if (ret && term)
		write_config_error(term, config_file, ret);

free_config_file:
	mem_free(config_file);

free_cfg_str:
	mem_free(cfg_str);

	return ret;
}

void
write_config(struct terminal *term)
{
	write_config_file(elinks_home, "elinks.conf", &root_options, term);
}
