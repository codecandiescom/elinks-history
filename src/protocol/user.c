/* Internal "mailto", "telnet", "tn3270" and misc. protocol implementation */
/* $Id: user.c,v 1.13 2002/12/21 19:22:42 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "elinks.h"

#include "bfu/msgbox.h"
#include "config/options.h"
#include "document/download.h"
#include "document/session.h"
#include "intl/language.h"
#include "lowlevel/terminal.h"
#include "protocol/url.h"
#include "protocol/user.h"
#include "util/memory.h"
#include "util/string.h"


unsigned char *
get_prog(struct terminal *term, unsigned char *progid)
{
	struct option *opt;
	unsigned char *system_str =
		get_system_str(term ? term->environment & ENV_XWIN : 0);
	unsigned char *name;

	if (!system_str) return NULL;
	name = straconcat("protocol.user.", progid, ".",
			  system_str, NULL);
	mem_free(system_str);
	if (!name) return NULL;

	opt = get_opt_rec_real(&root_options, name);

	mem_free(name);
	return (unsigned char *) (opt ? opt->ptr : NULL);
}


static unsigned char *
subst_cmd(unsigned char *cmd, unsigned char *url, unsigned char *host,
	  unsigned char *port, unsigned char *subj)
{
	unsigned char *n = init_str();
	int l = 0;

	if (!n) return NULL;

	while (*cmd) {
		int p;

		for (p = 0; cmd[p] && cmd[p] != '%'; p++);

		add_bytes_to_str(&n, &l, cmd, p);
		cmd += p;

		if (*cmd == '%') {
			cmd++;
			switch (*cmd) {
				case 'u':
					add_to_str(&n, &l, url);
					break;
				case 'h':
					if (host) add_to_str(&n, &l, host);
					break;
				case 'p':
					if (port) add_to_str(&n, &l, port);
					break;
				case 's':
					if (subj) add_to_str(&n, &l, subj);
					break;
				default:
					add_bytes_to_str(&n, &l, cmd - 1, 2);
					break;
			}
			if (*cmd) cmd++;
		}
	}

	return n;
}

/* TODO: Merge with user_func() ? --pasky */
static void
prog_func(struct terminal *term, unsigned char *url, unsigned char *proto,
	  unsigned char *host, unsigned char *port, unsigned char *subj)
{
	unsigned char *cmd;
	unsigned char *prog = get_prog(term, proto);

	if (!prog || !*prog) {
		/* Shouldn't ever happen, but be paranoid. */
		/* Happens when you're in X11 and you've no handler for it. */
		msg_box(term, NULL,
			TEXT(T_NO_PROGRAM), AL_CENTER | AL_EXTD_TEXT,
			TEXT(T_NO_PROGRAM_SPECIFIED_FOR), " ", proto, ".", NULL,
			NULL, 1,
			TEXT(T_CANCEL), NULL, B_ENTER | B_ESC);
		return;
	}

	cmd = subst_cmd(prog, url, host, port, subj);
	if (cmd) {
		exec_on_terminal(term, cmd, "", 1);
		mem_free(cmd);
	}
}


void
user_func(struct session *ses, unsigned char *url)
{
	unsigned char *urldata;
	unsigned char *proto, *host, *port, *subj = NULL;

	/* I know this may be NULL and I don't care. --pasky */
	proto = get_protocol_name(url);

	host = get_host_and_pass(url, 0);
	if (!host) {
		if (proto) mem_free(proto);
		msg_box(ses->term, NULL,
			TEXT(T_BAD_URL_SYNTAX), AL_CENTER,
			TEXT(T_BAD_USER_URL),
			NULL, 1,
			TEXT(T_CANCEL), NULL, B_ENTER | B_ESC);
		return;
	}
	if (*host) check_shell_security(&host);

	port = get_port_str(url);
	if (port && *port) check_shell_security(&port);

	urldata = get_url_data(url);
	if (urldata) {
		urldata = stracpy(urldata);
		if (urldata) subj = strchr(urldata, '?');
	}

	/* Some mailto specific stuff follows... */
	/* Stay silent about complete RFC 2368 support or do it yourself! ;-).
	 * --pasky */

	if (subj) {
		subj++;
		if (strncmp(subj, "subject=", 8)) {
			subj = strstr(subj, "&subject=");
			if (subj) {
				unsigned char *t;

				subj += 9;
				t = strchr(subj, '&');
				if (t) *t = 0;
			}
		} else {
			unsigned char *t;

			subj += 8;
			t = strchr(subj, '&');
			if (t) *t = 0;
		}
	}

	prog_func(ses->term, url, proto, host, port, subj);

	if (urldata) mem_free(urldata);

	if (port) mem_free(port);
	mem_free(host);
	if (proto) mem_free(proto);
}
