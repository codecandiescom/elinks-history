/* Status/error messages managment */
/* $Id: error.c,v 1.5 2003/07/03 01:03:35 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "intl/gettext/libintl.h"
#include "sched/connection.h"
#include "terminal/terminal.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/string.h"


/* TODO: Move S_* constants to sched/error.h as well? I'm not sure, personally.
 * --pasky */


/* Global variables */
struct s_msg_dsc {
	int n;
	unsigned char *msg;
} msg_dsc[] = {
	{S_WAIT,		N_("Waiting in queue")},
	{S_DNS,			N_("Looking up host")},
	{S_CONN,		N_("Making connection")},
	{S_SSL_NEG,		N_("SSL negotiation")},
	{S_SENT,		N_("Request sent")},
	{S_LOGIN,		N_("Logging in")},
	{S_GETH,		N_("Getting headers")},
	{S_PROC,		N_("Server is processing request")},
	{S_TRANS,		N_("Transferring")},

	{S_WAIT_REDIR,		N_("Waiting for redirect confirmation")},
	{S_OK,			N_("OK")},
	{S_INTERRUPTED,		N_("Interrupted")},
	{S_EXCEPT,		N_("Socket exception")},
	{S_INTERNAL,		N_("Internal error")},
	{S_OUT_OF_MEM,		N_("Out of memory")},
	{S_NO_DNS,		N_("Host not found")},
	{S_CANT_WRITE,		N_("Error writing to socket")},
	{S_CANT_READ,		N_("Error reading from socket")},
	{S_MODIFIED,		N_("Data modified")},
	{S_BAD_URL,		N_("Bad URL syntax")},
	{S_TIMEOUT,		N_("Receive timeout")},
	{S_RESTART,		N_("Request must be restarted")},
	{S_STATE,		N_("Can't get socket state")},

	{S_HTTP_ERROR,		N_("Bad HTTP response")},
	{S_HTTP_100,		N_("HTTP 100 (\?\?\?)")},
	{S_HTTP_204,		N_("No content")},

	{S_FILE_TYPE,		N_("Unknown file type")},
	{S_FILE_ERROR,		N_("Error opening file")},

	{S_FTP_ERROR,		N_("Bad FTP response")},
	{S_FTP_UNAVAIL,		N_("FTP service unavailable")},
	{S_FTP_LOGIN,		N_("Bad FTP login")},
	{S_FTP_PORT,		N_("FTP PORT command failed")},
	{S_FTP_NO_FILE,		N_("File not found")},
	{S_FTP_FILE_ERROR,	N_("FTP file error")},
#ifdef HAVE_SSL
	{S_SSL_ERROR,		N_("SSL error")},
#else
	{S_NO_SSL,		N_("This version of ELinks does not contain SSL/TSL support")},
#endif
	{0,			NULL}
};


struct strerror_val {
	LIST_HEAD(struct strerror_val);

	unsigned char msg[1]; /* must be last */
};

static INIT_LIST_HEAD(strerror_buf); /* struct strerror_val */


unsigned char *
get_err_msg(int state, struct terminal *term)
{
	unsigned char *e;
	struct strerror_val *s;

	if (state <= S_OK || state >= S_WAIT) {
		int i;

		for (i = 0; msg_dsc[i].msg; i++)
			if (msg_dsc[i].n == state)
				return _(msg_dsc[i].msg, term);
unknown_error:
		return _("Unknown error", term);
	}

	e = (unsigned char *) strerror(-state);
	if (!e || !*e) goto unknown_error;

	foreach (s, strerror_buf)
		if (!strcmp(s->msg, e))
			return s->msg;

	s = mem_alloc(sizeof(struct strerror_val) + strlen(e) + 1);
	if (!s) goto unknown_error;

	strcpy(s->msg, e);
	add_to_list(strerror_buf, s);

	return s->msg;
}

void
free_strerror_buf(void)
{
	free_list(strerror_buf);
}
