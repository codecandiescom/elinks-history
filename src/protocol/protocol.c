/* Protocol implementation manager. */
/* $Id: protocol.c,v 1.40 2004/05/02 13:30:15 jonas Exp $ */

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
#include "protocol/rewrite/rewrite.h"
#include "protocol/smb/smb.h"
#include "protocol/user.h"

static struct protocol_backend dummyjs_protocol_backend;
static struct protocol_backend lua_protocol_backend;

static void
unknown_protocol_func(struct session *ses, struct uri *uri)
{
	print_error_dialog(ses, S_UNKNOWN_PROTOCOL, PRI_CANCEL);
}

static struct protocol_backend unknown_protocol_backend = {
	/* name: */			NULL,
	/* port: */			0,
	/* handler: */			NULL,
	/* external_handler: */		unknown_protocol_func,
	/* free_syntax: */		0,
	/* need_slashes: */		0,
	/* need_slash_after_host: */	0,
};


static struct protocol_backend *protocol_backends[] = {
	/* PROTOCOL_FILE */	&file_protocol_backend,
	/* PROTOCOL_FINGER */	&finger_protocol_backend,
	/* PROTOCOL_FTP */	&ftp_protocol_backend,
	/* PROTOCOL_HTTP */	&http_protocol_backend,
	/* PROTOCOL_HTTPS */	&https_protocol_backend,
#ifdef CONFIG_SMB
	/* PROTOCOL_SMB */	&smb_protocol_backend,
#else
	/* PROTOCOL_SMB */	&unknown_protocol_backend,
#endif
	/* PROTOCOL_JAVASCRIPT */	&dummyjs_protocol_backend,
	/* PROTOCOL_LUA */	&lua_protocol_backend,
	/* PROTOCOL_PROXY */	&proxy_protocol_backend,

	/* Keep these two last! */
	/* PROTOCOL_UNKNOWN */	&unknown_protocol_backend,

	/* Internal protocol for mapping to protocol.user.* handlers. Placed
	 * last because it's checked first and else should be ignored. */
	/* PROTOCOL_USER */	&user_protocol_backend,
};


static void
dummyjs_func(struct session *ses, struct uri *uri)
{
	print_error_dialog(ses, S_NO_JAVASCRIPT, PRI_CANCEL);
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
get_protocol(unsigned char *name, int namelen)
{
	int protocol;

	/* First check if this isn't some custom (protocol.user) protocol. It
	 * has higher precedence than builtin handlers. */
	/* TODO: In order to fully give higher precedence to user chosen
	 *	 protocols we have to get some terminal to pass along. */
	if (get_user_program(NULL, name, namelen))
		return PROTOCOL_USER;

	/* Abuse that we iterate until protocol is PROTOCOL_UNKNOWN */
	for (protocol = 0; protocol < PROTOCOL_UNKNOWN; protocol++) {
		unsigned char *pname = protocol_backends[protocol]->name;

		if (pname && !strlcasecmp(pname, -1, name, namelen))
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


static struct module *protocol_submodules[] = {
#ifdef CONFIG_URI_REWRITE
	&uri_rewrite_module,
#endif
	NULL,
};

struct module protocol_module = struct_module(
	/* name: */		N_("Protocol"),
	/* options: */		NULL,
	/* hooks: */		NULL,
	/* submodules: */	protocol_submodules,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL
);
