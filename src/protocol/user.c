/* Internal "mailto", "telnet", "tn3270" and misc. protocol implementation */
/* $Id: user.c,v 1.74 2004/07/01 22:26:47 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "bfu/msgbox.h"
#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "osdep/osdep.h"
#include "protocol/uri.h"
#include "protocol/user.h"
#include "sched/download.h"
#include "sched/session.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/conv.h"
#include "util/file.h"
#include "util/memory.h"
#include "util/string.h"


unsigned char *
get_user_program(struct terminal *term, unsigned char *progid, int progidlen)
{
	struct option *opt;
	int xwin = term ? term->environment & ENV_XWIN : 0;
	struct string name;

	if (!init_string(&name)) return NULL;

	add_to_string(&name, "protocol.user.");

	/* Now add lowercased progid part. Delicious. */
	add_bytes_to_string(&name, progid, progidlen);
	convert_to_lowercase(&name.source[sizeof("protocol.user.") - 1], progidlen);

	add_char_to_string(&name, '.');
	add_to_string(&name, get_system_str(xwin));

	opt = get_opt_rec_real(config_options, name.source);

	done_string(&name);
	return (unsigned char *) (opt ? opt->value.string : NULL);
}


static unsigned char *
subst_cmd(unsigned char *cmd, struct uri *uri, unsigned char *subj,
	  unsigned char *formfile)
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
				unsigned char *url = struri(uri);
				int length = get_real_uri_length(uri);

				add_shell_safe_to_string(&string, url, length);
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
			case 'f':
				if (formfile)
					add_to_string(&string, formfile);
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

	if (strncmp(query, "subject=", 8)) {
		subject = strstr(query, "&subject=");
		if (!subject) return NULL;
		subject += 9;
	} else {
		subject = query + 8;
	}

	/* Return subject until next '&'-value or end of string */
	return memacpy(subject, strcspn(subject, "&"));
}

static unsigned char *
save_form_data_to_file(struct uri *uri)
{
	unsigned char *filename = get_tempdir_filename("elinks-XXXXXX");
	int formfd;
	FILE *formfile;

	if (!filename) return NULL;

	formfd = safe_mkstemp(filename);
	if (formfd < 0) {
		mem_free(filename);
		return NULL;
	}

	formfile = fdopen(formfd, "w");
	if (!formfile) {
		mem_free(filename);
		close(formfd);
		return NULL;
	}

	if (uri->post) {
		/* Jump the content type */
		unsigned char *formdata = strchr(uri->post, '\n');

		formdata = formdata ? formdata + 1 : uri->post;
		fwrite(formdata, strlen(formdata), 1, formfile);
	}
	fclose(formfile);

	return filename;
}

void
user_protocol_handler(struct session *ses, struct uri *uri)
{
	unsigned char *subj = NULL, *prog;
	unsigned char *formfilename;

	prog = get_user_program(ses->tab->term, struri(uri), uri->protocollen);
	if (!prog || !*prog) {
		unsigned char *protocol = memacpy(struri(uri), uri->protocollen);

		/* Shouldn't ever happen, but be paranoid. */
		/* Happens when you're in X11 and you've no handler for it. */
		msg_box(ses->tab->term, NULL, MSGBOX_FREE_TEXT,
			N_("No program"), ALIGN_CENTER,
			msg_text(ses->tab->term,
				N_("No program specified for protocol %s."),
				empty_string_or_(protocol)),
			NULL, 1,
			N_("OK"), NULL, B_ENTER | B_ESC);

		mem_free_if(protocol);
		return;
	}

	if (uri->data && uri->datalen) {
		/* Some mailto specific stuff follows... */
		unsigned char *query = get_uri_string(uri, URI_QUERY);

		if (query) {
			subj = get_subject_from_query(query);
			mem_free(query);
		}
	}

	formfilename = save_form_data_to_file(uri);

	prog = subst_cmd(prog, uri, subj, formfilename);
	mem_free_if(subj);
	if (prog) {
		unsigned char *delete = empty_string_or_(formfilename);

		exec_on_terminal(ses->tab->term, prog, delete, 1);
		mem_free(prog);

	} else if (formfilename) {
		unlink(formfilename);
	}

	mem_free_if(formfilename);
}
