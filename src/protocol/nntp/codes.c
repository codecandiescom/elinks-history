/* NNTP response codes */
/* $Id: codes.c,v 1.8 2005/04/21 01:29:09 jonas Exp $ */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* Needed for asprintf() */
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "bfu/dialog.h"
#include "intl/gettext/libintl.h"
#include "protocol/nntp/codes.h"
#include "protocol/uri.h"
#include "sched/connection.h"
#include "sched/session.h"
#include "sched/task.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/snprintf.h"
#include "viewer/text/draw.h"


/* FIXME: Code duplication with HTTP error document */

struct nntp_code_info {
	enum nntp_code code;
	unsigned char *string;
};

static struct nntp_code_info nntp_code_info[] = {
	/* Information: */
	{ NNTP_CODE_100_HELP_TEXT,	  "Help text on way" },
	{ NNTP_CODE_180_AUTH,		  "Authorization capabilities" },
	{ NNTP_CODE_199_DEBUG,		  "Debug output" },

	/* Success:*/
	{ NNTP_CODE_200_HELLO,		  "Hello; you can post" },
	{ NNTP_CODE_201_HELLO_NOPOST,	  "Hello; you can't post" },
	{ NNTP_CODE_202_SLAVE_STATUS,	  "Slave status noted" },
	{ NNTP_CODE_205_GOODBYE,	  "Closing connection" },
	{ NNTP_CODE_211_GROUP_SELECTED,	  "Group selected" },
	{ NNTP_CODE_215_FOLLOW_GROUPS,	  "Newsgroups follow" },
	{ NNTP_CODE_220_FOLLOW_ARTICLE,	  "Article (head & body) follows" },
	{ NNTP_CODE_221_FOLLOW_HEAD,	  "Head follows" },
	{ NNTP_CODE_222_FOLLOW_BODY,	  "Body follows" },
	{ NNTP_CODE_223_FOLLOW_NOTEXT,	  "No text sent -- stat, next, last" },
	{ NNTP_CODE_224_FOLLOW_XOVER,	  "Overview data follows" },
	{ NNTP_CODE_230_FOLLOW_NEWNEWS,	  "New articles by message-id follow" },
	{ NNTP_CODE_231_FOLLOW_NEWGROUPS, "New newsgroups follow" },
	{ NNTP_CODE_235_ARTICLE_RECEIVED, "Article transferred successfully" },
	{ NNTP_CODE_240_ARTICLE_POSTED,	  "Article posted successfully" },
	{ NNTP_CODE_281_AUTH_ACCEPTED,	  "Authorization (user/pass) ok" },

	/* Continuation: */
	{ NNTP_CODE_335_ARTICLE_TRANSFER, "Send article to be transferred" },
	{ NNTP_CODE_340_ARTICLE_POSTING,  "Send article to be posted" },
	{ NNTP_CODE_380_AUTHINFO_USER,	  "Authorization is required (send user)" },
	{ NNTP_CODE_381_AUTHINFO_PASS,	  "More authorization data required (send password)" },

	/* Notice: */
	{ NNTP_CODE_400_GOODBYE,	  "Have to hang up for some reason" },
	{ NNTP_CODE_411_GROUP_UNKNOWN,	  "No such newsgroup" },
	{ NNTP_CODE_412_GROUP_UNSET,	  "Not currently in newsgroup" },
	{ NNTP_CODE_420_ARTICLE_UNSET,	  "No current article selected" },
	{ NNTP_CODE_421_ARTICLE_NONEXT,	  "No next article in this group" },
	{ NNTP_CODE_422_ARTICLE_NOPREV,	  "No previous article in this group" },
	{ NNTP_CODE_423_ARTICLE_NONUMBER, "No such article number in this group" },
	{ NNTP_CODE_430_ARTICLE_NOID,	  "No such article id at all" },
	{ NNTP_CODE_435_ARTICLE_NOSEND,	  "Already got that article, don't send" },
	{ NNTP_CODE_436_ARTICLE_TRANSFER, "Transfer failed" },
	{ NNTP_CODE_437_ARTICLE_REJECTED, "Article rejected, don't resend" },
	{ NNTP_CODE_440_POSTING_DENIED,	  "Posting not allowed" },
	{ NNTP_CODE_441_POSTING_FAILED,	  "Posting failed" },
	{ NNTP_CODE_480_AUTH_REQUIRED,	  "Authorization required for command" },
	{ NNTP_CODE_482_AUTH_REJECTED,	  "Authorization data rejected" },

	/* Error: */
	{ NNTP_CODE_500_COMMAND_UNKNOWN,  "Command not recognized" },
	{ NNTP_CODE_501_COMMAND_SYNTAX,	  "Command syntax error" },
	{ NNTP_CODE_502_ACCESS_DENIED,	  "Access to server denied" },
	{ NNTP_CODE_503_PROGRAM_FAULT,	  "Program fault, command not performed" },
	{ NNTP_CODE_580_AUTH_FAILED,	  "Authorization Failed" },
};

static unsigned char *
get_nntp_code_string(enum nntp_code code)
{
	int start = 0;
	int end = sizeof_array(nntp_code_info) - 1; /* can be negative. */

	/* Dichotomic search is used there. */
	while (start <= end) {
		int middle = (start + end) >> 1;
		struct nntp_code_info *info = &nntp_code_info[middle];

		if (info->code == code)
			return info->string;

		if (info->code > code)
			end = middle - 1;
		else
			start = middle + 1;
	}

	return NULL;
}


static void
nntp_error_code_dialog(struct terminal *term, struct uri *uri, enum nntp_code code)
{
	unsigned char *codestr = get_nntp_code_string(code);

	if (!codestr) codestr = "Unknown error";

	info_box(term, MSGBOX_FREE_TEXT,
		 N_("NNTP error"), ALIGN_CENTER,
		 msg_text(term,
			 N_("Unable to retrieve %s.\n"
			 "\n"
			 "%s"),
			 struri(uri), codestr));
}

struct nntp_error_info {
	enum nntp_code code;
	struct uri *uri;
};

static void
show_nntp_error_dialog(struct session *ses, void *data)
{
	struct nntp_error_info *info = data;

	nntp_error_code_dialog(ses->tab->term, info->uri, info->code);

	done_uri(info->uri);
	mem_free(info);
}


void
nntp_error_dialog(struct connection *conn, enum nntp_code code)
{
	struct nntp_error_info *info;

	assert(conn && conn->uri);

	info = mem_calloc(1, sizeof(*info));
	if (!info) return;

	info->code = code;
	info->uri = get_uri_reference(conn->uri);

	add_questions_entry(show_nntp_error_dialog, info);
}
