/* Protocol implementation manager. */
/* $Id: protocol.c,v 1.1 2003/06/26 17:02:28 jonas Exp $ */

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

struct {
	unsigned char *prot;
	int port;
	void (*func)(struct connection *);
	void (*nc_func)(struct session *, unsigned char *);
	int free_syntax;
	int need_slashes;
	int need_slash_after_host;
} protocols[] =
{
	{"custom", 0, NULL, user_func, 0, 0, 0}, /* protocol.user.* */ /* DO NOT MOVE! */
	{"file", 0, file_func, NULL, 1, 1, 0},
	{"http", 80, http_func, NULL, 0, 1, 1},
	{"https", 443, https_func, NULL, 0, 1, 1},
	{"proxy", 3128, proxy_func, NULL, 0, 1, 1},
	{"ftp", 21, ftp_func, NULL, 0, 1, 1},
	{"finger", 79, finger_func, NULL, 0, 1, 1},
	{"javascript", 0, NULL, dummyjs_func, 0, 0, 0},
	{"user", 0, NULL, NULL, 0, 0, 0}, /* lua */
	{NULL, 0, NULL}
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


int
check_protocol(unsigned char *p, int l)
{
	int i;

	/* First check if this isn't some custom (protocol.user) protocol. It
	 * has higher precedence than builtin handlers. */
	p[l] = 0;
	if (get_prog(NULL, p)) {
		p[l] = ':';
		/* XXX: We rely on the fact that custom is at the top of the
		 * protocols table. */
		return 0;
	}

	for (i = 0; protocols[i].prot; i++)
		if (!strcasecmp(protocols[i].prot, p)) {
			p[l] = ':';
			return i;
		}

	p[l] = ':';
	return -1;
}

int
get_prot_info(unsigned char *prot, int *port,
	      void (**func)(struct connection *),
	      void (**nc_func)(struct session *ses, unsigned char *))
{
	int i;

	i = check_protocol(prot, strlen(prot));
	if (i < 0) return -1;

	if (port) *port = protocols[i].port;
	if (func) *func = protocols[i].func;
	if (nc_func) *nc_func = protocols[i].nc_func;
	return 0;
}

void
get_prot_url_info(int i, int *free_syntax, int *need_slashes,
                  int *need_slash_after_host)
{
        if (free_syntax)
                *free_syntax = protocols[i].free_syntax;
        if (need_slashes)
                *need_slashes = protocols[i].need_slashes;
        if (need_slash_after_host)
                *need_slash_after_host = protocols[i].need_slash_after_host;
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
