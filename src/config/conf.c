/* Config file and commandline proccessing */
/* $Id: conf.c,v 1.16 2002/05/20 15:28:29 pasky Exp $ */

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

#include "links.h"

#include "bfu/align.h"
#include "bfu/bfu.h"
#include "config/conf.h"
#include "config/kbdbind.h"
#include "config/options.h"
#include "intl/language.h"
#include "lowlevel/home.h"
#include "lowlevel/terminal.h"
#include "util/hash.h"
#include "util/secsave.h"


/* Config file has only very simple grammar:
 * 
 * /^set option *= *value;$/
 * /^#.*$/
 * 
 * Where option consists from any number of categories separated by dots and
 * name of the option itself. Both category and option name consists from
 * [a-zA-Z0-9_] - using uppercase letters is not recommended, though.
 * 
 * Value can consist from:
 * - number (it will be converted to int/long)
 * - enum (like on, off; true, fake, last_url; etc ;) - in planning state yet
 * - string - "blah blah"
 * 
 * "set" command is parsed first, and then type-specific function is called,
 * with option as one parameter and value as a second. Usually it just assigns
 * value to an option, but sometimes you may want to first create the option
 * ;). Then this will come handy. */


/* TODO: This ought to be rewritten - we want special hash for commandline
 * options as "aliases" there. */
unsigned char *
_parse_options(int argc, unsigned char *argv[], struct hash *opt)
{
	unsigned char *location = NULL;

	while (argc) {
		argv++, argc--;

		if (argv[-1][0] == '-') {
			struct option *option;
			unsigned char *argname = &argv[-1][1];
			unsigned char *oname = opt_name(argname);

			/* Treat --foo same as -foo. */
			if (argname[0] == '-') argname++;

			option = get_opt_rec(opt, argname);
			if (!option && oname)
				option = get_opt_rec(opt, oname);

			mem_free(oname);

			if (!option)
				continue;

			if (option_types[option->type].rd_cmd
			    && option->flags & OPT_CMDLINE) {
				unsigned char *err;

				err = option_types[option->type].rd_cmd(option, &argv, &argc);

				if (err) {
					if (err[0])
						fprintf(stderr, "Error parsing option %s: %s\n", argv[-1], err);

					return NULL;
				}

				goto found;
			}

			goto unknown_option;

		} else if (!location) {
			location = argv[-1];

		} else {
unknown_option:		fprintf(stderr, "Unknown option %s\n", argv[-1]);

			return NULL;
		}

found:
	}

	return location ? location : (unsigned char *) "";
}

unsigned char *
parse_options(int argc, unsigned char *argv[])
{
	return _parse_options(argc, argv, root_options);
}


/* TODO: This ought to disappear. */
unsigned char *
get_token(unsigned char **line)
{
	unsigned char *s = NULL;
	int l = 0;
	int escape = 0;
	int quote = 0;

	while (**line == ' ' || **line == 9) (*line)++;
	if (**line) {
		for (s = init_str(); **line; (*line)++) {
			if (escape)
				escape = 0;
			else if (**line == '\\') {
				escape = 1;
				continue;
			}
			else if (**line == '"') {
				quote = !quote;
			    	continue;
			}
			else if ((**line == ' ' || **line == 9) && !quote)
				break;
			add_chr_to_str(&s, &l, **line);
		}
	}
	return s;
}

/* TODO: New config file format. */
void
parse_config_file(unsigned char *name, unsigned char *file, struct hash *opt)
{
	int error = 0;
	int line = 0;

	while (file[0]) {
		struct option *option;
		unsigned char *oname;
		unsigned char *id, *val, *tok = NULL;
		int id_len, val_len, tok_len;

		/* New line */
		line++;
		while (file[0] && (file[0] == ' ' || file[0] == 9)) file++;

		/* Get identifier */
		id = file;
		while (file[0] && file[0] > ' ') file++;
		id_len = file - id;

		/* No identifier? */
		if (! id_len) {
			if (file[0]) file++;
			continue;
		}

		/* Skip separator */
		while (file[0] == 9 || file[0] == ' ') file++;

		/* Get value */
		val = file;
		while (file[0] && file[0] != 10 && file[0] != 13) file++;
		val_len = file - val;

		/* Possibly move to new line */
		if (file[0]) {
			if ((file[1] == 10 || file[1] == 13) && file[0] != file[1]) file++;
			file++;
		}

		/* Comment? */
		if (id[0] == '#') continue;

		/* Get token or go on */
		tok = get_token(&id);
		if (!tok) continue;

		/* TODO: Move following to separate function. */

		tok_len = strlen(tok);
		oname = mem_alloc(tok_len + 1);
		safe_strncpy(oname, tok, tok_len + 1);
		option = get_opt_rec(opt, oname);
		mem_free(oname);

		if (!option)
			continue;

		if (option->flags & OPT_CFGFILE) {
			unsigned char *value = memacpy(val, val_len);
			unsigned char *err = option_types[option->type].rd_cfg(option, value);

			if (err) {
				if (err[0])
					fprintf(stderr, "Error parsing config file %s, line %d: %s\n",
						name, line, err);
				error = 1;
			}

			mem_free(value);
			goto next;
		}

		fprintf(stderr, "Unknown option in config file %s, line %d\n", name, line);
		error = 1;
next:
		if (tok) mem_free(tok);
	}

	if (error) {
		fprintf(stderr, "\007");
		sleep(3);
	}
}


unsigned char *
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

void
load_config_file(unsigned char *prefix, unsigned char *name)
{
	unsigned char *config_str, *config_file;

	config_file = straconcat(prefix, name, NULL);
	if (!config_file) return;

	config_str = read_config_file(config_file);
	if (!config_str) {
		mem_free(config_file);
		config_file = straconcat(prefix, ".", name, NULL);
		if (!config_file) return;

		config_str = read_config_file(config_file);
		if (!config_str) {
			mem_free(config_file);
			return;
		}
	}

	parse_config_file(config_file, config_str, root_options);

	mem_free(config_str);
	mem_free(config_file);
}

void
load_config()
{
	load_config_file("/etc/elinks/", "elinks.conf");
	load_config_file(elinks_home, "elinks.conf");
}


/* TODO: We want to get rid of user.cfg. Let's rewrite config file
 * non-destructively. */
unsigned char *
create_config_string(struct hash *options)
{
	unsigned char *str = init_str();
	int len = 0;
	struct hash_item *item;
	int i;

	add_to_str(&str, &len,
		   "# This file is automatically generated by Links "
		   "-- please DO NOT edit!!" NEWLINE
		   "# For own options (like keybindings), use user.cfg.");

	foreach_hash_item (options, item, i) {
		struct option *option = item->value;

		if (option_types[option->type].wr_cfg
		    && option->flags & OPT_CFGFILE) {
			option_types[option->type].wr_cfg(option, &str, &len);
		}
	}

	add_to_str(&str, &len, NEWLINE);

	return str;
}

/* TODO: The error condition should be handled somewhere else. */
int
write_config_file(unsigned char *prefix, unsigned char *name, struct hash *o,
		  struct terminal *term)
{
	int ret = -1;
	struct secure_save_info *ssi;
	unsigned char *config_file;
	unsigned char *cfg_str = create_config_string(o);

	if (!cfg_str) return -1;
	
	config_file = straconcat(prefix, name, NULL);
	if (!config_file) goto free_cfg_str;
	
	ssi = secure_open(config_file, 0177);
	if (!ssi) goto free_config_file;

	secure_fputs(ssi, cfg_str);
	ret = secure_close(ssi);

	if (ret && term) {
		msg_box(term, NULL,
			TEXT(T_CONFIG_ERROR), AL_CENTER | AL_EXTD_TEXT,
			TEXT(T_UNABLE_TO_WRITE_TO_CONFIG_FILE), "\n",
			config_file, ": ", strerror(ret), NULL,
			NULL, 1,
			TEXT(T_CANCEL), NULL, B_ENTER | B_ESC);
	}

free_config_file:
	mem_free(config_file);
	
free_cfg_str:
	mem_free(cfg_str);

	return ret;
}

void
write_config(struct terminal *term)
{
	write_config_file(elinks_home, "elinks.conf", root_options, term);
}
