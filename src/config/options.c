/* Options list and handlers and interface */
/* $Id: options.c,v 1.28 2002/05/19 19:34:57 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h> /* OS/2 needs this after sys/types.h */
#include <sys/stat.h> /* OS/2 needs this after sys/types.h */
#include <netdb.h>

/* We need to have it here. Stupid BSD. */
#include <netinet/in.h>
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include "links.h"

#include "config/conf.h"
#include "config/kbdbind.h"
#include "config/options.h"
#include "document/options.h"
#include "document/html/colors.h"
#include "intl/charsets.h"
#include "intl/language.h"
#include "lowlevel/dns.h"
#include "protocol/types.h"
#include "util/error.h"
#include "util/hash.h"


struct hash *root_options;

/**********************************************************************
 Options interface
**********************************************************************/

/* Note that this is only a base for hiearchical options, which will be much
 * more fun. This part of code is under heavy development, so please treat
 * with care. --pasky, 20020428 ;) */

/* If option name contains dots, they are created as "categories" - first,
 * first category is retrieeved from hash, taken as a hash, second category
 * is retrieved etc. */

/* Get record of option of given name, or NULL if there's no such option. */
struct option *
get_opt_rec(struct hash *hash, unsigned char *_name)
{
	struct hash_item *item;
	unsigned char *aname = stracpy(_name);
	unsigned char *name = aname;
	unsigned char *sep;

	while ((sep = strchr(name, '|'))) {
		*sep = 0;

		item = get_hash_item(hash, name);
		if (!item) {
			mem_free(aname);
			return NULL;
		}

		hash = (struct hash *) item->value;

		name = sep + 1;
	}

	item = get_hash_item(hash, name);
	mem_free(aname);
	if (!item) return NULL;

	return (struct option *) item->value;
}

/* Fetch pointer to value of certain option. It is guaranteed to never return
 * NULL. */
void *
get_opt(struct hash *hash, unsigned char *name)
{
	struct option *opt = get_opt_rec(hash, name);

	if (!opt) internal("Attempted to fetch unexistent option %s!", name);
	if (!opt->ptr) internal("Option %s has no value!", name);
	return opt->ptr;
}

/* Add option to hash. */
void
add_opt_rec(struct hash *hash, unsigned char *path, struct option *option)
{
	struct option *aopt = mem_alloc(sizeof(struct option));
	unsigned char *aname = stracpy(path);
	unsigned char *name = aname;
	unsigned char *sep;

	if (!aopt) return;
	memcpy(aopt, option, sizeof(struct option));

	while (*name) {
		struct hash_item *item;

		/* We take even the last element of path (ended not by '.'
		 * but by '\0'). */
		sep = strchr(name, '|');
		if (sep) *sep = 0;
		else sep = name + strlen(name) - 1;

		item = get_hash_item(hash, name);
		if (!item) {
			mem_free(aname);
			mem_free(aopt);
			return;
		}

		hash = (struct hash *) item->value;

		name = sep + 1;
	}
	mem_free(aname);

	add_hash_item(hash, stracpy(option->name), aopt);
}

void
add_opt(struct hash *hash, unsigned char *path, unsigned char *name,
	enum option_flags flags, enum option_type type,
	int min, int max, void *ptr,
	unsigned char *desc)
{
	struct option *option = mem_alloc(sizeof(struct option));

	option->name = name;
	option->flags = flags;
	option->type = type;
	option->min = min;
	option->max = max;
	option->ptr = ptr;
	option->desc = desc;

	add_opt_rec(hash, path, option);

	mem_free(option);
}


void register_options();

struct hash *
init_options_hash()
{
	/* 6 bits == 64 entries; I guess it's the best number for options
	 * hash. --pasky */
	struct hash *hash = init_hash(6);

	return hash;
}

void
init_options()
{
	root_options = init_options_hash();
	register_options();
}

void
free_options_hash(struct hash *hash)
{
	struct hash_item *item;
	int i;

	foreach_hash_item (hash, item, i) {
		struct option *option = item->value;

		if (option->type == OPT_BOOL ||
		    option->type == OPT_INT ||
		    option->type == OPT_LONG ||
		    option->type == OPT_STRING ||
		    option->type == OPT_CODEPAGE)
			mem_free(option->ptr);

		else if (option->type == OPT_HASH)
			free_options_hash((struct hash *) option->ptr);
	}

	free_hash(hash);
}

void
done_options()
{
	free_options_hash(root_options);
}


/* Get command-line alias for option name */
unsigned char *
cmd_name(unsigned char *name)
{
	unsigned char *cname = stracpy(name);
	unsigned char *ptr;

	for (ptr = cname; *ptr; ptr++) {
		if (*ptr == '_') *ptr = '-';
	}

	return cname;
}

/* Get option name from command-line alias */
unsigned char *
opt_name(unsigned char *name)
{
	unsigned char *cname = stracpy(name);
	unsigned char *ptr;

	for (ptr = cname; *ptr; ptr++) {
		if (*ptr == '-') *ptr = '_';
	}

	return cname;
}


/**********************************************************************
 Options handlers
**********************************************************************/

void add_nm(struct option *o, unsigned char **s, int *l)
{
	if (*l) add_to_str(s, l, NEWLINE);
	add_to_str(s, l, o->name);
	add_to_str(s, l, " ");
}

void add_quoted_to_str(unsigned char **s, int *l, unsigned char *q)
{
	add_chr_to_str(s, l, '"');
	while (*q) {
		if (*q == '"' || *q == '\\') add_chr_to_str(s, l, '\\');
		add_chr_to_str(s, l, *q);
		q++;
	}
	add_chr_to_str(s, l, '"');
}

/* If 0 follows, disable option and eat 0. If 1 follows, enable option and
 * eat 1. If anything else follow, enable option and don't eat anything. */
unsigned char *
bool_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	*((int *) o->ptr) = 1;

	if (!*argc) return NULL;

	/* Argument is empty or longer than 1 char.. */
	if (!(*argv)[0][0] || !(*argv)[0][1]) return NULL;

	switch ((*argv)[0][0] == '0') {
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

unsigned char *num_rd(struct option *o, unsigned char *c)
{
	unsigned char *tok = get_token(&c);
	unsigned char *end;
	long l;
	if (!tok) return "Missing argument";
	l = strtolx(tok, &end);
	if (*end) {
		mem_free(tok);
		return "Number expected";
	}
	if (l < o->min || l > o->max) {
		mem_free(tok);
		return "Out of range";
	}
	*((int *) o->ptr) = l;
	mem_free(tok);
	return NULL;
}

void num_wr(struct option *o, unsigned char **s, int *l)
{
	add_nm(o, s, l);
	add_knum_to_str(s, l, *((int *) o->ptr));
}

unsigned char *str_rd(struct option *o, unsigned char *c)
{
	unsigned char *tok = get_token(&c);
	unsigned char *e = NULL;
	if (!tok) return NULL;
	if (strlen(tok) + 1 > o->max) e = "String too long";
	else { mem_free(o->ptr); o->ptr = stracpy(tok); }
	mem_free(tok);
	return e;
}

void str_wr(struct option *o, unsigned char **s, int *l)
{
	add_nm(o, s, l);
	if (strlen(o->ptr) > o->max - 1) {
		unsigned char *s1 = init_str();
		int l1 = 0;
		add_bytes_to_str(&s1, &l1, o->ptr, o->max - 1);
		add_quoted_to_str(s, l, s1);
		mem_free(s1);
	}
	else add_quoted_to_str(s, l, o->ptr);
}

unsigned char *cp_rd(struct option *o, unsigned char *c)
{
	unsigned char *tok = get_token(&c);
	unsigned char *e = NULL;
	int i;
	if (!tok) return "Missing argument";
	/*if (!strcasecmp(c, "none")) i = -1;
	else */if ((i = get_cp_index(tok)) == -1) e = "Unknown codepage";
	else *((int *) o->ptr) = i;
	mem_free(tok);
	return e;
}

void cp_wr(struct option *o, unsigned char **s, int *l)
{
	unsigned char *n = get_cp_mime_name(*(int *)o->ptr);
	add_nm(o, s, l);
	add_to_str(s, l, n);
}

unsigned char *lang_rd(struct option *o, unsigned char *c)
{
	int i;
	unsigned char *tok = get_token(&c);
	if (!tok) return "Missing argument";
	for (i = 0; i < n_languages(); i++)
		if (!(strcasecmp(language_name(i), tok))) {
			set_language(i);
			mem_free(tok);
			return NULL;
		}
	mem_free(tok);
	return "Unknown language";
}

void lang_wr(struct option *o, unsigned char **s, int *l)
{
	add_nm(o, s, l);
	add_quoted_to_str(s, l, language_name(current_language));
}

int getnum(unsigned char *s, int *n, int r1, int r2)
{
	unsigned char *e;
	long l = strtol(s, (char **)&e, 10);
	if (*e || !*s) return -1;
	if (l < r1 || l >= r2) return -1;
	*n = (int)l;
	return 0;
}

unsigned char *type_rd(struct option *o, unsigned char *c)
{
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
}

void type_wr(struct option *o, unsigned char **s, int *l)
{
	struct assoc *a;
	foreachback(a, assoc) {
		add_nm(o, s, l);
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
}

unsigned char *ext_rd(struct option *o, unsigned char *c)
{
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
}

void ext_wr(struct option *o, unsigned char **s, int *l)
{
	struct extension *a;
	foreachback(a, extensions) {
		add_nm(o, s, l);
		add_quoted_to_str(s, l, a->ext);
		add_to_str(s, l, " ");
		add_quoted_to_str(s, l, a->ct);
	}
}

unsigned char *prog_rd(struct option *o, unsigned char *c)
{
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
}

void prog_wr(struct option *o, unsigned char **s, int *l)
{
	struct protocol_program *a;
	foreachback(a, *(struct list_head *)o->ptr) {
		if (!*a->prog) continue;
		add_nm(o, s, l);
		add_quoted_to_str(s, l, a->prog);
		add_to_str(s, l, " ");
		add_num_to_str(s, l, a->system);
	}
}

/* terminal NAME(str) MODE(0-3) M11_HACK(0-1) BLOCK_CURSOR.RESTRICT_852.COL(0-7) CHARSET(str) [ UTF_8_IO("utf-8") ]*/
unsigned char *term_rd(struct option *o, unsigned char *c)
{
	struct term_spec *ts;
	unsigned char *w;
	int i;
	if (!(w = get_token(&c))) goto err;
	if (!(ts = new_term_spec(w))) {
		mem_free(w);
		goto end;
	}
	ts->utf_8_io = 0;
	mem_free(w);
	if (!(w = get_token(&c))) goto err;
	if (strlen(w) != 1 || w[0] < '0' || w[0] > '3') goto err_f;
	ts->mode = w[0] - '0';
	mem_free(w);
	if (!(w = get_token(&c))) goto err;
	if (strlen(w) != 1 || w[0] < '0' || w[0] > '1') goto err_f;
	ts->m11_hack = w[0] - '0';
	mem_free(w);
	if (!(w = get_token(&c))) goto err;
	if (strlen(w) != 1 || w[0] < '0' || w[0] > '7') goto err_f;
	ts->col = (w[0] - '0') & 1;
	ts->restrict_852 = !!((w[0] - '0') & 2);
	ts->block_cursor = !!((w[0] - '0') & 4);
	mem_free(w);
	if (!(w = get_token(&c))) goto err;
	if ((i = get_cp_index(w)) == -1) goto err_f;
	ts->charset = i;
	mem_free(w);
	if (!(w = get_token(&c))) goto end;
	if (!(strcasecmp(w, "utf-8"))) ts->utf_8_io = 1;
	mem_free(w);
	end:
	return NULL;
	err_f:
	mem_free(w);
	err:
	return "Error reading terminal specification";
}

/* terminal2 NAME(str) MODE(0-3) M11_HACK(0-1) RESTRICT_852(0-1) COL(0-1) CHARSET(str) [ UTF_8_IO("utf-8") ]*/
unsigned char *term2_rd(struct option *o, unsigned char *c)
{
	struct term_spec *ts;
	unsigned char *w;
	int i;
	if (!(w = get_token(&c))) goto err;
	if (!(ts = new_term_spec(w))) {
		mem_free(w);
		goto end;
	}
	ts->utf_8_io = 0;
	mem_free(w);
	if (!(w = get_token(&c))) goto err;
	if (strlen(w) != 1 || w[0] < '0' || w[0] > '3') goto err_f;
	ts->mode = w[0] - '0';
	mem_free(w);
	if (!(w = get_token(&c))) goto err;
	if (strlen(w) != 1 || w[0] < '0' || w[0] > '1') goto err_f;
	ts->m11_hack = w[0] - '0';
	mem_free(w);
	if (!(w = get_token(&c))) goto err;
	if (strlen(w) != 1 || w[0] < '0' || w[0] > '1') goto err_f;
	ts->restrict_852 = w[0] - '0';
	mem_free(w);
	if (!(w = get_token(&c))) goto err;
	if (strlen(w) != 1 || w[0] < '0' || w[0] > '1') goto err_f;
	ts->col = w[0] - '0';
	mem_free(w);
	if (!(w = get_token(&c))) goto err;
	if ((i = get_cp_index(w)) == -1) goto err_f;
	ts->charset = i;
	mem_free(w);
	if (!(w = get_token(&c))) goto end;
	if (!(strcasecmp(w, "utf-8"))) ts->utf_8_io = 1;
	mem_free(w);
	end:
	return NULL;
	err_f:
	mem_free(w);
	err:
	return "Error reading terminal specification";
}

void term_wr(struct option *o, unsigned char **s, int *l)
{
	struct term_spec *ts;
	foreachback(ts, term_specs) {
		add_nm(o, s, l);
		add_quoted_to_str(s, l, ts->term);
		add_to_str(s, l, " ");
		add_num_to_str(s, l, ts->mode);
		add_to_str(s, l, " ");
		add_num_to_str(s, l, ts->m11_hack);
		add_to_str(s, l, " ");
		add_num_to_str(s, l, !!ts->col + !!ts->restrict_852 * 2 + !!ts->block_cursor * 4);
		add_to_str(s, l, " ");
		add_to_str(s, l, get_cp_mime_name(ts->charset));
		if (ts->utf_8_io) {
			add_to_str(s, l, " utf-8");
		}
	}
}

unsigned char *color_rd(struct option *o, unsigned char *c)
{
	unsigned char *val = get_token(&c);

	if (!val) {
		return "Missing argument";
	} else {
		int err = decode_color(val, o->ptr);

		mem_free(val);
		return (err) ? "Error decoding color" : NULL;
	}
}

unsigned char *gen_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	unsigned char *r;
	if (!*argc) return "Parameter expected";
	(*argv)++; (*argc)--;
	if (!(r = option_types[o->type].rd_cfg(o, *(*argv - 1)))) return NULL;
	(*argv)--; (*argc)++;
	return r;
}

unsigned char *lookup_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	struct sockaddr *addrs;
	int addrno, i;

	if (!*argc) return "Parameter expected";
	if (*argc > 1) return "Too many parameters";

	(*argv)++; (*argc)--;
	if (do_real_lookup(*(*argv - 1), &addrs, &addrno)) {
#ifdef HAVE_HERROR
		herror("error");
#else
		fprintf(stderr, "error: host not found\n");
#endif
		return "";
	}

	for (i = 0; i < addrno; i++) {
#ifdef IPV6
		struct sockaddr_in6 addr = *((struct sockaddr_in6 *) &((struct sockaddr_storage *) addrs)[i]);
		unsigned char p[INET6_ADDRSTRLEN];

		if (! inet_ntop(addr.sin6_family, &addr.sin6_addr, p, INET6_ADDRSTRLEN))
			printf("Resolver error.");
		else
			printf("%s\n", p);
#else
		struct sockaddr_in addr = *((struct sockaddr_in *) &((struct sockaddr_storage *) addrs)[i]);
		unsigned char *p = (unsigned char *) &addr.sin_addr.s_addr;

		printf("%d.%d.%d.%d\n", (int) p[0], (int) p[1],
				        (int) p[2], (int) p[3]);
#endif
	}

	mem_free(addrs);

	fflush(stdout);

	return "";
}

unsigned char *version_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	printf("ELinks " VERSION_STRING " - Text WWW browser\n");
	fflush(stdout);
	return "";
}

unsigned char *
printhelp_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	struct hash_item *item;
	int i;

	version_cmd(NULL, NULL, NULL);
	printf("\n");

	printf("Usage: elinks [OPTION]... [URL]\n\n");
	printf("Options:\n\n");

	/* TODO: Alphabetical order! */
	foreach_hash_item (root_options, item, i) {
		struct option *option = item->value;

		if (option->flags & OPT_CMDLINE) {
			unsigned char *cname = cmd_name(option->name);

			printf("-%s ", cname);
			mem_free(cname);

			printf("%s", option_types[option->type].help_str);
			printf("  (%s)\n", option->name);

			if (option->desc) {
				int l = strlen(option->desc);
				int i;

				printf("%15s", "");

				for (i = 0; i < l; i++) {
					putchar(option->desc[i]);

					if (option->desc[i] == '\n')
						printf("%15s", "");
				}

				printf("\n");

				if (option->type == OPT_INT ||
				    option->type == OPT_BOOL ||
				    option->type == OPT_LONG)
					printf("%15sDefault: %d\n", "", * (int *) option->ptr);
				else if (option->type == OPT_STRING)
					printf("%15sDefault: %s\n", "", option->ptr ? (char *) option->ptr : "");
			}

			printf("\n");
		}
	}

/*printf("Keys:\n\
 	ESC	 display menu\n\
	^C	 quit\n\
	^P, ^N	 scroll up, down\n\
	[, ]	 scroll left, right\n\
	up, down select link\n\
	->	 follow link\n\
	<-	 go back\n\
	g	 go to url\n\
	G	 go to url based on current url\n\
	/	 search\n\
	?	 search back\n\
	n	 find next\n\
	N	 find previous\n\
	=	 document info\n\
	\\	 document source\n\
	d	 download\n\
	q	 quit\n");*/

	fflush(stdout);
	return "";
}


struct option_type_info option_types[] = {
	{ bool_cmd, num_rd, num_wr, "[0|1]" },
	{ gen_cmd, num_rd, num_wr, "<num>" },
	{ gen_cmd, num_rd, num_wr, "<num>" },
	{ gen_cmd, str_rd, str_wr, "<str>" },

	{ gen_cmd, cp_rd, cp_wr, "<codepage>" },
	{ gen_cmd, lang_rd, lang_wr, "<language>" },
	{ NULL, type_rd, type_wr, "" },
	{ NULL, ext_rd, ext_wr, "" },
	{ NULL, prog_rd, prog_wr, "" },
	{ NULL, term_rd, term_wr, "" },
	{ NULL, term2_rd, NULL, "" },
	{ NULL, bind_rd, NULL, "" },
	{ NULL, unbind_rd, NULL, "" },
	{ gen_cmd, color_rd, NULL, "<color|#rrggbb>" },

	{ exec_cmd, NULL, NULL, "[<...>]" },
};


/**********************************************************************
 Options values
**********************************************************************/

struct rgb default_fg = { 191, 191, 191 };
struct rgb default_bg = { 0, 0, 0 };
struct rgb default_link = { 0, 0, 255 };
struct rgb default_vlink = { 255, 255, 0 };

void
register_options()
{
	add_opt_string("",
		"accept_language", OPT_CMDLINE | OPT_CFGFILE, "",
		"Send Accept-Language header.");
		
	add_opt_bool("",
		"accesskey_enter", OPT_CMDLINE | OPT_CFGFILE, 0,
		"Automatically follow link / submit form if appropriate accesskey\n"
		"is pressed - this is standart behaviour, however dangerous.");

	add_opt_int("",
		"accesskey_priority", OPT_CMDLINE | OPT_CFGFILE, 0, 2, 1,
		"Priority of 'accesskey' HTML attribute:\n"
		"0 is first try all normal bindings and if it fails, check accesskey\n"
		"1 is first try only frame bindings and if it fails, check accesskey\n"
		"2 is first check accesskey (that can be dangerous)");

	add_opt_bool("",
		"allow_special_files", OPT_CMDLINE | OPT_CFGFILE, 0,
		"Allow reading from non-regular files? (DANGEROUS - reading\n"
		"/dev/urandom or /dev/zero can ruin your day!)");

	add_opt_bool("",
		"anonymous", OPT_CMDLINE, 0,
		"Restrict ELinks so that it can run on an anonymous account.\n"
		"No local file browsing, no downloads. Executing of viewers\n"
		"is allowed, but user can't add or modify entries in\n"
		"association table.");

	/* TODO: We should re-implement this! */
#if 0
	add_opt_ptr("",
		"assume_codepage", OPT_CMDLINE | OPT_CFGFILE, OPT_CODEPAGE, &dds.assume_cp,
		"Use the given codepage when the webpage did not specify\n"
		"its codepage.\n"
		"Default: ISO 8859-1");
#endif

	add_opt_bool("",
		"async_dns", OPT_CMDLINE | OPT_CFGFILE, 1,
		"Use asynchronous DNS resolver?");

	add_opt_int("",
		"base_session", OPT_CMDLINE, 0, MAXINT, 0,
		"Run this ELinks in separate session - instances of ELinks with\n"
		"same base_session will connect together and share runtime\n"
		"informations. By default, base_session is 0.");

	add_opt_bool("",
		"color_dirs", OPT_CMDLINE | OPT_CFGFILE, 1,
		"Highlight directories when listing local disk content?");

	add_opt_int("",
		"cookies_accept", OPT_CMDLINE | OPT_CFGFILE,
		COOKIES_ACCEPT_NONE, COOKIES_ACCEPT_ALL, COOKIES_ACCEPT_ALL,
		"Mode of accepting cookies:\n"
		"0 is accept no cookies\n"
		"1 is ask for confirmation before accepting cookie (UNIMPLEMENTED)\n"
		"2 is accept all cookies");

	add_opt_bool("",
		"cookies_paranoid_security", OPT_CMDLINE | OPT_CFGFILE, 0,
		"When enabled, we'll require three dots in cookies domain for all\n"
		"non-international domains (instead of just two dots). Please see\n"
		"code (cookies.c:check_domain_security()) for further description");

	add_opt_bool("",
		"cookies_save", OPT_CMDLINE | OPT_CFGFILE, 1,
		"Load/save cookies from/to disk?");

	add_opt_bool("",
		"cookies_resave", OPT_CMDLINE | OPT_CFGFILE, 1,
		"Save cookies after each change in cookies list? No effect when\n"
		"cookies_save is off.");

	add_opt_ptr("",
		"default_fg", OPT_CMDLINE | OPT_CFGFILE, OPT_COLOR, &default_fg,
		"Default foreground color.");

	/* FIXME - this produces ugly results now */
	add_opt_ptr("",
		"default_bg", /* OPT_CMDLINE | OPT_CFGFILE */ 0, OPT_COLOR, &default_bg,
		"Default background color.");

	add_opt_ptr("",
		"default_link", OPT_CMDLINE | OPT_CFGFILE, OPT_COLOR, &default_link,
		"Default link color.");

	add_opt_ptr("",
		"default_vlink", OPT_CMDLINE | OPT_CFGFILE, OPT_COLOR, &default_vlink,
		"Default vlink color.");

	add_opt_string("",
		"default_mime_type", OPT_CMDLINE | OPT_CFGFILE, "text/plain",
		"MIME type for a document we should assume by default (when we are\n"
		"unable to guess it properly from known informations about the\n"
		"document).");

	add_opt_string("",
		"download_dir", OPT_CMDLINE | OPT_CFGFILE, "./",
		"Default download directory.");

	add_opt_bool("",
		"download_utime", OPT_CMDLINE | OPT_CFGFILE, 0,
		"Set time of downloaded files?");

	add_opt_bool("",
		"dump", OPT_CMDLINE, 0,
		"Write a plain-text version of the given HTML document to\n"
		"stdout.");

	add_opt_int("",
		"dump_width", OPT_CMDLINE | OPT_CFGFILE, 40, 512, 80,
		"Size of screen in characters, when dumping a HTML document.");

	add_opt_int("",
		"format_cache_size", OPT_CMDLINE | OPT_CFGFILE, 0, 256, 5,
		"Number of cached formatted pages.");

	add_opt_bool("",
		"form_submit_auto", OPT_CMDLINE | OPT_CFGFILE, 1,
		"Automagically submit a form when enter pressed on text field.");

	add_opt_bool("",
		"form_submit_confirm", OPT_CMDLINE | OPT_CFGFILE, 1,
		"Ask for confirmation when submitting a form.");

	add_opt_string("",
		"ftp.anonymous_password", OPT_CMDLINE | OPT_CFGFILE, "some@host.domain",
		"FTP anonymous password to be sent.");

	add_opt_string("",
		"ftp_proxy", OPT_CMDLINE | OPT_CFGFILE, "",
		"Host and port number (host:port) of the FTP proxy, or blank.");

	add_opt_command("",
		"?", OPT_CMDLINE, printhelp_cmd,
		NULL);

	add_opt_command("",
		"h", OPT_CMDLINE, printhelp_cmd,
		NULL);

	add_opt_command("",
		"help", OPT_CMDLINE, printhelp_cmd,
		"Print usage help and exit.");

	add_opt_bool("",
		"http_bugs.allow_blacklist", OPT_CMDLINE | OPT_CFGFILE, 1,
		"Allow blacklist of buggy servers?");

	add_opt_bool("",
		"http_bugs.bug_302_redirect", OPT_CMDLINE | OPT_CFGFILE, 0,
		"Broken 302 redirect (violates RFC but compatible with Netscape)?");

	add_opt_bool("",
		"http_bugs.bug_post_no_keepalive", OPT_CMDLINE | OPT_CFGFILE, 0,
		"No keepalive connection after POST request?");

	add_opt_bool("",
		"http_bugs.http10", OPT_CMDLINE | OPT_CFGFILE, 0,
		"Use HTTP/1.0 protocol?");

	add_opt_string("",
		"http_proxy", OPT_CMDLINE | OPT_CFGFILE, "",
		"Host and port number (host:port) of the HTTP proxy, or blank.");

	/* TODO: REFERER_SAME_URL as default instead? */
	add_opt_int("",
		"http_referer", OPT_CMDLINE | OPT_CFGFILE,
		REFERER_NONE, REFERER_TRUE, REFERER_NONE,
		"Mode of sending HTTP referer:\n"
		"0 is send no referer\n"
		"1 is send current URL as referer\n"
		"2 is send fixed fake referer\n"
		"3 is send previous URL as referer (correct, but insecure)\n");

	add_opt_string("",
		"fake_referer", OPT_CMDLINE | OPT_CFGFILE, "",
		"Fake referer to be sent when http_referer is 3.");

	/* XXX: Disable global history if -anonymous is given? */
	add_opt_bool("",
		"enable_global_history", OPT_CMDLINE | OPT_CFGFILE, 1,
		"Enable global history (\"history of all pages visited\")?");

	add_opt_bool("",
		"keep_unhistory", OPT_CMDLINE | OPT_CFGFILE, 1,
		"Keep unhistory (\"forward history\")?");

	add_opt_ptr("",
		"language", OPT_CMDLINE | OPT_CFGFILE, OPT_LANGUAGE, &current_language,
		"Language of user interface.");

	/* TODO - this is somehow implemented by ff, but disabled
	 * for now as it doesn't work. */
	add_opt_bool("",
		"links_wraparound", /* OPT_CMDLINE | OPT_CFGFILE */ 0, 0,
		"When pressing 'down' on the last link, jump at the first one, and\n"
		"vice versa.");

	add_opt_command("",
		"lookup", OPT_CMDLINE, lookup_cmd,
		"Make lookup for specified host.");

	add_opt_int("",
		"max_connections", OPT_CMDLINE | OPT_CFGFILE, 1, 16, 10,
		"Maximum number of concurrent connections.");

	add_opt_int("",
		"max_connections_to_host", OPT_CMDLINE | OPT_CFGFILE, 1, 8, 2,
		"Maximum number of concurrent connection to a given host.");

	add_opt_int("",
		"memory_cache_size", OPT_CMDLINE | OPT_CFGFILE, 0, MAXINT, 1048576,
		"Memory cache size (in kilobytes).");

	add_opt_bool("",
		"no_connect", OPT_CMDLINE, 0,
		"Run ELinks as a separate instance - instead of connecting to\n"
		"existing instance.");

	add_opt_string("",
		"proxy_user", OPT_CMDLINE | OPT_CFGFILE, "",
		"Proxy authentication user");

	add_opt_string("",
		"proxy_passwd", OPT_CMDLINE | OPT_CFGFILE, "",
		"Proxy authentication passwd");

	add_opt_int("",
		"receive_timeout", OPT_CMDLINE | OPT_CFGFILE, 1, 1800, 120,
		"Timeout on receive (in seconds).");

	add_opt_int("",
		"retries", OPT_CMDLINE | OPT_CFGFILE, 1, 16, 3,
		"Number of tries to estabilish a connection.");

	add_opt_bool("",
		"secure_save", OPT_CMDLINE | OPT_CFGFILE, 1,
		"First write data to 'file.tmp', rename to 'file' upon\n"
		"successful finishing this. Note that this relates only to\n"
		"config files, not downloaded files. You may want to disable\n"
		"it, if you want some config file with some exotic permissions.\n"
		"Secure save is automagically disabled if file is symlink.");

	add_opt_bool("",
		"show_status_bar", OPT_CMDLINE | OPT_CFGFILE, 1,
		"Show status bar on the screen?");

	add_opt_bool("",
		"show_title_bar", OPT_CMDLINE | OPT_CFGFILE, 1,
		"Show title bar on the screen?");

	add_opt_bool("",
		"source", OPT_CMDLINE, 0,
		"Write the given HTML document in source form to stdout.");

	/* TODO - this is implemented, but disabled for now as
	 * it's buggy. */
	add_opt_bool("",
		"startup_goto_dialog", /* OPT_CMDLINE | OPT_CFGFILE */ 0, 1,
		"Pop up goto dialog on startup when there's no homepage?");

	add_opt_int("",
		"unrestartable_receive_timeout", OPT_CMDLINE | OPT_CFGFILE, 1, 1800, 600,
		"Timeout on non restartable connections (in seconds).");

	add_opt_string("",
		"user_agent", OPT_CMDLINE | OPT_CFGFILE, "",
		"Change the User Agent. That means identification string, which\n"
		"is sent to HTTP server, when a document is requested.\n"
		"If empty, defaults to: ELinks (<version>; <system_id>; <term_size>)");
			
	add_opt_command("",
		"version", OPT_CMDLINE, version_cmd,
		"Print ELinks version information and exit.");

	/* config-file-only options */

	add_opt_void("",
		"terminal", OPT_CFGFILE, OPT_TERM,
		NULL);

	add_opt_void("",
		"terminal2", OPT_CFGFILE, OPT_TERM2,
		NULL);

	add_opt_void("",
		"association", OPT_CFGFILE, OPT_MIME_TYPE,
		NULL);

	add_opt_void("",
		"extension", OPT_CFGFILE, OPT_EXTENSION,
		NULL);

	add_opt_ptr("",
		"mailto", OPT_CFGFILE, OPT_PROGRAM, &mailto_prog,
		NULL);

	add_opt_ptr("",
		"telnet", OPT_CFGFILE, OPT_PROGRAM, &telnet_prog,
		NULL);

	add_opt_ptr("",
		"tn3270", OPT_CFGFILE, OPT_PROGRAM, &tn3270_prog,
		NULL);

	add_opt_void("",
		"bind", OPT_CFGFILE, OPT_KEYBIND,
		NULL);

	add_opt_void("",
		"unbind", OPT_CFGFILE, OPT_KEYUNBIND,
		NULL);

	/* HTML options */

	add_opt_codepage("",
		"html_assume_codepage", OPT_CMDLINE | OPT_CFGFILE, 0,
		"Default document codepage.");

	add_opt_bool("",
		"html_avoid_dark_on_black", OPT_CMDLINE | OPT_CFGFILE, 1,
		"Avoid dark colors on black background.");

	add_opt_bool("",
		"html_frames", OPT_CMDLINE | OPT_CFGFILE, 1,
		"Display frames.");

	add_opt_bool("",
		"html_hard_assume", OPT_CMDLINE | OPT_CFGFILE, 0,
		"Ignore charset info sent by server.");

	add_opt_bool("",
		"html_images", OPT_CMDLINE | OPT_CFGFILE, 0,
		"Display links to images.");

	add_opt_int("",
		"html_margin", OPT_CMDLINE | OPT_CFGFILE, 0, 9, 3,
		"Text margin.");

	add_opt_bool("",
		"html_numbered_links", OPT_CMDLINE | OPT_CFGFILE, 0,
		"Display links numbered.");

	add_opt_bool("",
		"html_tables", OPT_CMDLINE | OPT_CFGFILE, 1,
		"Display tables.");

	add_opt_bool("",
		"html_table_order", OPT_CMDLINE | OPT_CFGFILE, 0,
		"Move by columns in table.");
	
	add_opt_bool("",
		"html_use_document_colours", OPT_CMDLINE | OPT_CFGFILE, 1,
		"Use colors specified in document.");
}
