/* Config file manipulation */
/* $Id: conf.c,v 1.23 2002/05/23 20:44:54 pasky Exp $ */

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
#include "config/options.h"
#include "config/opttypes.h"
#include "intl/language.h"
#include "lowlevel/home.h"
#include "lowlevel/terminal.h"
#include "util/secsave.h"


/* Config file has only very simple grammar:
 * 
 * /set option *= *value;/
 * /#.*$/
 * 
 * Where option consists from any number of categories separated by dots and
 * name of the option itself. Both category and option name consists from
 * [a-zA-Z0-9_-] - using uppercase letters is not recommended, though.
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


/* Skip block of whitespaces. */
unsigned char *
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

/* Parse 'set' command. Returns 0 if error, 1 if ok. */
int
parse_set(unsigned char **optname, int *optlen, unsigned char **optval,
	  int *line)
{
	unsigned char *ptr = *optname;

	ptr = skip_white(ptr, line);
	if (!*ptr) return 0;

	/* Option name */
	*optname = ptr;
	while (isA(*ptr) || *ptr == '.') ptr++;
	*optlen = ptr - *optname;

	/* Equal sign */
	ptr = skip_white(ptr, line);
	if (*(ptr++) != '=') return 0;

	/* Option value */
	ptr = skip_white(ptr, line);
	if (!*ptr) return 0;
	*optval = ptr;

	return 1;
}

void
parse_config_file(unsigned char *name, unsigned char *file,
		  struct list_head *opt_tree)
{
	int line = 1;
	int error_occured = 0;
	enum {
		ERROR_NONE,
		ERROR_PARSE,
		ERROR_OPTION,
		ERROR_VALUE,
	} error = 0;
	unsigned char error_msg[][80] = {
		"no error",
		"parse error",
		"unknown option",
		"bad value",
	};

	while (file && *file) {
		/* Skip all possible comments and whitespaces. */
		file = skip_white(file, &line);

		/* Second chance to escape from the hell. */
		if (!*file) break;

		/* TODO: This should be done in a more generic way, maintaining
		 * table of handlers for each command. Definitively overkill
		 * when we support only one command ;-). --pasky */

		if (!strncmp(file, "set", 3) && WHITECHAR(file[3])) {
			unsigned char *optname = file + 3;
			int optname_l = 0;
			unsigned char *optval;

			if (parse_set(&optname, &optname_l, &optval, &line)) {
				unsigned char *oname;
				struct option *opt;

				/* FIXME: By the time when I write it, I
				 * already dislike it. However I just want to
				 * get it done now, we should move this stuff
				 * to separate function Later (tm). --pasky */

				oname = memacpy(optname, optname_l);
				opt = get_opt_rec(opt_tree, oname);

				if (opt) {
					if (!option_types[opt->type].read(opt, &optval))
						error = ERROR_VALUE;
					file = optval;
				} else {
					error = ERROR_OPTION;
				}

				mem_free(oname);
			} else {
				error = ERROR_PARSE;
			}
		} else {
			error = ERROR_PARSE;
			/* Jump over this crap we can't understand. */
			while (!WHITECHAR(*file) && *file != '#' && *file)
				file++;
		}

		if (error) {
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
create_config_string(struct list_head *options)
{
	unsigned char *str = init_str();
	int len = 0;
	struct option *option;

	add_to_str(&str, &len,
		   "# This file is automatically generated by Links "
		   "-- please DO NOT edit!!" NEWLINE);

	foreach (option, *options) {
		if (option_types[option->type].write
		    && option->flags & OPT_CFGFILE) {
			add_to_str(&str, &len, "set ");
			add_to_str(&str, &len, option->name);
			add_to_str(&str, &len, " = ");
			option_types[option->type].write(option, &str, &len);
			add_to_str(&str, &len, NEWLINE);
		}
	}

	return str;
}

/* TODO: The error condition should be handled somewhere else. */
int
write_config_file(unsigned char *prefix, unsigned char *name, struct list_head *o,
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
