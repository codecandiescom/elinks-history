/* Options list and handlers and interface */
/* $Id: options.c,v 1.22 2002/05/19 11:06:25 pasky Exp $ */

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


struct hash *links_options;
struct hash *html_options;

struct hash *all_options[] = { /*links_options*/ NULL, /*html_options*/ NULL, NULL, };


/**********************************************************************
 Options interface
**********************************************************************/

/* Note that this is only a base for hiearchical options, which will be much
 * more fun. This part of code is under heavy development, so please treat
 * with care. --pasky, 20020428 ;) */

/* Get record of option of given name, or NULL if there's no such option. */
struct option *
get_opt_rec(struct hash *hash, unsigned char *name)
{
	struct hash_item *item = get_hash_item(hash, name);

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
add_opt_rec(struct hash *hash, struct option *option)
{
	struct option *aopt = mem_alloc(sizeof(struct option));

	memcpy(aopt, option, sizeof(struct option));

	add_hash_item(hash, stracpy(option->name), aopt);
}

void
add_opt(struct hash *hash, unsigned char *name, enum option_flags flags,
	enum option_type type, int min, int max, void *ptr,
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

	add_hash_item(hash, stracpy(option->name), option);
}


void register_options();

void
init_options()
{
	/* 6 bits == 64 entries; I guess it's the best number for options
	 * hash. --pasky */
	links_options = init_hash(6);
	html_options = init_hash(6);

	all_options[0] = links_options;
	all_options[1] = html_options;

	register_options();
}

void
done_options()
{
	free_hash(links_options);
	free_hash(html_options);
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
	*(int *)o->ptr = l;
	mem_free(tok);
	return NULL;
}

void num_wr(struct option *o, unsigned char **s, int *l)
{
	add_nm(o, s, l);
	add_knum_to_str(s, l, *(int *)o->ptr);
}

unsigned char *str_rd(struct option *o, unsigned char *c)
{
	unsigned char *tok = get_token(&c);
	unsigned char *e = NULL;
	if (!tok) return NULL;
	if (strlen(tok) + 1 > o->max) e = "String too long";
	else strcpy(o->ptr, tok);
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
	else *(int *)o->ptr = i;
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

unsigned char *no_connect_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	get_opt_int("no_connect") = 1;
	return NULL;
}

unsigned char *anonymous_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	get_opt_int("anonymous") = 1;
	return NULL;
}

unsigned char *
printhelp_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	struct hash_item *item;
	int i;

	version_cmd(NULL, NULL, NULL);
	printf("\n");

	printf("Usage: links [OPTION]... [URL]\n\n");
	printf("Options:\n\n");

	/* TODO: Alphabetical order! */
	foreach_hash_item (links_options, item, i) {
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

int anonymous = 0;

int no_connect = 0;
int base_session = 0;

int source = 0;
int dump = 0;
int dump_width = 80;

enum cookies_accept cookies_accept = COOKIES_ACCEPT_ALL;
int cookies_save = 1;
int cookies_resave = 1;
int cookies_paranoid_security = 0;

int secure_save = 1;

int async_lookup = 1;
int download_utime = 0;
int max_connections = 10;
int max_connections_to_host = 2;
int max_tries = 3;
int receive_timeout = 120;
int unrestartable_receive_timeout = 600;

int max_format_cache_entries = 5;
long memory_cache_size = 1048576;

struct document_setup dds = { 0, 0, 1, 1, 1, 1, 0, 3, 0, 0 };

struct rgb default_fg = { 191, 191, 191 };
struct rgb default_bg = { 0, 0, 0 };
struct rgb default_link = { 0, 0, 255 };
struct rgb default_vlink = { 255, 255, 0 };

int color_dirs = 1;

int show_status_bar = 1;
int show_title_bar = 1;

int form_submit_auto = 1;
int form_submit_confirm = 1;
int accesskey_enter = 0;
int accesskey_priority = 1;
int links_wraparound = 0;

int allow_special_files = 0;
int keep_unhistory = 1;

int enable_global_history = 1;

unsigned char fake_referer[MAX_STR_LEN] = "";
enum referer referer = REFERER_NONE;

unsigned char http_proxy[MAX_STR_LEN] = "";
unsigned char ftp_proxy[MAX_STR_LEN] = "";

unsigned char download_dir[MAX_STR_LEN] = "./";

unsigned char default_mime_type[MAX_STR_LEN] = "text/plain";

unsigned char default_anon_pass[MAX_STR_LEN] = "somebody@host.domain";

unsigned char user_agent[MAX_STR_LEN] = "";

unsigned char accept_language[MAX_STR_LEN] = "";

unsigned char proxy_user[MAX_STR_LEN] = "";
unsigned char proxy_passwd[MAX_STR_LEN] = "";

int startup_goto_dialog = 1;

/* These are workarounds for some CGI script bugs */
struct http_bugs http_bugs = { 0, 1, 0, 0 };
/*int bug_302_redirect = 0;*/
	/* When got 301 or 302 from POST request, change it to GET
	   - this violates RFC2068, but some buggy message board scripts rely on it */
/*int bug_post_no_keepalive = 0;*/
	/* No keepalive connection after POST request. Some buggy PHP databases report bad
	   results if GET wants to retreive data POSTed in the same connection */

void
register_options()
{
	add_opt(links_options,
		"accept_language", OPT_CMDLINE | OPT_CFGFILE,
		OPT_STRING, 0, MAX_STR_LEN, accept_language,
		"Send Accept-Language header.");
		
	add_opt(links_options,
		"accesskey_enter", OPT_CMDLINE | OPT_CFGFILE,
		OPT_BOOL, 0, 1, &accesskey_enter,
		"Automatically follow link / submit form if appropriate accesskey\n"
		"is pressed - this is standart behaviour, however dangerous.");

	add_opt(links_options,
		"accesskey_priority", OPT_CMDLINE | OPT_CFGFILE,
		OPT_INT, 0, 2, &accesskey_priority,
		"Priority of 'accesskey' HTML attribute:\n"
		"0 is first try all normal bindings and if it fails, check accesskey\n"
		"1 is first try only frame bindings and if it fails, check accesskey\n"
		"2 is first check accesskey (that can be dangerous)");

	add_opt(links_options,
		"allow_special_files", OPT_CMDLINE | OPT_CFGFILE,
		OPT_BOOL, 0, 1, &allow_special_files,
		"Allow reading from non-regular files? (DANGEROUS - reading\n"
		"/dev/urandom or /dev/zero can ruin your day!)");

	add_opt(links_options,
		"anonymous", OPT_CMDLINE,
		OPT_BOOL, 0, 1, &anonymous,
		"Restrict links so that it can run on an anonymous account.\n"
		"No local file browsing, no downloads. Executing of viewers\n"
		"is allowed, but user can't add or modify entries in\n"
		"association table.");

	add_opt(links_options,
		"assume_codepage", OPT_CMDLINE | OPT_CFGFILE,
		OPT_CODEPAGE, 0, 0, &dds.assume_cp,
		"Use the given codepage when the webpage did not specify\n"
		"its codepage.\n"
		"Default: ISO 8859-1");

	add_opt(links_options,
		"async_dns", OPT_CMDLINE | OPT_CFGFILE,
		OPT_BOOL, 0, 1, &async_lookup,
		"Use asynchronous DNS resolver?");

	add_opt(links_options,
		"base_session", OPT_CMDLINE,
		OPT_INT, 0, MAXINT, &base_session,
		"Run this links in separate session - instances of links with\n"
		"same base_session will connect together and share runtime\n"
		"informations. By default, base_session is 0.");

	add_opt(links_options,
		"color_dirs", OPT_CMDLINE | OPT_CFGFILE,
		OPT_BOOL, 0, 1, &color_dirs,
		"Highlight directories when listing local disk content?");

	add_opt(links_options,
		"cookies_accept", OPT_CMDLINE | OPT_CFGFILE,
		OPT_INT, COOKIES_ACCEPT_NONE, COOKIES_ACCEPT_ALL, &cookies_accept,
		"Mode of accepting cookies:\n"
		"0 is accept no cookies\n"
		"1 is ask for confirmation before accepting cookie (UNIMPLEMENTED)\n"
		"2 is accept all cookies");

	add_opt(links_options,
		"cookies_paranoid_security", OPT_CMDLINE | OPT_CFGFILE,
		OPT_BOOL, 0, 1, &cookies_paranoid_security,
		"When enabled, we'll require three dots in cookies domain for all\n"
		"non-international domains (instead of just two dots). Please see\n"
		"code (cookies.c:check_domain_security()) for further description");

	add_opt(links_options,
		"cookies_save", OPT_CMDLINE | OPT_CFGFILE,
		OPT_BOOL, 0, 1, &cookies_save,
		"Load/save cookies from/to disk?");

	add_opt(links_options,
		"cookies_resave", OPT_CMDLINE | OPT_CFGFILE,
		OPT_BOOL, 0, 1, &cookies_resave,
		"Save cookies after each change in cookies list? No effect when\n"
		"cookies_save is off.");

	add_opt(links_options,
		"default_fg", OPT_CMDLINE | OPT_CFGFILE,
		OPT_COLOR, 0, 1, &default_fg,
		"Default foreground color.");

	/* FIXME - this produces ugly results now */
	add_opt(links_options,
		"default_bg", /* OPT_CMDLINE | OPT_CFGFILE */ 0,
		OPT_COLOR, 0, 1, &default_bg,
		"Default background color.");

	add_opt(links_options,
		"default_link", OPT_CMDLINE | OPT_CFGFILE,
		OPT_COLOR, 0, 1, &default_link,
		"Default link color.");

	add_opt(links_options,
		"default_vlink", OPT_CMDLINE | OPT_CFGFILE,
		OPT_COLOR, 0, 1, &default_vlink,
		"Default vlink color.");

	add_opt(links_options,
		"default_mime_type", OPT_CMDLINE | OPT_CFGFILE,
		OPT_STRING, 0, MAX_STR_LEN, default_mime_type,
		"MIME type for a document we should assume by default (when we are\n"
		"unable to guess it properly from known informations about the\n"
		"document).");

	add_opt(links_options,
		"download_dir", OPT_CMDLINE | OPT_CFGFILE,
		OPT_STRING, 0, MAX_STR_LEN, download_dir,
		"Default download directory.");

	add_opt(links_options,
		"download_utime", OPT_CMDLINE | OPT_CFGFILE,
		OPT_BOOL, 0, 1, &download_utime,
		"Set time of downloaded files?");

	add_opt(links_options,
		"dump", OPT_CMDLINE,
		OPT_BOOL, 0, 1, &dump,
		"Write a plain-text version of the given HTML document to\n"
		"stdout.");

	add_opt(links_options,
		"dump_width", OPT_CMDLINE | OPT_CFGFILE,
		OPT_INT, 40, 512, &dump_width,
		"Size of screen in characters, when dumping a HTML document.");

	add_opt(links_options,
		"format_cache_size", OPT_CMDLINE | OPT_CFGFILE,
		OPT_INT, 0, 256, &max_format_cache_entries,
		"Number of cached formatted pages.");

	add_opt(links_options,
		"form_submit_auto", OPT_CMDLINE | OPT_CFGFILE,
		OPT_BOOL, 0, 1, &form_submit_auto,
		"Automagically submit a form when enter pressed on text field.");

	add_opt(links_options,
		"form_submit_confirm", OPT_CMDLINE | OPT_CFGFILE,
		OPT_BOOL, 0, 1, &form_submit_confirm,
		"Ask for confirmation when submitting a form.");

	add_opt(links_options,
		"ftp.anonymous_password", OPT_CMDLINE | OPT_CFGFILE,
		OPT_STRING, 0, MAX_STR_LEN, default_anon_pass,
		"FTP anonymous password to be sent.");

	add_opt(links_options,
		"ftp_proxy", OPT_CMDLINE | OPT_CFGFILE,
		OPT_STRING, 0, MAX_STR_LEN, ftp_proxy,
		"Host and port number (host:port) of the FTP proxy, or blank.");

	add_opt(links_options,
		"?", OPT_CMDLINE,
		OPT_COMMAND, 0, 0, printhelp_cmd,
		NULL);

	add_opt(links_options,
		"h", OPT_CMDLINE,
		OPT_COMMAND, 0, 0, printhelp_cmd,
		NULL);

	add_opt(links_options,
		"help", OPT_CMDLINE,
		OPT_COMMAND, 0, 0, printhelp_cmd,
		"Print usage help and exit.");

	add_opt(links_options,
		"http_bugs.allow_blacklist", OPT_CMDLINE | OPT_CFGFILE,
		OPT_BOOL, 0, 1, &http_bugs.allow_blacklist,
		"Allow blacklist of buggy servers?");

	add_opt(links_options,
		"http_bugs.bug_302_redirect", OPT_CMDLINE | OPT_CFGFILE,
		OPT_BOOL, 0, 1, &http_bugs.bug_302_redirect,
		"Broken 302 redirect (violates RFC but compatible with Netscape)?");

	add_opt(links_options,
		"http_bugs.bug_post_no_keepalive", OPT_CMDLINE | OPT_CFGFILE,
		OPT_BOOL, 0, 1, &http_bugs.bug_post_no_keepalive,
		"No keepalive connection after POST request?");

	add_opt(links_options,
		"http_bugs.http10", OPT_CMDLINE | OPT_CFGFILE,
		OPT_BOOL, 0, 1, &http_bugs.http10,
		"Use HTTP/1.0 protocol?");

	add_opt(links_options,
		"http_proxy", OPT_CMDLINE | OPT_CFGFILE,
		OPT_STRING, 0, MAX_STR_LEN, http_proxy,
		"Host and port number (host:port) of the HTTP proxy, or blank.");

	add_opt(links_options,
		"http_referer", OPT_CMDLINE | OPT_CFGFILE,
		OPT_INT, REFERER_NONE, REFERER_TRUE, &referer,
		"Mode of sending HTTP referer:\n"
		"0 is send no referer\n"
		"1 is send current URL as referer\n"
		"2 is send fixed fake referer\n"
		"3 is send previous URL as referer (correct, but insecure)\n");

	/* XXX: Exception to alphabetical order. */
	add_opt(links_options,
		"fake_referer", OPT_CMDLINE | OPT_CFGFILE,
		OPT_STRING, 0, MAX_STR_LEN, fake_referer,
		"Fake referer to be sent when http_referer is 3.");

	/* XXX: Disable global history if -anonymous is given? */
	add_opt(links_options,
		"enable_global_history", OPT_CMDLINE | OPT_CFGFILE,
		OPT_BOOL, 0, 1, &enable_global_history,
		"Enable global history (\"history of all pages visited\")?");

	add_opt(links_options,
		"keep_unhistory", OPT_CMDLINE | OPT_CFGFILE,
		OPT_BOOL, 0, 1, &keep_unhistory,
		"Keep unhistory (\"forward history\")?");

	add_opt(links_options,
		"language", OPT_CMDLINE | OPT_CFGFILE,
		OPT_LANGUAGE, 0, 0, &current_language,
		"Language of user interface.");

	/* TODO - this is somehow implemented by ff, but disabled
	 * for now as it doesn't work. */
	add_opt(links_options,
		"links_wraparound", /* OPT_CMDLINE | OPT_CFGFILE */ 0,
		OPT_BOOL, 0, 1, &links_wraparound,
		"When pressing 'down' on the last link, jump at the first one, and\n"
		"vice versa.");

	add_opt(links_options,
		"lookup", OPT_CMDLINE,
		OPT_COMMAND, 0, 0, lookup_cmd,
		"Make lookup for specified host.");

	add_opt(links_options,
		"max_connections", OPT_CMDLINE | OPT_CFGFILE,
		OPT_INT, 1, 16, &max_connections,
		"Maximum number of concurrent connections.");

	add_opt(links_options,
		"max_connections_to_host", OPT_CMDLINE | OPT_CFGFILE,
		OPT_INT, 1, 8, &max_connections_to_host,
		"Maximum number of concurrent connection to a given host.");

	add_opt(links_options,
		"memory_cache_size", OPT_CMDLINE | OPT_CFGFILE,
		OPT_INT, 0, MAXINT, &memory_cache_size,
		"Memory cache size (in kilobytes).");

	add_opt(links_options,
		"no_connect", OPT_CMDLINE,
		OPT_BOOL, 0, 1, &no_connect,
		"Run links as a separate instance - instead of connecting to\n"
		"existing instance.");

	add_opt(links_options,
		"proxy_user", OPT_CMDLINE | OPT_CFGFILE,
		OPT_STRING, 0, MAX_STR_LEN, proxy_user,
		"Proxy authentication user");

	add_opt(links_options,
		"proxy_passwd", OPT_CMDLINE | OPT_CFGFILE,
		OPT_STRING, 0, MAX_STR_LEN, proxy_passwd,
		"Proxy authentication passwd");

	add_opt(links_options,
		"receive_timeout", OPT_CMDLINE | OPT_CFGFILE,
		OPT_INT, 1, 1800, &receive_timeout,
		"Timeout on receive (in seconds).");

	add_opt(links_options,
		"retries", OPT_CMDLINE | OPT_CFGFILE,
		OPT_INT, 1, 16, &max_tries,
		"Number of tries to estabilish a connection.");

	add_opt(links_options,
		"secure_save", OPT_CMDLINE | OPT_CFGFILE,
		OPT_BOOL, 0, 1, &secure_save,
		"First write data to 'file.tmp', rename to 'file' upon\n"
		"successful finishing this. Note that this relates only to\n"
		"config files, not downloaded files. You may want to disable\n"
		"it, if you want some config file with some exotic permissions.\n"
		"Secure save is automagically disabled if file is symlink.");

	add_opt(links_options,
		"show_status_bar", OPT_CMDLINE | OPT_CFGFILE,
		OPT_BOOL, 0, 1, &show_status_bar,
		"Show status bar on the screen?");

	add_opt(links_options,
		"show_title_bar", OPT_CMDLINE | OPT_CFGFILE,
		OPT_BOOL, 0, 1, &show_title_bar,
		"Show title bar on the screen?");

	add_opt(links_options,
		"source", OPT_CMDLINE,
		OPT_BOOL, 0, 1, &source,
		"Write the given HTML document in source form to stdout.");

	/* TODO - this is implemented, but disabled for now as
	 * it's buggy. */
	add_opt(links_options,
		"startup_goto_dialog", /* OPT_CMDLINE | OPT_CFGFILE */ 0,
		OPT_BOOL, 0, 1, &startup_goto_dialog,
		"Pop up goto dialog on startup when there's no homepage?");

	add_opt(links_options,
		"unrestartable_receive_timeout", OPT_CMDLINE | OPT_CFGFILE,
		OPT_INT, 1, 1800, &unrestartable_receive_timeout,
		"Timeout on non restartable connections (in seconds).");

	add_opt(links_options,
		"user_agent", OPT_CMDLINE | OPT_CFGFILE,
		OPT_STRING, 0, MAX_STR_LEN, user_agent,
		"Change the User Agent. That means identification string, which\n"
		"is sent to HTTP server, when a document is requested.\n"
		"If empty, defaults to: ELinks (<version>; <system_id>; <term_size>)");
			
	add_opt(links_options,
		"version", OPT_CMDLINE,
		OPT_COMMAND, 0, 0, version_cmd,
		"Print links version information and exit.");

	/* config-file-only options */

	add_opt(links_options,
		"terminal", OPT_CFGFILE,
		OPT_TERM, 0, 0, NULL,
		NULL);

	add_opt(links_options,
		"terminal2", OPT_CFGFILE,
		OPT_TERM2, 0, 0, NULL,
		NULL);

	add_opt(links_options,
		"association", OPT_CFGFILE,
		OPT_MIME_TYPE, 0, 0, NULL,
		NULL);

	add_opt(links_options,
		"extension", OPT_CFGFILE,
		OPT_EXTENSION, 0, 0, NULL,
		NULL);

	add_opt(links_options,
		"mailto", OPT_CFGFILE,
		OPT_PROGRAM, 0, 0, &mailto_prog,
		NULL);

	add_opt(links_options,
		"telnet", OPT_CFGFILE,
		OPT_PROGRAM, 0, 0, &telnet_prog,
		NULL);

	add_opt(links_options,
		"tn3270", OPT_CFGFILE,
		OPT_PROGRAM, 0, 0, &tn3270_prog,
		NULL);

	add_opt(links_options,
		"bind", OPT_CFGFILE,
		OPT_KEYBIND, 0, 0, NULL,
		NULL);

	add_opt(links_options,
		"unbind", OPT_CFGFILE,
		OPT_KEYUNBIND, 0, 0, NULL,
		NULL);

	/* HTML options */

	add_opt(html_options,
		"html_assume_codepage", OPT_CMDLINE | OPT_CFGFILE,
		OPT_CODEPAGE, 0, 0, &dds.assume_cp,
		"Default document codepage.");

	add_opt(html_options,
		"html_avoid_dark_on_black", OPT_CMDLINE | OPT_CFGFILE,
		OPT_BOOL, 0, 1, &dds.avoid_dark_on_black,
		"Avoid dark colors on black background.");

	add_opt(html_options,
		"html_frames", OPT_CMDLINE | OPT_CFGFILE,
		OPT_BOOL, 0, 1, &dds.frames,
		"Display frames.");

	add_opt(html_options,
		"html_hard_assume", OPT_CMDLINE | OPT_CFGFILE,
		OPT_BOOL, 0, 1, &dds.hard_assume,
		"Ignore charset info sent by server.");

	add_opt(html_options,
		"html_images", OPT_CMDLINE | OPT_CFGFILE,
		OPT_BOOL, 0, 1, &dds.images,
		"Display links to images.");

	add_opt(html_options,
		"html_margin", OPT_CMDLINE | OPT_CFGFILE,
		OPT_INT, 0, 9, &dds.margin,
		"Text margin.");

	add_opt(html_options,
		"html_numbered_links", OPT_CMDLINE | OPT_CFGFILE,
		OPT_BOOL, 0, 1, &dds.num_links,
		"Display links numbered.");

	add_opt(html_options,
		"html_tables", OPT_CMDLINE | OPT_CFGFILE,
		OPT_BOOL, 0, 1, &dds.tables,
		"Display tables.");

	add_opt(html_options,
		"html_table_order", OPT_CMDLINE | OPT_CFGFILE,
		OPT_BOOL, 0, 1, &dds.table_order,
		"Move by columns in table.");

	add_opt(html_options,
		"html_use_document_colours", OPT_CMDLINE | OPT_CFGFILE,
		OPT_BOOL, 0, 1, &dds.use_document_colours,
		"Use colors specified in document.");
}
