/* Internal "mailto", "telnet", "tn3270" and misc. protocol implementation */
/* $Id: user.c,v 1.43 2003/07/21 04:14:33 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "elinks.h"

#include "bfu/msgbox.h"
#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "protocol/user.h"
#include "sched/download.h"
#include "sched/session.h"
#include "terminal/terminal.h"
#include "util/memory.h"
#include "util/string.h"


unsigned char *
get_user_program(struct terminal *term, unsigned char *progid, int progidlen)
{
	struct option *opt;
	unsigned char *system_str =
		get_system_str(term ? term->environment & ENV_XWIN : 0);
	struct string name;

	if (!system_str) return NULL;

	if (!init_string(&name)) {
		mem_free(system_str);
		return NULL;
	}

	add_to_string(&name, "protocol.user.");
	add_bytes_to_string(&name, progid, progidlen);
	add_char_to_string(&name, '.');
	add_to_string(&name, system_str);
	mem_free(system_str);

	opt = get_opt_rec_real(config_options, name.source);

	done_string(&name);
	return (unsigned char *) (opt ? opt->ptr : NULL);
}


static unsigned char *
subst_cmd(unsigned char *cmd, unsigned char *url, unsigned char *host,
	  unsigned char *port, unsigned char *dir, unsigned char *subj)
{
	struct string string;

	if (!init_string(&string)) return NULL;

	while (*cmd) {
		int p;

		for (p = 0; cmd[p] && cmd[p] != '%'; p++);

		add_bytes_to_string(&string, cmd, p);
		cmd += p;

		if (*cmd == '%') {
			cmd++;
			switch (*cmd) {
				case 'u':
					add_to_string(&string, url);
					break;
				case 'h':
					if (host)
						add_to_string(&string, host);
					break;
				case 'p':
					if (port)
						add_to_string(&string, port);
					break;
				case 'd':
					if (dir)
						add_to_string(&string, dir);
					break;
				case 's':
					if (subj)
						add_to_string(&string, subj);
					break;
				default:
					add_bytes_to_string(&string, cmd - 1, 2);
					break;
			}
			if (*cmd) cmd++;
		}
	}

	return string.source;
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
	unsigned char *subj, *prog, *host;
	struct uri uri;
	unsigned char *uristring = stracpy(url);

	if (!uristring) return;
	if (!parse_uri(&uri, uristring)) {
		mem_free(uristring);
		msg_box(ses->tab->term, NULL, 0,
			N_("Bad URL syntax"), AL_CENTER,
			N_("Bad user protocol URL"),
			NULL, 1,
			N_("Cancel"), NULL, B_ENTER | B_ESC);
		return;
	}

	uri.protocol[uri.protocollen] = 0;
	prog = get_user_program(ses->tab->term, uri.protocol, uri.protocollen);
	if (!prog || !*prog) {
		/* Shouldn't ever happen, but be paranoid. */
		/* Happens when you're in X11 and you've no handler for it. */
		msg_box(ses->tab->term, getml(uristring, NULL), MSGBOX_FREE_TEXT,
			N_("No program"), AL_CENTER,
			msg_text(ses->tab->term,
				N_("No program specified for protocol %s."),
				uri.protocol),
			NULL, 1,
			N_("Cancel"), NULL, B_ENTER | B_ESC);
		return;
	}

	/* XXX Order matters here. Prepare last fields first since the 'common'
	 * uri string is modified by exchanging the field delimiters with
	 * string delimiters. */

	if (uri.data && uri.datalen) {
		uri.data[uri.datalen] = 0;

		/* Some mailto specific stuff follows... */
		subj = strchr(uri.data, '?');
		if (subj) {
			subj++;
			subj = get_subject_from_query(subj);
			if (subj) check_shell_security(&subj);
		}

		/* After mailto subject extraction since we know out '?' */
		check_shell_security(&uri.data);
	} else {
		subj = NULL;
	}

	if (uri.port && uri.portlen) {
		uri.port[uri.portlen] = 0;
		check_shell_security(&uri.port);
	}

	/* TODO	For some user protocols it would be better if substitution of
	 *	each uri field was completely configurable. Now @host contains
	 *	both the uri username field, (password field) and hostname
	 *	field because it is useful for mailto protocol handling. */
	/* It would break a lot of configurations so I don't know. --jonas */

	host = (uri.user ? uri.user : uri.host);
	if (host && *host) {
		uri.host[uri.hostlen] = 0;
		check_shell_security(&host);
	}

	prog = subst_cmd(prog, url, host, uri.port, uri.data, subj);
	mem_free(uristring);
	if (subj) mem_free(subj);
	if (prog) {
		exec_on_terminal(ses->tab->term, prog, "", 1);
		mem_free(prog);
	}
}

struct protocol_backend user_protocol_backend = {
	/* name: */			"custom",
	/* port: */			0,
	/* handler: */			NULL,
	/* external_handler: */		user_func,
	/* free_syntax: */		0,
	/* need_slashes: */		0,
	/* need_slash_after_host: */	0,
};
