/* Internal "mailto", "telnet", "tn3270" and misc. protocol implementation */
/* $Id: mailto.c,v 1.7 2002/04/29 17:28:01 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <links.h>

#include <bfu/align.h>
#include <bfu/bfu.h>
#include <document/download.h>
#include <document/session.h>
#include <intl/language.h>
#include <lowlevel/terminal.h>
#include <protocol/mailto.h>
#include <protocol/types.h>
#include <protocol/url.h>

void
prog_func(struct terminal *term, struct list_head *list,
	  unsigned char *param, unsigned char *name)
{
	unsigned char *cmd;
	unsigned char *prog = get_prog(list);

	if (!prog || !*prog) {
		msg_box(term, NULL,
			TEXT(T_NO_PROGRAM), AL_CENTER | AL_EXTD_TEXT,
			TEXT(T_NO_PROGRAM_SPECIFIED_FOR), " ", name, ".", NULL,
			NULL, 1,
			TEXT(T_CANCEL), NULL, B_ENTER | B_ESC);
		return;
	}

	cmd = subst_file(prog, param);
	if (cmd) {
		exec_on_terminal(term, cmd, "", 1);
		mem_free(cmd);
	}
}


void
mailto_func(struct session *ses, unsigned char *url)
{
	unsigned char *user, *host, *param;
	int f = 1;

	user = get_user_name(url);
	if (!user) goto fail;

	host = get_host_name(url);
	if (!host) goto fail1;

	param = straconcat(user, "@", host, NULL);
	if (!param) goto fail2;

	check_shell_security(&param);

	f = 0;
	prog_func(ses->term, &mailto_prog, param, TEXT(T_MAIL));

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
tn_func(struct session *ses, unsigned char *url, struct list_head *prog,
	unsigned char *t1, unsigned char *t2)
{
	unsigned char *host, *port, *param;

	host = get_host_name(url);
	if (!host) goto fail;
	check_shell_security(&host);

	port = get_port_str(url);

	if (port) {
		check_shell_security(&port);
		param = straconcat(host, " ", port, NULL);
		mem_free(port);
	} else {
		param = stracpy(host);
	}

	mem_free(host);

	if (!param) goto fail;

	prog_func(ses->term, prog, param, t1);
	mem_free(param);

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
	tn_func(ses, url, &telnet_prog, TEXT(T_TELNET), TEXT(T_BAD_TELNET_URL));
}


void
tn3270_func(struct session *ses, unsigned char *url)
{
	tn_func(ses, url, &tn3270_prog, TEXT(T_TN3270), TEXT(T_BAD_TN3270_URL));
}
