/* Options variables manipulation core */
/* $Id: options.c,v 1.34 2002/05/25 13:46:03 pasky Exp $ */

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

#include "config/options.h"
#include "config/opttypes.h"
#include "document/html/colors.h"
#include "intl/language.h"
#include "lowlevel/dns.h"
#include "protocol/types.h"
#include "util/error.h"


/* TODO? In the past, covered by shadow and legends, remembered only by the
 * ELinks Elders now, options were in hashes (it was not for a long time, after
 * we started to use dynamic options lists and before we really started to use
 * hiearchic options). Hashes might be swift and deft, but they had a flaw and
 * the flaw showed up as the fatal flaw. They were unsorted, and it was
 * unfriendly to mere mortal users, without pasky's options handlers in their
 * brain, but their own poor-written software. And thus pasky went and rewrote
 * options so that they were in lists from then to now and for all the ages of
 * men, to the glory of mankind. However, one true hero may arise in future
 * fabulous and implement possibility to have both lists and hashes for trees,
 * as it may be useful for some supernatural entities. And when that age will
 * come... */

struct list_head *root_options;

/**********************************************************************
 Options interface
**********************************************************************/

/* If option name contains dots, they are created as "categories" - first,
 * first category is retrieved from list, taken as a list, second category
 * is retrieved etc. */

/* Get record of option of given name, or NULL if there's no such option. */
struct option *
get_opt_rec(struct list_head *tree, unsigned char *name_)
{
	struct option *option;
	unsigned char *aname = stracpy(name_);
	unsigned char *name = aname;
	unsigned char *sep;

	/* Thou shalt read name of following function carefully. */
	if ((sep = strrchr(name, '.'))) {
		struct option *cat;

		*sep = '\0';

		cat = get_opt_rec(tree, name);
		if (!cat || cat->type != OPT_TREE) {
#if 0
			debug("ERROR in get_opt_rec() crawl: %s (%d) -> %s", name, cat?cat->type:-1, sep + 1);
#endif
			mem_free(aname);
			return NULL;
		}

		tree = (struct list_head *) cat->ptr;

		*sep = '.';
		name = sep + 1;
	}

	foreach (option, *tree) {
		if (!strcmp(option->name, name)) {
			mem_free(aname);
			return option;
		}
	}

	mem_free(aname);
	return NULL;
}

/* Fetch pointer to value of certain option. It is guaranteed to never return
 * NULL. Note that you are supposed to use wrapper get_opt(). */
void *
get_opt_(unsigned char *file, int line, struct list_head *tree,
	 unsigned char *name)
{
	struct option *opt = get_opt_rec(tree, name);

#ifdef DEBUG
	errfile = file;
	errline = line;
	if (!opt) int_error("Attempted to fetch unexistent option %s!", name);
	if (!opt->ptr) int_error("Option %s has no value!", name);
#endif

	return opt->ptr;
}

/* Add option to tree. */
void
add_opt_rec(struct list_head *tree, unsigned char *path, struct option *option)
{
	struct list_head *cat = tree;

	if (*path) cat = get_opt(tree, path);
	if (!cat) return;

	add_to_list(*cat, option);
}

void
add_opt(struct list_head *tree, unsigned char *path, unsigned char *name,
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

	add_opt_rec(tree, path, option);
}


void register_options();

struct list_head *
init_options_tree()
{
	struct list_head *list = mem_alloc(sizeof(struct list_head));

	init_list(*list);

	return list;
}

void
init_options()
{
	root_options = init_options_tree();
	register_options();
}

void
free_options_tree(struct list_head *tree)
{
	struct option *option;

	foreach (option, *tree) {
		if (option->type == OPT_BOOL ||
		    option->type == OPT_INT ||
		    option->type == OPT_LONG ||
		    option->type == OPT_STRING ||
		    option->type == OPT_CODEPAGE) {
			/*debug("free %s", option->name);*/
			mem_free(option->ptr);

		} else if (option->type == OPT_TREE) {
			/*debug("-> %s", option->name);*/
			free_options_tree((struct list_head *) option->ptr);
			/*debug("<-");*/
		}
	}

	free_list(*tree);
	mem_free(tree);
}

void
done_options()
{
	free_options_tree(root_options);
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
printhelp_cmd(struct option *option, unsigned char ***argv, int *argc)
{
	version_cmd(NULL, NULL, NULL);
	printf("\n");

	printf("Usage: elinks [OPTION]... [URL]\n\n");
	printf("Options:\n\n");

	/* TODO: Alphabetical order! */
	foreach (option, *root_options) {

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
	add_opt_tree("",
		"connection", OPT_CFGFILE,
		"Connection options.");

	add_opt_bool("connection",
		"async_dns", OPT_CMDLINE | OPT_CFGFILE, 1,
		"Use asynchronous DNS resolver?");

	add_opt_int("connection",
		"max_connections", OPT_CMDLINE | OPT_CFGFILE, 1, 16, 10,
		"Maximum number of concurrent connections.");

	add_opt_int("connection",
		"max_connections_to_host", OPT_CMDLINE | OPT_CFGFILE, 1, 8, 2,
		"Maximum number of concurrent connection to a given host.");

	add_opt_int("connection",
		"retries", OPT_CMDLINE | OPT_CFGFILE, 1, 16, 3,
		"Number of tries to estabilish a connection.");

	add_opt_int("connection",
		"receive_timeout", OPT_CMDLINE | OPT_CFGFILE, 1, 1800, 120,
		"Timeout on receive (in seconds).");

	add_opt_int("connection",
		"unrestartable_receive_timeout", OPT_CMDLINE | OPT_CFGFILE, 1, 1800, 600,
		"Timeout on non restartable connections (in seconds).");



	add_opt_tree("",
		"cookies", OPT_CFGFILE,
		"Cookies options.");

	add_opt_int("cookies",
		"policy", OPT_CMDLINE | OPT_CFGFILE,
		COOKIES_ACCEPT_NONE, COOKIES_ACCEPT_ALL, COOKIES_ACCEPT_ALL,
		"Mode of accepting cookies:\n"
		"0 is accept no cookies\n"
		"1 is ask for confirmation before accepting cookie (UNIMPLEMENTED)\n"
		"2 is accept all cookies");

	add_opt_bool("cookies",
		"paranoid_security", OPT_CMDLINE | OPT_CFGFILE, 0,
		"When enabled, we'll require three dots in cookies domain for all\n"
		"non-international domains (instead of just two dots). Please see\n"
		"code (cookies.c:check_domain_security()) for further description");

	add_opt_bool("cookies",
		"save", OPT_CMDLINE | OPT_CFGFILE, 1,
		"Load/save cookies from/to disk?");

	add_opt_bool("cookies",
		"resave", OPT_CMDLINE | OPT_CFGFILE, 1,
		"Save cookies after each change in cookies list? No effect when\n"
		"cookies_save is off.");



	add_opt_tree("",
		"document", OPT_CFGFILE,
		"Document options.");

	add_opt_tree("document",
		"browse", OPT_CFGFILE,
		"Document browsing options (mainly interactivity).");


	add_opt_tree("document.browse",
		"accesskey", OPT_CFGFILE,
		"Options for handling accesskey attribute.");

	add_opt_bool("document.browse.accesskey",
		"auto_follow", OPT_CMDLINE | OPT_CFGFILE, 0,
		"Automatically follow link / submit form if appropriate accesskey\n"
		"is pressed - this is standart behaviour, however dangerous.");

	add_opt_int("document.browse.accesskey",
		"priority", OPT_CMDLINE | OPT_CFGFILE, 0, 2, 1,
		"Priority of 'accesskey' HTML attribute:\n"
		"0 is first try all normal bindings and if it fails, check accesskey\n"
		"1 is first try only frame bindings and if it fails, check accesskey\n"
		"2 is first check accesskey (that can be dangerous)");


	add_opt_tree("document.browse",
		"forms", OPT_CFGFILE,
		"Options for handling forms interaction.");

	add_opt_bool("document.browse.forms",
		"auto_submit", OPT_CMDLINE | OPT_CFGFILE, 1,
		"Automagically submit a form when enter pressed on text field.");

	add_opt_bool("document.browse.forms",
		"confirm_submit", OPT_CMDLINE | OPT_CFGFILE, 1,
		"Ask for confirmation when submitting a form.");


	add_opt_tree("document.browse",
		"images", OPT_CFGFILE,
		"Options for handling of images.");

	add_opt_bool("document.browse.images",
		"show_as_links", OPT_CMDLINE | OPT_CFGFILE, 0,
		"Display links to images.");


	add_opt_tree("document.browse",
		"links", OPT_CFGFILE,
		"Options for handling of links to other documents.");

	add_opt_bool("document.browse.links",
		"color_dirs", OPT_CMDLINE | OPT_CFGFILE, 1,
		"Highlight links to directories when listing local disk content.");

	add_opt_bool("document.browse.links",
		"numbering", OPT_CMDLINE | OPT_CFGFILE, 0,
		"Display links numbered.");

	/* TODO - this is somehow implemented by ff, but disabled
	 * for now as it doesn't work. */
	add_opt_bool("document.browse.links",
		"wraparound", /* OPT_CMDLINE | OPT_CFGFILE */ 0, 0,
		"When pressing 'down' on the last link, jump at the first one, and\n"
		"vice versa.");


	add_opt_int("document.browse",
		"margin_width", OPT_CMDLINE | OPT_CFGFILE, 0, 9, 3,
		"Horizontal text margin.");

	add_opt_bool("document.browse",
		"show_status_bar", OPT_CMDLINE | OPT_CFGFILE, 1,
		"Show status bar on the screen.");

	add_opt_bool("document.browse",
		"show_title_bar", OPT_CMDLINE | OPT_CFGFILE, 1,
		"Show title bar on the screen.");

	/* TODO - this is implemented, but disabled for now as
	 * it's buggy. */
	add_opt_bool("document.browse",
		"startup_goto_dialog", /* OPT_CMDLINE | OPT_CFGFILE */ 0, 1,
		"Pop up goto dialog on startup when there's no homepage.");

	add_opt_bool("document.browse",
		"table_move_order", OPT_CMDLINE | OPT_CFGFILE, 0,
		"Move by columns in table.");



	add_opt_tree("document",
		"cache", OPT_CFGFILE,
		"Cache options.");

	add_opt_int("document.cache",
		"format_cache_size", OPT_CMDLINE | OPT_CFGFILE, 0, 256, 5,
		"Number of cached formatted pages.");

	add_opt_int("document.cache",
		"memory_cache_size", OPT_CMDLINE | OPT_CFGFILE, 0, MAXINT, 1048576,
		"Memory cache size (in kilobytes).");



	add_opt_tree("document",
		"colors", OPT_CFGFILE,
		"Default color settings.");

	add_opt_ptr("document.colors",
		"text", OPT_CMDLINE | OPT_CFGFILE, OPT_COLOR, &default_fg,
		"Default text color.");

	/* FIXME - this produces ugly results now */
	add_opt_ptr("document.colors",
		"background", /* OPT_CMDLINE | OPT_CFGFILE */ 0, OPT_COLOR, &default_bg,
		"Default background color.");

	add_opt_ptr("document.colors",
		"link", OPT_CMDLINE | OPT_CFGFILE, OPT_COLOR, &default_link,
		"Default link color.");

	add_opt_ptr("document.colors",
		"vlink", OPT_CMDLINE | OPT_CFGFILE, OPT_COLOR, &default_vlink,
		"Default vlink color.");

	add_opt_bool("document.colors",
		"avoid_dark_on_black", OPT_CMDLINE | OPT_CFGFILE, 1,
		"Avoid dark colors on black background.");

	add_opt_bool("document.colors",
		"use_document_colors", OPT_CMDLINE | OPT_CFGFILE, 1,
		"Use colors specified in document.");



	add_opt_tree("document",
		"download", OPT_CFGFILE,
		"Options regarding files downloading and handling.");

	add_opt_string("document.download",
		"default_mime_type", OPT_CMDLINE | OPT_CFGFILE, "text/plain",
		"MIME type for a document we should assume by default (when we are\n"
		"unable to guess it properly from known informations about the\n"
		"document).");

	add_opt_string("document.download",
		"download_dir", OPT_CMDLINE | OPT_CFGFILE, "./",
		"Default download directory.");

	add_opt_bool("document.download",
		"download_utime", OPT_CMDLINE | OPT_CFGFILE, 0,
		"Set time of downloaded files accordingly to one stored on server.");



	add_opt_tree("document",
		"history", OPT_CFGFILE,
		"History options.");

	/* XXX: Disable global history if -anonymous is given? */
	add_opt_bool("document.history",
		"enable_global_history", OPT_CMDLINE | OPT_CFGFILE, 1,
		"Enable global history (\"history of all pages visited\")?");

	add_opt_bool("document.history",
		"keep_unhistory", OPT_CMDLINE | OPT_CFGFILE, 1,
		"Keep unhistory (\"forward history\")?");



	add_opt_tree("document",
		"html", OPT_CFGFILE,
		"Options concerning displaying of HTML pages.");

	add_opt_bool("document.html",
		"display_frames", OPT_CMDLINE | OPT_CFGFILE, 1,
		"Display frames.");

	add_opt_bool("document.html",
		"display_tables", OPT_CMDLINE | OPT_CFGFILE, 1,
		"Display tables.");



	add_opt_codepage("document",
		"assume_codepage", OPT_CMDLINE | OPT_CFGFILE, 0,
		"Default document codepage.");

	add_opt_bool("document",
		"force_assume_codepage", OPT_CMDLINE | OPT_CFGFILE, 0,
		"Ignore charset info sent by server.");

	add_opt_int("document",
		"dump_width", OPT_CMDLINE | OPT_CFGFILE, 40, 512, 80,
		"Size of screen in characters, when dumping a HTML document.");



	add_opt_tree("",
		"protocol", OPT_CFGFILE,
		"Protocol specific options.");

	add_opt_tree("protocol",
		"http", OPT_CFGFILE,
		"HTTP specific options.");


	add_opt_tree("protocol.http",
		"bugs", OPT_CFGFILE,
		"Server-side HTTP bugs workarounds.");

	add_opt_bool("protocol.http.bugs",
		"allow_blacklist", OPT_CMDLINE | OPT_CFGFILE, 1,
		"Allow blacklist of buggy servers.");

	add_opt_bool("protocol.http.bugs",
		"broken_302_redirect", OPT_CMDLINE | OPT_CFGFILE, 0,
		"Broken 302 redirect (violates RFC but compatible with Netscape).");

	add_opt_bool("protocol.http.bugs",
		"post_no_keepalive", OPT_CMDLINE | OPT_CFGFILE, 0,
		"No keepalive connection after POST request.");

	add_opt_bool("protocol.http.bugs",
		"http10", OPT_CMDLINE | OPT_CFGFILE, 0,
		"Use HTTP/1.0 protocol instead of HTTP/1.1.");


	add_opt_tree("protocol.http",
		"proxy", OPT_CFGFILE,
		"HTTP proxy configuration.");

	add_opt_string("protocol.http.proxy",
		"host", OPT_CMDLINE | OPT_CFGFILE, "",
		"Host and port number (host:port) of the HTTP proxy, or blank.");

	add_opt_string("protocol.http.proxy",
		"user", OPT_CMDLINE | OPT_CFGFILE, "",
		"Proxy authentication user.");

	add_opt_string("protocol.http.proxy",
		"passwd", OPT_CMDLINE | OPT_CFGFILE, "",
		"Proxy authentication passwd.");


	add_opt_tree("protocol.http",
		"referer", OPT_CFGFILE,
		"HTTP referer sending rules.");

	add_opt_int("protocol.http.referer",
		"policy", OPT_CMDLINE | OPT_CFGFILE,
		REFERER_NONE, REFERER_TRUE, REFERER_SAME_URL,
		"Mode of sending HTTP referer:\n"
		"0 is send no referer\n"
		"1 is send current URL as referer\n"
		"2 is send fixed fake referer\n"
		"3 is send previous URL as referer (correct, but insecure)\n");

	add_opt_string("protocol.http.referer",
		"fake", OPT_CMDLINE | OPT_CFGFILE, "",
		"Fake referer to be sent when policy is 3.");


	add_opt_string("protocol.http",
		"accept_language", OPT_CMDLINE | OPT_CFGFILE, "",
		"Send Accept-Language header.");

	add_opt_string("protocol.http",
		"user_agent", OPT_CMDLINE | OPT_CFGFILE, "",
		"Change the User Agent ID. That means identification string, which\n"
		"is sent to HTTP server when a document is requested.\n"
		"If empty, defaults to: ELinks (<version>; <system_id>; <term_size>)");



	add_opt_tree("protocol",
		"ftp", OPT_CFGFILE,
		"FTP specific options.");

	add_opt_tree("protocol.ftp",
		"proxy", OPT_CFGFILE,
		"FTP proxy configuration.");

	add_opt_string("protocol.ftp.proxy",
		"host", OPT_CMDLINE | OPT_CFGFILE, "",
		"Host and port number (host:port) of the FTP proxy, or blank.");

	add_opt_string("protocol.ftp",
		"anon_passwd", OPT_CMDLINE | OPT_CFGFILE, "some@host.domain",
		"FTP anonymous password to be sent.");



	add_opt_tree("protocol",
		"files", OPT_CFGFILE,
		"Options specific for local browsing.");

	add_opt_bool("protocol.file",
		"allow_special_files", OPT_CMDLINE | OPT_CFGFILE, 0,
		"Allow reading from non-regular files? (DANGEROUS - reading\n"
		"/dev/urandom or /dev/zero can ruin your day!)");



	add_opt_tree("protocol",
		"user", OPT_CFGFILE,
		"User protocols options.");

	add_opt_ptr("protocol.user",
		"mailto", OPT_CFGFILE, OPT_PROGRAM, &mailto_prog,
		NULL);

	add_opt_ptr("protocol.user",
		"telnet", OPT_CFGFILE, OPT_PROGRAM, &telnet_prog,
		NULL);

	add_opt_ptr("protocol.user",
		"tn3270", OPT_CFGFILE, OPT_PROGRAM, &tn3270_prog,
		NULL);



	add_opt_tree("",
		"ui", OPT_CFGFILE,
		"User interface options.");

	add_opt_ptr("ui",
		"language", OPT_CMDLINE | OPT_CFGFILE, OPT_LANGUAGE, &current_language,
		"Language of user interface.");



	add_opt_bool("",
		"secure_save", OPT_CMDLINE | OPT_CFGFILE, 1,
		"First write data to 'file.tmp', rename to 'file' upon\n"
		"successful finishing this. Note that this relates only to\n"
		"config files, not downloaded files. You may want to disable\n"
		"it, if you want some config file with some exotic permissions.\n"
		"Secure save is automagically disabled if file is symlink.");



	add_opt_bool("",
		"anonymous", OPT_CMDLINE, 0,
		"Restrict ELinks so that it can run on an anonymous account.\n"
		"No local file browsing, no downloads. Executing of viewers\n"
		"is allowed, but user can't add or modify entries in\n"
		"association table.");

	add_opt_int("",
		"base_session", OPT_CMDLINE, 0, MAXINT, 0,
		"Run this ELinks in separate session - instances of ELinks with\n"
		"same base_session will connect together and share runtime\n"
		"informations. By default, base_session is 0.");

	add_opt_bool("",
		"dump", OPT_CMDLINE, 0,
		"Write a plain-text version of the given HTML document to\n"
		"stdout.");

	add_opt_command("",
		"?", OPT_CMDLINE, printhelp_cmd,
		NULL);

	add_opt_command("",
		"h", OPT_CMDLINE, printhelp_cmd,
		NULL);

	add_opt_command("",
		"help", OPT_CMDLINE, printhelp_cmd,
		"Print usage help and exit.");

	add_opt_command("",
		"lookup", OPT_CMDLINE, lookup_cmd,
		"Make lookup for specified host.");

	add_opt_bool("",
		"no_connect", OPT_CMDLINE, 0,
		"Run ELinks as a separate instance - instead of connecting to\n"
		"existing instance.");

	add_opt_bool("",
		"source", OPT_CMDLINE, 0,
		"Write the given HTML document in source form to stdout.");
			
	add_opt_command("",
		"version", OPT_CMDLINE, version_cmd,
		"Print ELinks version information and exit.");



	/* config-file-only options */
	/* These will disappear */

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

	add_opt_void("",
		"bind", OPT_CFGFILE, OPT_KEYBIND,
		NULL);

	add_opt_void("",
		"unbind", OPT_CFGFILE, OPT_KEYUNBIND,
		NULL);
}
