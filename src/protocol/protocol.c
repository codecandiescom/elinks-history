/* Protocol implementation manager. */
/* $Id: protocol.c,v 1.13 2003/06/26 23:49:00 jonas Exp $ */

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
	/* PROTOCOL_FILE */	&file_protocol_backend,
	/* PROTOCOL_FINGER */	&finger_protocol_backend,
	/* PROTOCOL_FTP */	&ftp_protocol_backend,
	/* PROTOCOL_HTTP */	&http_protocol_backend,
	/* PROTOCOL_HTTPS */	&https_protocol_backend,
	/* PROTOCOL_JAVASCRIPT */	&dummyjs_protocol_backend,
	/* PROTOCOL_LUA */	&lua_protocol_backend,
	/* PROTOCOL_PROXY */	&proxy_protocol_backend,

	/* Keep these two last! */
	/* PROTOCOL_UNKNOWN */	NULL,

	/* Internal protocol for mapping to protocol.user.* handlers. Placed
	 * last because it's checked first and else should be ignored. */
	/* PROTOCOL_USER */	&user_protocol_backend,
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


enum protocol
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
		return PROTOCOL_USER;
	}

	for (i = 0; i < PROTOCOL_UNKNOWN; i++) {
		if (strcasecmp(protocol_backends[i]->name, p))
			continue;
		p[l] = ':';
		return i;
	}

	p[l] = ':';
	return PROTOCOL_UNKNOWN;
}


int
get_protocol_port(enum protocol protocol)
{
	assert(protocol != PROTOCOL_UNKNOWN);
	return protocol_backends[protocol]->port;
}

int
get_protocol_free_syntax(enum protocol protocol)
{
	assert(protocol != PROTOCOL_UNKNOWN);
	return protocol_backends[protocol]->free_syntax;
}

int
get_protocol_need_slashes(enum protocol protocol)
{
	assert(protocol != PROTOCOL_UNKNOWN);
	return protocol_backends[protocol]->need_slashes;
}

int
get_protocol_need_slash_after_host(enum protocol protocol)
{
	assert(protocol != PROTOCOL_UNKNOWN);
	return protocol_backends[protocol]->need_slash_after_host;
}


protocol_handler *
get_protocol_handler(unsigned char *url)
{
	unsigned char *name = get_protocol_name(url);

	if (name) {
		enum protocol protocol = check_protocol(name, strlen(name));

		mem_free(name);
		if (protocol != PROTOCOL_UNKNOWN)
			return protocol_backends[protocol]->handler;
	}

	return NULL;
}

protocol_external_handler *
get_protocol_external_handler(unsigned char *url)
{
	unsigned char *name = get_protocol_name(url);

	if (name) {
		enum protocol protocol = check_protocol(name, strlen(name));

		mem_free(name);
		if (protocol != PROTOCOL_UNKNOWN)
			return protocol_backends[protocol]->external_handler;
	}

	return NULL;
}
