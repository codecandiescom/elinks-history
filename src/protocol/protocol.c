/* Protocol implementation manager. */
/* $Id: protocol.c,v 1.10 2003/06/26 21:19:31 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/msgbox.h"
#include "intl/gettext/libintl.h"
#include "protocol/protocol.h"
#include "protocol/url.h"
#include "sched/sched.h"
#include "sched/session.h"
#include "util/memory.h"
#include "util/string.h"

/* Backends dynamic area: */

#include "protocol/file.h"
#include "protocol/finger.h"
#include "protocol/ftp.h"
#include "protocol/http/http.h"
#include "protocol/http/https.h"
#include "protocol/user.h"

static struct protocol_backend dummyjs_protocol_backend;
static struct protocol_backend lua_protocol_backend;

static struct protocol_backend *protocol_backends[] = {
	/* SCHEME_FILE */	&file_protocol_backend,
	/* SCHEME_FINGER */	&finger_protocol_backend,
	/* SCHEME_FTP */	&ftp_protocol_backend,
	/* SCHEME_HTTP */	&http_protocol_backend,
	/* SCHEME_HTTPS */	&https_protocol_backend,
	/* SCHEME_JAVASCRIPT */	&dummyjs_protocol_backend,
	/* SCHEME_LUA */	&lua_protocol_backend,
	/* SCHEME_PROXY */	&proxy_protocol_backend,

	/* Keep these two last! */
	/* SCHEME_UNKNOWN */	NULL,

	/* Internal protocol for mapping to protocol.user.* handlers. Placed
	 * last because it's checked first and else should be ignored. */
	/* SCHEME_USER */	&user_protocol_backend,
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

static struct protocol_backend dummyjs_protocol_backend = {
	/* name: */			"javascript",
	/* port: */			0,
	/* handler: */			NULL,
	/* external_handler: */		dummyjs_func,
	/* free_syntax: */		0,
	/* need_slashes: */		0,
	/* need_slash_after_host: */	0,
};


static struct protocol_backend lua_protocol_backend = {
	/* name: */			"user",
	/* port: */			0,
	/* handler: */			NULL,
	/* external_handler: */		NULL,
	/* free_syntax: */		0,
	/* need_slashes: */		0,
	/* need_slash_after_host: */	0,
};


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

	for (i = 0; i < SCHEME_UNKNOWN; i++) {
		if (strcasecmp(protocol_backends[i]->name, p))
			continue;
		p[l] = ':';
		return i;
	}

	p[l] = ':';
	return SCHEME_UNKNOWN;
}

int
get_prot_info(unsigned char *prot, int *port, protocol_handler **handler,
	      protocol_external_handler **external_handler)
{
	enum uri_scheme scheme = check_protocol(prot, strlen(prot));

	if (scheme == SCHEME_UNKNOWN)
		return -1;

	if (port)
		*port = protocol_backends[scheme]->port;
	if (handler)
		*handler = protocol_backends[scheme]->handler;
	if (external_handler)
		*external_handler = protocol_backends[scheme]->external_handler;
	return 0;
}


int
get_protocol_free_syntax(enum uri_scheme scheme)
{
	assert(scheme != SCHEME_UNKNOWN);
	return protocol_backends[scheme]->free_syntax;
}

int
get_protocol_need_slashes(enum uri_scheme scheme)
{
	assert(scheme != SCHEME_UNKNOWN);
	return protocol_backends[scheme]->need_slashes;
}

int
get_protocol_need_slash_after_host(enum uri_scheme scheme)
{
	assert(scheme != SCHEME_UNKNOWN);
	return protocol_backends[scheme]->need_slash_after_host;
}


protocol_handler *get_protocol_handler(unsigned char *url)
{
	protocol_handler *f = NULL;
	unsigned char *p = get_protocol_name(url);

	if (!p) return NULL;
	if (*p) get_prot_info(p, NULL, &f, NULL);
	mem_free(p);

	return f;
}

protocol_external_handler *get_protocol_external_handler(unsigned char *url)
{
	protocol_external_handler *f = NULL;
	unsigned char *p = get_protocol_name(url);

	if (!p) return NULL;
	if (*p) get_prot_info(p, NULL, NULL, &f);
	mem_free(p);

	return f;
}
