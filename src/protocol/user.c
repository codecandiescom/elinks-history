/* Internal "mailto", "telnet", "tn3270" and misc. protocol implementation */
/* $Id: user.c,v 1.46 2003/07/25 15:58:35 jonas Exp $ */

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
#include "util/conv.h"
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
subst_cmd(unsigned char *cmd, struct uri *uri, unsigned char *subj)
{
	struct string string;

	if (!init_string(&string)) return NULL;

	while (*cmd) {
		int p;

		for (p = 0; cmd[p] && cmd[p] != '%'; p++);

		add_bytes_to_string(&string, cmd, p);
		cmd += p;

		if (*cmd != '%') break;

		cmd++;
		switch (*cmd) {
			case 'u':
			{
				unsigned char *url = struri(*uri);

				add_shell_safe_to_string(&string, url,
							 strlen(url));
				break;
			}
			case 'h':
				/* TODO	For some user protocols it would be
				 *	better if substitution of each uri
				 *	field was completely configurable. Now
				 *	@host contains both the uri username
				 *	field, (password field) and hostname
				 *	field because it is useful for mailto
				 *	protocol handling. */
				/* It would break a lot of configurations so I
				 * don't know. --jonas */
				if (uri->userlen && uri->hostlen) {
					int hostlen = uri->host + uri->hostlen - uri->user;

					add_shell_safe_to_string(&string, uri->user,
								 hostlen);
				} else if (uri->host) {
					add_shell_safe_to_string(&string, uri->host,
								 uri->hostlen);
				}
				break;
			case 'p':
				if (uri->portlen)
					add_shell_safe_to_string(&string, uri->port,
								 uri->portlen);
				break;
			case 'd':
				if (uri->datalen)
					add_shell_safe_to_string(&string, uri->data,
								 uri->datalen);
				break;
			case 's':
				if (subj)
					add_shell_safe_to_string(&string, subj,
								 strlen(subj));
				break;
			default:
				add_bytes_to_string(&string, cmd - 1, 2);
				break;
		}
		if (*cmd) cmd++;
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
	unsigned char *subj, *prog;
	struct uri uri;

	if (!parse_uri(&uri, url)) {
		msg_box(ses->tab->term, NULL, 0,
			N_("Bad URL syntax"), AL_CENTER,
			N_("Bad user protocol URL"),
			NULL, 1,
			N_("Cancel"), NULL, B_ENTER | B_ESC);
		return;
	}

	prog = get_user_program(ses->tab->term, uri.string, uri.protocollen);
	if (!prog || !*prog) {
		unsigned char *protocol = memacpy(uri.string, uri.protocollen);

		/* Shouldn't ever happen, but be paranoid. */
		/* Happens when you're in X11 and you've no handler for it. */
		msg_box(ses->tab->term, getml(protocol, NULL), MSGBOX_FREE_TEXT,
			N_("No program"), AL_CENTER,
			msg_text(ses->tab->term,
				N_("No program specified for protocol %s."),
				protocol),
			NULL, 1,
			N_("Cancel"), NULL, B_ENTER | B_ESC);
		return;
	}

	if (uri.data && uri.datalen) {
		/* Some mailto specific stuff follows... */
		subj = strchr(uri.data, '?');
		if (subj) {
			subj++;
			subj = get_subject_from_query(subj);
		}
	} else {
		subj = NULL;
	}

	prog = subst_cmd(prog, &uri, subj);
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
