/* Internal "mailto", "telnet", "tn3270" and misc. protocol implementation */
/* $Id: user.c,v 1.1 2002/08/06 23:26:31 pasky Exp $ */

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
#include "protocol/types.h"
#include "protocol/url.h"
#include "protocol/user.h"
#include "util/memory.h"
#include "util/string.h"

void
prog_func(struct terminal *term, unsigned char *progid,
	  unsigned char *param, unsigned char *name)
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
	prog_func(ses->term, "mailto", param, TEXT(T_MAIL));

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
	tn_func(ses, url, "telnet", TEXT(T_TELNET), TEXT(T_BAD_TELNET_URL));
}


void
tn3270_func(struct session *ses, unsigned char *url)
{
	tn_func(ses, url, "tn3270", TEXT(T_TN3270), TEXT(T_BAD_TN3270_URL));
}
