/* Protocol implementation manager. */
/* $Id: protocol.c,v 1.4 2003/06/26 19:17:06 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/msgbox.h"
#include "intl/gettext/libintl.h"
#include "protocol/file.h"
#include "protocol/finger.h"
#include "protocol/ftp.h"
#include "protocol/http/http.h"
#include "protocol/http/https.h"
#include "protocol/protocol.h"
#include "protocol/url.h"
#include "protocol/user.h"
#include "sched/sched.h"
#include "sched/session.h"
#include "util/memory.h"
#include "util/string.h"


static void dummyjs_func(struct session *, unsigned char *);

static struct protocol_backend protocol_backends[] = {
	/* SCHEME_FILE */	{"file", 0, file_func, NULL, 1, 1, 0},
	/* SCHEME_FINGER */	{"finger", 79, finger_func, NULL, 0, 1, 1},
	/* SCHEME_FTP */	{"ftp", 21, ftp_func, NULL, 0, 1, 1},
	/* SCHEME_HTTP */	{"http", 80, http_func, NULL, 0, 1, 1},
	/* SCHEME_HTTPS */	{"https", 443, https_func, NULL, 0, 1, 1},
	/* SCHEME_JAVASCRIPT */	{"javascript", 0, NULL, dummyjs_func, 0, 0, 0},
	/* SCHEME_LUA */	{"user", 0, NULL, NULL, 0, 0, 0}, /* lua */
	/* SCHEME_PROXY */	{"proxy", 3128, proxy_func, NULL, 0, 1, 1},

	/* Keep these two last! */
	/* SCHEME_UNKNOWN */	{NULL, 0, NULL},

	/* Internal protocol for mapping to protocol.user.* handlers. Placed
	 * last because it's checked first and else should be ignored. */
	/* SCHEME_USER */	{"custom", 0, NULL, user_func, 0, 0, 0}
};


static void
dummyjs_func(struct session *ses, unsigned char *url)
{
	msg_box(ses->tab->term, NULL, 0,
		N_("Error"), AL_CENTER,
		N_("JavaScript is currently not supported."),
		NULL, 1,
		N_("Cancel"), NULL, B_ENTER | B_ESC);
}


enum uri_scheme
check_protocol(unsigned char *p, int l)
{
	int i;

	/* First check if this isn't some custom (protocol.user) protocol. It
	 * has higher precedence than builtin handlers. */
	/* FIXME: We have to get some terminal to pass along or else this is
	 *	  pretty useless. */
	p[l] = 0;
	if (get_prog(NULL, p)) {
		p[l] = ':';
		/* XXX: We rely on the fact that custom is at the top of the
		 * protocols table. */
		return SCHEME_USER;
	}

	for (i = 0; i < SCHEME_UNKNOWN; i++)
		if (!strcasecmp(protocol_backends[i].name, p)) {
			p[l] = ':';
			return i;
		}

	p[l] = ':';
	return SCHEME_UNKNOWN;
}

int
get_prot_info(unsigned char *prot, int *port,
	      void (**func)(struct connection *),
	      void (**nc_func)(struct session *ses, unsigned char *))
{
	enum uri_scheme scheme = check_protocol(prot, strlen(prot));

	if (scheme == SCHEME_UNKNOWN) return -1;

	if (port) *port = protocol_backends[scheme].port;
	if (func) *func = protocol_backends[scheme].func;
	if (nc_func) *nc_func = protocol_backends[scheme].nc_func;
	return 0;
}

void
get_prot_url_info(enum uri_scheme scheme, int *free_syntax, int *need_slashes,
                  int *need_slash_after_host)
{
	assert(scheme != SCHEME_UNKNOWN);

	if (free_syntax)
		*free_syntax = protocol_backends[scheme].free_syntax;
	if (need_slashes)
		*need_slashes = protocol_backends[scheme].need_slashes;
	if (need_slash_after_host)
		*need_slash_after_host = protocol_backends[scheme].need_slash_after_host;
}


void (*get_protocol_handle(unsigned char *url))(struct connection *)
{
	void (*f)(struct connection *) = NULL;
	unsigned char *p = get_protocol_name(url);

	if (!p) return NULL;
	if (*p) get_prot_info(p, NULL, &f, NULL);
	mem_free(p);

	return f;
}

void (*get_external_protocol_function(unsigned char *url))(struct session *, unsigned char *)
{
	void (*f)(struct session *, unsigned char *) = NULL;
	unsigned char *p = get_protocol_name(url);

	if (!p) return NULL;
	if (*p) get_prot_info(p, NULL, NULL, &f);
	mem_free(p);

	return f;
}
