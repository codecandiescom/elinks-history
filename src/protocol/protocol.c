/* Protocol implementation manager. */
/* $Id: protocol.c,v 1.50 2004/05/09 23:33:13 jonas Exp $ */

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


struct protocol_backend {
	unsigned char *name;
	int port;
	protocol_handler *handler;
	unsigned int free_syntax:1;
	unsigned int need_slashes:1;
	unsigned int need_slash_after_host:1;
};

static const struct protocol_backend protocol_backends[] = {
	{ "file",	   0, file_protocol_handler,	1, 1, 0 },
	{ "finger",	  79, finger_protocol_handler,	0, 1, 1 },
	{ "ftp",	  21, ftp_protocol_handler,	0, 1, 1 },
	{ "http",	  80, http_protocol_handler,	0, 1, 1 },
	{ "https",	 443, https_protocol_handler,	0, 1, 1 },
	{ "smb",	 139, smb_protocol_handler,	0, 1, 1 },
	{ "javascript",	   0, NULL,			0, 0, 0 },
	{ "proxy",	3128, proxy_protocol_handler,	0, 1, 1 },

	/* Keep these last! */
	{ NULL,		   0, NULL,			0, 0, 0 },

	{ "user",	   0, NULL,			0, 0, 0 },
	/* Internal protocol for mapping to protocol.user.* handlers. Placed
	 * last because it's checked first and else should be ignored. */
	{ "custom",	   0, NULL,			0, 0, 0 },
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
		unsigned char *pname = protocol_backends[protocol].name;

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
	return protocol_backends[protocol].port;
}

int
get_protocol_free_syntax(enum protocol protocol)
{
	assert(VALID_PROTOCOL(protocol));
	if_assert_failed return 0;
	return protocol_backends[protocol].free_syntax;
}

int
get_protocol_need_slashes(enum protocol protocol)
{
	assert(VALID_PROTOCOL(protocol));
	if_assert_failed return 0;
	return protocol_backends[protocol].need_slashes;
}

int
get_protocol_need_slash_after_host(enum protocol protocol)
{
	assert(VALID_PROTOCOL(protocol));
	if_assert_failed return 0;
	return protocol_backends[protocol].need_slash_after_host;
}

protocol_handler *
get_protocol_handler(enum protocol protocol)
{
	assert(VALID_PROTOCOL(protocol));
	if_assert_failed return NULL;
	return protocol_backends[protocol].handler;
}


static void
generic_external_protocol_handler(struct session *ses, struct uri *uri)
{
	enum connection_state state;

	switch (uri->protocol) {
	case PROTOCOL_JAVASCRIPT:
		state = S_NO_JAVASCRIPT;
		break;
#ifndef CONFIG_SSL
	case PROTOCOL_HTTPS:
		state = S_NO_SSL;
		break;
#endif
#ifndef CONFIG_FINGER
	case PROTOCOL_FINGER:
		state = S_NO_FINGER;
		break;
#endif
#ifndef CONFIG_SMB
	case PROTOCOL_SMB:
		state = S_NO_SMB;
		break;
#endif
	default:
		state = S_UNKNOWN_PROTOCOL;
	}

	print_error_dialog(ses, state, PRI_CANCEL);
}

protocol_external_handler *
get_protocol_external_handler(enum protocol protocol)
{
	assert(VALID_PROTOCOL(protocol));
	if_assert_failed return NULL;

	if (protocol == PROTOCOL_USER)
		return user_protocol_handler;

	/* If both external and regular protocol handler is NULL return
	 * default handler */
	if (!protocol_backends[protocol].handler)
		return generic_external_protocol_handler;

	return NULL;
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
