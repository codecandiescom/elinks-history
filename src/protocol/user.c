/* Internal "mailto", "telnet", "tn3270" and misc. protocol implementation */
/* $Id: user.c,v 1.34 2003/07/09 14:39:59 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "elinks.h"

#include "bfu/msgbox.h"
#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "terminal/terminal.h"
#include "protocol/url.h"
#include "protocol/user.h"
#include "sched/download.h"
#include "sched/session.h"
#include "util/memory.h"
#include "util/string.h"


unsigned char *
get_user_program(struct terminal *term, unsigned char *progid, int progidlen)
{
	struct option *opt;
	unsigned char *system_str =
		get_system_str(term ? term->environment & ENV_XWIN : 0);
	unsigned char *name;
	int namelen = 0;

	if (!system_str) return NULL;

	name = init_str();
	if (!name) return NULL;

	add_to_str(&name, &namelen, "protocol.user.");
	add_bytes_to_str(&name, &namelen, progid, progidlen);
	add_chr_to_str(&name, &namelen, '.');
	add_to_str(&name, &namelen, system_str);
	mem_free(system_str);

	opt = get_opt_rec_real(&root_options, name);

	mem_free(name);
	return (unsigned char *) (opt ? opt->ptr : NULL);
}


static unsigned char *
subst_cmd(unsigned char *cmd, unsigned char *url, unsigned char *host,
	  unsigned char *port, unsigned char *dir, unsigned char *subj)
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
				case 'd':
					if (dir) add_to_str(&n, &l, dir);
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

/* Stay silent about complete RFC 2368 support or do it yourself! ;-).
 * --pasky */
static unsigned char *
get_subject_from_query(unsigned char *query)
{
	unsigned char *subject;
	unsigned char *subject_end;

	if (strncmp(query, "subject=", 8)) {
		subject = strstr(query, "&subject=");
		if (!subject) return NULL;
		subject += 9;
	} else {
		subject = query + 8;
	}

	subject_end = strchr(subject, '&');
	return memacpy(subject,
		       subject_end ? subject_end - subject : strlen(subject));
}

static void
user_func(struct session *ses, unsigned char *url)
{
	unsigned char *urldata;
	unsigned char *proto, *host, *port, *dir, *subj;
	unsigned char *prog;

	/* I know this may be NULL and I don't care. --pasky */
	proto = get_protocol_name(url);

	host = get_host_and_pass(url, 0);
	if (!host) {
		if (proto) mem_free(proto);
		msg_box(ses->tab->term, NULL, 0,
			N_("Bad URL syntax"), AL_CENTER,
			N_("Bad user protocol URL"),
			NULL, 1,
			N_("Cancel"), NULL, B_ENTER | B_ESC);
		return;
	}
	if (*host) check_shell_security(&host);

	port = get_port_str(url);
	if (port && *port) check_shell_security(&port);

	urldata = get_url_data(url);
	if (urldata && *urldata) {
		dir = stracpy(urldata);
		if (dir) check_shell_security(&dir);

		/* Some mailto specific stuff follows... */
		subj = strchr(urldata, '?');
		if (subj) {
			subj++;
			subj = get_subject_from_query(subj);
			if (subj) check_shell_security(&subj);
		}
	} else {
		dir = NULL;
		subj = NULL;
	}

	prog = get_user_program(ses->tab->term, proto, strlen(proto));
	if (!prog || !*prog) {
		/* Shouldn't ever happen, but be paranoid. */
		/* Happens when you're in X11 and you've no handler for it. */
		msg_box(ses->tab->term, NULL, MSGBOX_FREE_TEXT,
			N_("No program"), AL_CENTER,
			msg_text(ses->tab->term,
				N_("No program specified for protocol %s."),
				proto),
			NULL, 1,
			N_("Cancel"), NULL, B_ENTER | B_ESC);
		return;
	} else {
		unsigned char *cmd = subst_cmd(prog, url, host, port, dir, subj);

		if (cmd) {
			exec_on_terminal(ses->tab->term, cmd, "", 1);
			mem_free(cmd);
		}
	}

	if (dir) mem_free(dir);
	if (subj) mem_free(subj);
	if (port) mem_free(port);
	mem_free(host);
	if (proto) mem_free(proto);
}

struct protocol_backend user_protocol_backend = {
	/* name: */			"costum",
	/* port: */			0,
	/* handler: */			NULL,
	/* external_handler: */		user_func,
	/* free_syntax: */		0,
	/* need_slashes: */		0,
	/* need_slash_after_host: */	0,
};
