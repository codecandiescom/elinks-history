/* Protocol implementation manager. */
/* $Id: protocol.c,v 1.35 2004/04/02 22:04:04 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/msgbox.h"
#include "intl/gettext/libintl.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "sched/connection.h"
#include "sched/session.h"
#include "terminal/window.h"
#include "util/memory.h"
#include "util/string.h"

/* Backends dynamic area: */

#include "protocol/file/file.h"
#include "protocol/finger.h"
#include "protocol/ftp/ftp.h"
#include "protocol/http/http.h"
#include "protocol/http/https.h"
#include "protocol/smb/smb.h"
#include "protocol/user.h"

static struct protocol_backend dummyjs_protocol_backend;
static struct protocol_backend lua_protocol_backend;

static struct protocol_backend *protocol_backends[] = {
	/* PROTOCOL_FILE */	&file_protocol_backend,
	/* PROTOCOL_FINGER */	&finger_protocol_backend,
	/* PROTOCOL_FTP */	&ftp_protocol_backend,
	/* PROTOCOL_HTTP */	&http_protocol_backend,
	/* PROTOCOL_HTTPS */	&https_protocol_backend,
#ifdef CONFIG_SMB
	/* PROTOCOL_SMB */	&smb_protocol_backend,
#else
	/* PROTOCOL_SMB */	NULL,
#endif
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
dummyjs_func(struct session *ses, struct uri *uri)
{
	msg_box(ses->tab->term, NULL, 0,
		N_("Error"), AL_CENTER,
		N_("JavaScript is currently not supported."),
		NULL, 1,
		N_("OK"), NULL, B_ENTER | B_ESC);
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
known_protocol(unsigned char *url, unsigned char **end_)
{
	unsigned char *end = get_protocol_end(url);
	int protocol;
	unsigned char *name;
	int namelen;

	if (end_) *end_ = end;

	if (!end) return PROTOCOL_INVALID; /* No valid protocol scheme. */

	name	= url;
	namelen	= end - url;

	/* First check if this isn't some custom (protocol.user) protocol. It
	 * has higher precedence than builtin handlers. */
	/* TODO: In order to fully give higher precedence to user chosen
	 *	 protocols we have to get some terminal to pass along. */
	if (get_user_program(NULL, name, namelen))
		return PROTOCOL_USER;

	/* Abuse that we iterate until protocol is PROTOCOL_UNKNOWN */
	for (protocol = 0; protocol < PROTOCOL_UNKNOWN; protocol++) {
		unsigned char *pname = protocol_backends[protocol]
				     ? protocol_backends[protocol]->name : NULL;

		if (pname && !strlcmp(pname, -1, name, namelen))
			break;
	}

	return protocol;
}

int
get_protocol_port(enum protocol protocol)
{
	assert(VALID_PROTOCOL(protocol));
	if_assert_failed return 0;
	return protocol_backends[protocol]->port;
}

int
get_protocol_free_syntax(enum protocol protocol)
{
	assert(VALID_PROTOCOL(protocol));
	if_assert_failed return 0;
	return protocol_backends[protocol]->free_syntax;
}

int
get_protocol_need_slashes(enum protocol protocol)
{
	assert(VALID_PROTOCOL(protocol));
	if_assert_failed return 0;
	return protocol_backends[protocol]->need_slashes;
}

int
get_protocol_need_slash_after_host(enum protocol protocol)
{
	assert(VALID_PROTOCOL(protocol));
	if_assert_failed return 0;
	return protocol_backends[protocol]->need_slash_after_host;
}

protocol_handler *
get_protocol_handler(enum protocol protocol)
{
	assert(VALID_PROTOCOL(protocol));
	if_assert_failed return NULL;
	return protocol_backends[protocol]->handler;
}

protocol_external_handler *
get_protocol_external_handler(enum protocol protocol)
{
	assert(VALID_PROTOCOL(protocol));
	if_assert_failed return NULL;
	return protocol_backends[protocol]->external_handler;
}
