/* Internal "mailto", "telnet", "tn3270" and misc. protocol implementation */
/* $Id: user.c,v 1.7 2002/11/28 15:22:40 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "links.h"

#include "bfu/msgbox.h"
#include "document/download.h"
#include "document/session.h"
#include "intl/language.h"
#include "lowlevel/terminal.h"
#include "protocol/mime.h"
#include "protocol/url.h"
#include "protocol/user.h"
#include "util/memory.h"
#include "util/string.h"


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

void
prog_func(struct terminal *term, unsigned char *progid,
	  unsigned char *url, unsigned char *host, unsigned char *port,
	  unsigned char *subj, unsigned char *name)
{
	unsigned char *cmd;
	unsigned char *prog = get_prog(term, progid);

	if (!prog || !*prog) {
		msg_box(term, NULL,
			TEXT(T_NO_PROGRAM), AL_CENTER | AL_EXTD_TEXT,
			TEXT(T_NO_PROGRAM_SPECIFIED_FOR), " ", name, ".", NULL,
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
mailto_func(struct session *ses, unsigned char *url)
{
	unsigned char *user, *host, *param, *urldata = NULL, *subj;
	int f = 1;

	user = get_user_name(url);
	if (!user) goto fail;

	host = get_host_name(url);
	if (!host) goto fail1;

	param = straconcat(user, "@", host, NULL);
	if (!param) goto fail2;

	subj = strchr(param, '?');
	if (subj) {
		*subj = 0;
	} else {
		urldata = get_url_data(url);
		if (urldata) {
			urldata = stracpy(urldata);
			if (!urldata) goto fail3;
			subj = strchr(urldata, '?');
		}
	}

	/* Stay silent about complete RFC 2368 support or do it yourself! ;-).
	 * --pasky */

	if (subj) {
		subj++;
		if (strncmp(subj, "subject=", 8)) {
			subj = strstr(subj, "&subject=");
			if (subj) {
				char *t;

				subj += 9;
				t = strchr(subj, '&');
				if (t) *t = 0;
			}
		} else {
			char *t;

			subj += 8;
			t = strchr(subj, '&');
			if (t) *t = 0;
		}
	}

	check_shell_security(&param);

	f = 0;

	prog_func(ses->term, "mailto", url, param, NULL, subj, TEXT(T_MAIL));

	if (urldata) mem_free(urldata);

fail3:
	mem_free(param);

fail2:
	mem_free(host);

fail1:
	mem_free(user);

fail:
	if (f) {
		msg_box(ses->term, NULL,
			TEXT(T_BAD_URL_SYNTAX), AL_CENTER,
			TEXT(T_BAD_MAILTO_URL),
			NULL, 1,
			TEXT(T_CANCEL), NULL, B_ENTER | B_ESC);
	}
}


void
tn_func(struct session *ses, unsigned char *url, unsigned char *prog,
	unsigned char *t1, unsigned char *t2)
{
	unsigned char *host, *port;

	host = get_host_name(url);
	if (!host) goto fail;
	if (*host) check_shell_security(&host);

	port = get_port_str(url);
	if (port && *port) check_shell_security(&port);

	prog_func(ses->term, prog, url, host, port, NULL, t1);

	mem_free(host);
	if (port) mem_free(port);

	return;
fail:
	msg_box(ses->term, NULL,
		TEXT(T_BAD_URL_SYNTAX), AL_CENTER,
		t2,
		NULL, 1,
		TEXT(T_CANCEL), NULL, B_ENTER | B_ESC);
}


void
telnet_func(struct session *ses, unsigned char *url)
{
	tn_func(ses, url, "telnet", TEXT(T_TELNET), TEXT(T_BAD_TELNET_URL));
}


void
tn3270_func(struct session *ses, unsigned char *url)
{
	tn_func(ses, url, "tn3270", TEXT(T_TN3270), TEXT(T_BAD_TN3270_URL));
}
