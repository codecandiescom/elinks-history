/* Status/error messages managment */
/* $Id: error.c,v 1.14 2003/11/29 18:07:44 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "intl/gettext/libintl.h"
#include "sched/connection.h"
#include "sched/error.h"
#include "terminal/terminal.h"
#include "util/conv.h"
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
	{S_NO_SSL,		N_("This version of ELinks does not contain SSL/TLS support")},
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
	int len;

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

	len = strlen(e);

	foreach (s, strerror_buf)
		if (!strlcmp(s->msg, -1, e, len))
			return s->msg;

	s = mem_calloc(1, sizeof(struct strerror_val) + len);
	if (!s) goto unknown_error;

	memcpy(s->msg, e, len + 1);
	add_to_list(strerror_buf, s);

	return s->msg;
}

void
free_strerror_buf(void)
{
	free_list(strerror_buf);
}


#define average_speed(progress) \
	((longlong) (progress)->loaded * 10 / ((progress)->elapsed / 100))

#define current_speed(progress) \
	((progress)->cur_loaded / (CURRENT_SPD_SEC * SPD_DISP_TIME / 1000))

#define estimated_time(progress) \
	(((progress)->size - (progress)->pos) \
	 / ((longlong) (progress)->loaded * 10 / (progress)->elapsed / 100) \
	 * 1000)

unsigned char *
get_stat_msg(struct download *stat, struct terminal *term, int wide, int full)
{
	struct string msg;

	if (stat->state != S_TRANS || !(stat->prg->elapsed / 100)) {

		/* debug("%d -> %s", stat->state, _(get_err_msg(stat->state), term)); */
		return stracpy(get_err_msg(stat->state, term));
	}

	if (!init_string(&msg)) return NULL;

	add_to_string(&msg, _("Received", term));
	add_char_to_string(&msg, ' ');
	add_xnum_to_string(&msg, stat->prg->pos + stat->prg->start);
	if (stat->prg->size >= 0) {
		add_char_to_string(&msg, ' ');
		add_to_string(&msg, _("of", term));
		add_char_to_string(&msg, ' ');
		add_xnum_to_string(&msg, stat->prg->size);
	}

	add_to_string(&msg, ", ");

	if (stat->prg->elapsed >= CURRENT_SPD_AFTER * SPD_DISP_TIME) {
		add_to_string(&msg,
			      _(wide ? N_("Average speed") : N_("avg"), term));
	} else {
		add_to_string(&msg, _("Speed", term));
	}

	add_char_to_string(&msg, ' ');
	add_xnum_to_string(&msg, average_speed(stat->prg));
	add_to_string(&msg, "/s");

	if (stat->prg->elapsed >= CURRENT_SPD_AFTER * SPD_DISP_TIME) {
		add_to_string(&msg, ", ");
		add_to_string(&msg,
			      _(wide ? N_("current speed") : N_("cur"), term));
		add_char_to_string(&msg, ' '),
		add_xnum_to_string(&msg, current_speed(stat->prg));
		add_to_string(&msg, "/s");
	}

	if (!full) return msg.source;

	/* Do the following only if there is room */

	add_to_string(&msg, ", ");

	add_to_string(&msg, _("Elapsed time", term));
	add_char_to_string(&msg, ' ');
	add_time_to_string(&msg, stat->prg->elapsed);

	if (stat->prg->size >= 0 && stat->prg->loaded > 0) {
		add_to_string(&msg, ", ");
		add_to_string(&msg, _("estimated time", term));
		add_char_to_string(&msg, ' ');
		add_time_to_string(&msg, estimated_time(stat->prg));
	}

	return msg.source;
}
