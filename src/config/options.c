/* Options list and handlers */
/* $Id: options.c,v 1.2 2002/04/27 21:43:48 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

/* We need to have it here. Stupid BSD. */
#include <netinet/in.h>
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <links.h>

#include <config/conf.h>
#include <config/kbdbind.h>
#include <config/options.h>
#include <document/options.h>
#include <document/html/colors.h>
#include <intl/charsets.h>
#include <intl/language.h>
#include <lowlevel/dns.h>
#include <protocol/types.h>


struct option links_options[];
struct option html_options[];

struct option *all_options[] = { links_options, html_options, NULL, };



/**********************************************************************
 Options handlers
**********************************************************************/

void add_nm(struct option *o, unsigned char **s, int *l)
{
	if (*l) add_to_str(s, l, NEWLINE);
	add_to_str(s, l, o->cfg_name);
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
	if (!(r = o->rd_cfg(o, *(*argv - 1)))) return NULL;
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
	no_connect = 1;
	return NULL;
}

unsigned char *anonymous_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	anonymous = 1;
	return NULL;
}

unsigned char *dump_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	if (dmp != o->min && dmp) return "Can't use both -dump and -source";
	dmp = o->min;
	no_connect = 1;
	return NULL;
}

unsigned char *printhelp_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	struct option *option;

	version_cmd(NULL, NULL, NULL);
	printf("\n");

	printf("Usage: links [OPTION]... [URL]\n\n");
	printf("Options:\n\n");

	for (option = links_options; option->cmd_name || option->cfg_name; option++) {
		if (option->cmd_name) {
			printf("-%s ", option->cmd_name);

			if (option->rd_cfg == num_rd)
				printf("<num>");
			else if (option->rd_cfg == str_rd)
				printf("<str>");
			else if (option->rd_cfg == color_rd)
				printf("<color|#rrggbb>");
			else if (option->rd_cfg)
				printf("<...>");

			printf("\n");

			if (option->desc) {
				int l = strlen(option->desc);
				int i;

				printf("%35s", "");

				for (i = 0; i < l; i++) {
					putchar(option->desc[i]);

					if (option->desc[i] == '\n')
						printf("%35s", "");
				}

				printf("\n");

				if (option->rd_cfg == num_rd)
					printf("%35sDefault: %d\n", "", * (int *) option->ptr);
				else if (option->rd_cfg == str_rd)
					printf("%35sDefault: %s\n", "", option->ptr ? (char *) option->ptr : "");
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



/**********************************************************************
 Options values
**********************************************************************/

int anonymous = 0;

unsigned char system_name[MAX_STR_LEN];

unsigned char *links_home = NULL;
int first_use = 0;
int created_home = 0;

int no_connect = 0;
int base_session = 0;

int dmp = 0;
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

int enable_html_tables = 1;
int enable_html_frames = 1;
int display_images = 1;

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

int default_left_margin = HTML_LEFT_MARGIN;

unsigned char fake_referer[MAX_STR_LEN] = "";
enum referer referer = REFERER_NONE;

unsigned char http_proxy[MAX_STR_LEN] = "";
unsigned char ftp_proxy[MAX_STR_LEN] = "";

unsigned char download_dir[MAX_STR_LEN] = "./";

unsigned char default_mime_type[MAX_STR_LEN] = "text/plain";

unsigned char default_anon_pass[MAX_STR_LEN] = "somebody@host.domain";

unsigned char user_agent[MAX_STR_LEN] = "";

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



/**********************************************************************
 Options list
**********************************************************************/

/* Following lists are sorted alphabetically */

struct option links_options[] = {
	/* <optname>, <cfgoptname>,
	 * <cmdread_cmdline>, <cmdread_file>, <cmdwrite_file>,
	 * <minval>, <maxval>, <varname>
	 * <description> */

	{	"accesskey-enter", "accesskey_enter",
		gen_cmd, num_rd, num_wr,
	 	0, 1, &accesskey_enter,
		"Automatically follow link / submit form if appropriate accesskey\n"
		"is pressed - this is standart behaviour, however dangerous." },

	{	"accesskey-priority", "accesskey_priority",
		gen_cmd, num_rd, num_wr,
	 	0, 2, &accesskey_priority,
		"Priority of 'accesskey' HTML attribute:\n"
		"0 is first try all normal bindings and if it fails, check accesskey\n"
		"1 is first try only frame bindings and if it fails, check accesskey\n"
		"2 is first check accesskey (that can be dangerous)" },

	{	"allow-special-files", "allow_special_files",
		gen_cmd, num_rd, num_wr,
	 	0, 1, &allow_special_files,
		"Allow reading from non-regular files? (DANGEROUS - reading\n"
		"/dev/urandom or /dev/zero can ruin your day!)" },

	{	"anonymous", NULL,
		anonymous_cmd, NULL, NULL,
	 	0, 0, NULL,
	      	"Restrict links so that it can run on an anonymous account.\n"
		"No local file browsing, no downloads. Executing of viewers\n"
		"is allowed, but user can't add or modify entries in\n"
		"association table." },

	{	"assume-codepage", "assume_codepage",
		gen_cmd, cp_rd, NULL,
	 	0, 0, &dds.assume_cp,
		"Use the given codepage when the webpage did not specify\n"
		"its codepage.\n"
		"Default: ISO 8859-1" },

	{	"async-dns", "async_dns",
		gen_cmd, num_rd, num_wr,
	 	0, 1, &async_lookup,
		"Use asynchronous DNS resolver?" },

	{	"base-session", NULL,
		gen_cmd, num_rd, NULL,
	 	0, MAXINT, &base_session,
	 	"Run this links in separate session - instances of links with\n"
       		"same base-session will connect together and share runtime\n"
		"informations. By default, base-session is 0." },

	{	"color-dirs", "color_dirs",
	       	gen_cmd, num_rd, num_wr,
		0, 1, &color_dirs,
		"Highlight directories when listing local disk content?" },

	{	"cookies-accept", "cookies_accept",
	       	gen_cmd, num_rd, num_wr,
		COOKIES_ACCEPT_NONE, COOKIES_ACCEPT_ALL, &cookies_accept,
		"Mode of accepting cookies:\n"
		"0 is accept no cookies\n"
		"1 is ask for confirmation before accepting cookie (UNIMPLEMENTED)\n"
		"2 is accept all cookies" },

	{	"cookies-paranoid-security", "cookies_paranoid_security",
	       	gen_cmd, num_rd, num_wr,
		0, 1, &cookies_paranoid_security,
		"When enabled, we'll require three dots in cookies domain for all\n"
		"non-international domains (instead of just two dots). Please see\n"
		"code (cookies.c:check_domain_security()) for further description" },

	{	"cookies-save", "cookies_save",
	       	gen_cmd, num_rd, num_wr,
		0, 1, &cookies_save,
		"Load/save cookies from/to disk?" },

	{	"cookies-resave", "cookies_resave",
	       	gen_cmd, num_rd, num_wr,
		0, 1, &cookies_resave,
		"Save cookies after each change in cookies list? No effect when\n"
		"cookies-save is off." },

	{	"default-fg", "default_fg",
	       	gen_cmd, color_rd, NULL,
		0, 1, &default_fg,
		"Default foreground color." },

		/* FIXME - this produces ugly results now */
#if 0
	{	"default-bg", "default_bg",
	       	gen_cmd, color_rd, NULL,
		0, 1, &default_bg,
		"Default background color." },
#endif

	{	"default-link", "default_link",
	       	gen_cmd, color_rd, NULL,
		0, 1, &default_link,
		"Default link color." },

	{	"default-vlink", "default_vlink",
	       	gen_cmd, color_rd, NULL,
		0, 1, &default_vlink,
		"Default vlink color." },

	{	"default-mime-type", "default_mime_type",
		gen_cmd, str_rd, str_wr,
		0, MAX_STR_LEN, default_mime_type,
		"MIME type for a document we should assume by default (when we are\n"
		"unable to guess it properly from known informations about the\n"
		"document)." },

	{	"download-dir", "download_dir",
		gen_cmd, str_rd, str_wr,
	 	0, MAX_STR_LEN, download_dir,
		"Default download directory." },

	{	"download-utime", "download_utime",
		gen_cmd, num_rd, num_wr,
	 	0, 1, &download_utime,
	 	"Set time of downloaded files?" },

	{	"dump", NULL,
		dump_cmd, NULL, NULL,
	 	D_DUMP, 0, NULL,
		"Write a plain-text version of the given HTML document to\n"
		"stdout." },

	{	"dump-width", "dump_width",
		gen_cmd, num_rd, num_wr,
	 	40, 512, &dump_width,
	 	"Size of screen in characters, when dumping a HTML document." },

	{	"format-cache-size", "format_cache_size",
		gen_cmd, num_rd, num_wr,
	 	0, 256, &max_format_cache_entries,
		"Number of cached formatted pages." },

	{	"form-submit-auto", "form_submit_auto",
		gen_cmd, num_rd, num_wr,
	 	0, 1, &form_submit_auto,
		"Automagically submit a form when enter pressed on text field." },

	{	"form-submit-confirm", "form_submit_confirm",
		gen_cmd, num_rd, num_wr,
	 	0, 1, &form_submit_confirm,
		"Ask for confirmation when submitting a form." },

	{	"ftp.anonymous-password", "ftp.anonymous_password",
		gen_cmd, str_rd, str_wr,
	 	0, MAX_STR_LEN, default_anon_pass,
		"FTP anonymous password to be sent." },

	{	"ftp-proxy", "ftp_proxy",
		gen_cmd, str_rd, str_wr,
	 	0, MAX_STR_LEN, ftp_proxy,
		"Host and port number (host:port) of the FTP proxy, or blank." },

	{	"?", NULL,
		printhelp_cmd, NULL, NULL,
	 	0, 0, NULL,
	 	NULL },

	{	"h", NULL,
		printhelp_cmd, NULL, NULL,
	 	0, 0, NULL,
	 	NULL },

	{	"help", NULL,
		printhelp_cmd, NULL, NULL,
		0, 0, NULL,
	 	"Print usage help and exit." },

	{	"http-bugs.allow-blacklist", "http_bugs.allow_blacklist",
		gen_cmd, num_rd, num_wr,
	 	0, 1, &http_bugs.allow_blacklist,
		"Allow blacklist of buggy servers?" },

	{	"http-bugs.bug-302-redirect", "http_bugs.bug_302_redirect",
		gen_cmd, num_rd, num_wr,
	 	0, 1, &http_bugs.bug_302_redirect,
		"Broken 302 redirect (violates RFC but compatible with Netscape)?" },

	{	"http-bugs.bug-post-no-keepalive", "http_bugs.bug_post_no_keepalive",
		gen_cmd, num_rd, num_wr,
	 	0, 1, &http_bugs.bug_post_no_keepalive,
		"No keepalive connection after POST request?" },

	{	"http-bugs.http10", "http_bugs.http10",
		gen_cmd, num_rd, num_wr,
	 	0, 1, &http_bugs.http10,
		"Use HTTP/1.0 protocol?" },

	{	"http-proxy", "http_proxy",
		gen_cmd, str_rd, str_wr,
	 	0, MAX_STR_LEN, http_proxy,
		"Host and port number (host:port) of the HTTP proxy, or blank." },

	{	"http-referer", "http_referer",
		gen_cmd, num_rd, num_wr,
	 	REFERER_NONE, REFERER_TRUE, &referer,
		"Mode of sending HTTP referer:\n"
		"0 is send no referer\n"
		"1 is send current URL as referer\n"
		"2 is send fixed fake referer\n"
		"3 is send previous URL as referer (correct, but insecure)\n" },

	{	"fake-referer", "fake_referer", /* exception to alphabetical order */
		gen_cmd, str_rd, str_wr,
	 	0, MAX_STR_LEN, fake_referer,
		"Fake referer to be sent when http-referer is 3." },

		/* XXX: Disable global history if -anonymous is given? */
	{	"global-history", "enable_global_history",
		gen_cmd, num_rd, num_wr,
	 	0, 1, &enable_global_history,
	 	"Enable global history (\"history of all pages visited\")?" },

	{	"keep-unhistory", "keep_unhistory",
		gen_cmd, num_rd, num_wr,
	 	0, 1, &keep_unhistory,
	 	"Keep unhistory (\"forward history\")?" },

	{	"language", "language",
		gen_cmd, lang_rd, lang_wr,
	 	0, 0, &current_language,
		"Language of user interface." },

		/* TODO - this is somehow implemented by ff, but commented out
		 * for now as it doesn't work. */
#if 0
	{	"links-wraparound", "links_wraparound",
		gen_cmd, num_rd, num_wr,
	 	0, 1, &links_wraparound,
	 	"When pressing 'down' on the last link, jump at the first one, and\n"
		"vice versa." },
#endif

	{	"lookup", NULL,
		lookup_cmd, NULL, NULL,
	 	0, 0, NULL,
	 	"Make lookup for specified host." },

	{	"max-connections", "max_connections",
		gen_cmd, num_rd, num_wr,
	 	1, 16, &max_connections,
		"Maximum number of concurrent connections." },

	{	"max-connections-to-host", "max_connections_to_host",
		gen_cmd, num_rd, num_wr,
	 	1, 8, &max_connections_to_host,
		"Maximum number of concurrent connection to a given host." },

	{	"memory-cache-size", "memory_cache_size",
		gen_cmd, num_rd, num_wr,
	 	0, MAXINT, &memory_cache_size,
		"Memory cache size (in kilobytes)." },

	{	"no-connect", NULL,
		no_connect_cmd, NULL, NULL,
	 	0, 0, NULL,
	 	"Run links as a separate instance - instead of connecting to\n"
	 	"existing instance." },

	{	"proxy-user", "proxy_user",
		gen_cmd, str_rd, str_wr,
		0, MAX_STR_LEN, proxy_user,
		"Proxy authentication user" },

	{	"proxy-passwd", "proxy_passwd",
		gen_cmd, str_rd, str_wr,
		0, MAX_STR_LEN, proxy_passwd,
		"Proxy authentication passwd" },

	{	"receive-timeout", "receive_timeout",
		gen_cmd, num_rd, num_wr,
	 	1, 1800, &receive_timeout,
		"Timeout on receive (in seconds)." },

	{	"retries", "retries",
		gen_cmd, num_rd, num_wr,
	 	1, 16, &max_tries,
		"Number of tries to estabilish a connection." },

	{	"secure-save", "secure_save",
		gen_cmd, num_rd, num_wr,
		0, 1, &secure_save,
		"First write data to 'file.tmp', rename to 'file' upon\n"
		"successful finishing this. Note that this relates only to\n"
       		"config files, not downloaded files. You may want to disable\n"
		"it, if you want some config file as a symlink or with some\n"
		"exotic permissions." },

	{	"show-status-bar", "show_status_bar",
		gen_cmd, num_rd, num_wr,
	 	0, 1, &show_status_bar,
		"Show status bar on the screen?" },

	{	"show-title-bar", "show_title_bar",
		gen_cmd, num_rd, num_wr,
	 	0, 1, &show_title_bar,
		"Show title bar on the screen?" },

	{	"source", NULL,
		dump_cmd, NULL, NULL,
	 	D_SOURCE, 0, NULL,
		"Write the given HTML document in source form to stdout." },

		/* TODO - this is implemented, but commented out for now as
		 * it's buggy. */
#if 0
	{	"startup-goto-dialog", "startup_goto_dialog",
		gen_cmd, num_rd, num_wr,
	 	0, 1, &startup_goto_dialog,
		"Pop up goto dialog on startup when there's no homepage?" },
#endif

	{	"unrestartable-receive-timeout", "unrestartable_receive_timeout",
		gen_cmd, num_rd, num_wr,
	 	1, 1800, &unrestartable_receive_timeout,
		"Timeout on non restartable connections (in seconds)." },

	{	"user-agent", "user_agent",
		gen_cmd, str_rd, str_wr,
	 	0, MAX_STR_LEN, user_agent,
	        "Change the User Agent. That means identification string, which\n"
		"is sent to HTTP server, when a document is requested.\n"
		"If empty, defaults to: ELinks (<version>; <system_id>; <term_size>)" },
			
	{	"version", NULL,
		version_cmd, NULL, NULL,
	 	0, 0, NULL,
	 	"Print links version information and exit." },

	/* config-file-only options */

	{	NULL, "terminal",
		NULL, term_rd, term_wr,
	 	0, 0, NULL,
		NULL },

	{	NULL, "terminal2",
		NULL, term2_rd, NULL,
	 	0, 0, NULL,
		NULL },

	{	NULL, "association",
		NULL, type_rd, type_wr,
	 	0, 0, NULL,
		NULL },

	{	NULL, "extension",
		NULL, ext_rd, ext_wr,
	 	0, 0, NULL,
		NULL },

	{	NULL, "mailto",
		NULL, prog_rd, prog_wr,
	 	0, 0, &mailto_prog,
		NULL },

	{	NULL, "telnet",
		NULL, prog_rd, prog_wr,
	 	0, 0, &telnet_prog,
		NULL },

	{	NULL, "tn3270",
		NULL, prog_rd, prog_wr,
	 	0, 0, &tn3270_prog,
		NULL },

	{	NULL, "bind",
		NULL, bind_rd, NULL,
	 	0, 0, NULL,
		NULL },

	{	NULL, "unbind",
		NULL, unbind_rd, NULL,
	 	0, 0, NULL,
		NULL },

	{	NULL, NULL,
		NULL, NULL, NULL,
	 	0, 0, NULL,
		NULL },
};

struct option html_options[] = {

	{	"html-assume-codepage", "html_assume_codepage",
		gen_cmd, cp_rd, cp_wr,
	 	0, 0, &dds.assume_cp,
		"Default document codepage." },

	{	"html-avoid-dark-on-black", "html_avoid_dark_on_black",
		gen_cmd, num_rd, num_wr,
	 	0, 1, &dds.avoid_dark_on_black,
		"Avoid dark colors on black background." },

	{	"html-frames", "html_frames",
		gen_cmd, num_rd, num_wr,
	 	0, 1, &dds.frames,
		"Display frames." },

	{	"html-hard-assume", "html_hard_assume",
		gen_cmd, num_rd, num_wr,
	 	0, 1, &dds.hard_assume,
		"Ignore charset info sent by server." },

	{	"html-images", "html_images",
		gen_cmd, num_rd, num_wr,
	 	0, 1, &dds.images,
		"Display links to images." },

	{	"html-margin", "html_margin",
		gen_cmd, num_rd, num_wr,
	 	0, 9, &dds.margin,
		"Text margin." },

	{	"html-numbered-links", "html_numbered_links",
		gen_cmd, num_rd, num_wr,
	 	0, 1, &dds.num_links,
		"Display links numbered." },

	{	"html-tables", "html_tables",
		gen_cmd, num_rd, num_wr,
	 	0, 1, &dds.tables,
		"Display tables." },

	{	"html-table-order", "html_table_order",
		gen_cmd, num_rd, num_wr,
	 	0, 1, &dds.table_order,
		"Move by columns in table." },

	{	"html-use-document-colours", "html_use_document_colours",
		gen_cmd, num_rd, num_wr,
	 	0, 1, &dds.use_document_colours,
		"Use colors specified in document." },

	{	NULL, NULL,
		NULL, NULL, NULL,
	 	0, 0, NULL,
		NULL },
};
