/* Internal SMB protocol implementation */
/* $Id: smb.c,v 1.42 2004/04/27 13:48:08 zas Exp $ */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* Needed for asprintf() */
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef CONFIG_SMB

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h> /* FreeBSD needs this before resource.h */
#endif
#include <sys/types.h> /* FreeBSD needs this before resource.h */
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "cache/cache.h"
#include "lowlevel/connect.h"
#include "lowlevel/select.h"
#include "osdep/osdep.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "sched/connection.h"
#include "util/memory.h"
#include "util/snprintf.h"
#include "util/string.h"

/* XXX: Nice cleanup target --pasky */
/* FIXME: we rely on smbclient output which may change in future,
 * so i think we should use libsmbclient instead (or better in addition)
 * This stuff is a quick hack, but it works ;). --Zas */

enum smb_list_type {
	SMB_LIST_NONE,
	SMB_LIST_SHARES,
	SMB_LIST_DIR,
};

struct smb_connection_info {
	enum smb_list_type list_type;

	/* If this is 1, it means one socket is already off. The second one
	 * should call end_smb_connection() when it goes off as well. */
	int closing;

	int textlen;
	unsigned char text[1];
};

static void end_smb_connection(struct connection *conn);


/* Return 0 if @conn->cached was set. */
static int
smb_get_cache(struct connection *conn)
{
	if (conn->cached) return 0;

	conn->cached = get_cache_entry(conn->uri);
	if (conn->cached) return 0;

	abort_conn_with_state(conn, S_OUT_OF_MEM);
	return -1;
}


#define READ_SIZE	4096

static int
smb_read_data(struct connection *conn, int sock, unsigned char *dst)
{
	int r;
	struct smb_connection_info *si = conn->info;

	r = read(sock, dst, READ_SIZE);
	if (r == -1) {
		retry_conn_with_state(conn, -errno);
		return -1;
	}
	if (r == 0) {
		if (!si->closing) {
			si->closing = 1;
			set_handlers(conn->socket, NULL, NULL, NULL, NULL);
			return 0;
		}
		end_smb_connection(conn);
		return 0;
	}

	return r;
}

static void
smb_read_text(struct connection *conn, int sock)
{
	int r;
	struct smb_connection_info *si = conn->info;

	si = mem_realloc(si, sizeof(struct smb_connection_info) + si->textlen
			     + READ_SIZE + 2); /* XXX: why +2 ? --Zas */
	if (!si) {
		abort_conn_with_state(conn, S_OUT_OF_MEM);
		return;
	}
	conn->info = si;

	r = smb_read_data(conn, sock, si->text + si->textlen);
	if (r <= 0) return;

	if (!conn->from) set_connection_state(conn, S_GETH);
	si->textlen += r;
}

static void
smb_got_data(struct connection *conn)
{
	struct smb_connection_info *si = conn->info;
	unsigned char buffer[READ_SIZE];
	int r;

	if (si->list_type != SMB_LIST_NONE) {
		smb_read_text(conn, conn->data_socket);
		return;
	}

	r = smb_read_data(conn, conn->data_socket, buffer);
	if (r <= 0) return;

	set_connection_state(conn, S_TRANS);

	if (smb_get_cache(conn)) return;

	conn->received += r;
	if (add_fragment(conn->cached, conn->from, buffer, r) == 1)
		conn->tries = 0;
	conn->from += r;
}

#undef READ_SIZE

static void
smb_got_text(struct connection *conn)
{
	smb_read_text(conn, conn->socket);
}

 /* Search for @str1 followed by @str2 in @line.
  * It returns NULL if not found, or pointer to start
  * of @str2 in @line if found.  */
static unsigned char *
find_strs(unsigned char *line, unsigned char *str1,
	  unsigned char *str2)
{
	unsigned char *p = strstr(line, str1);

	if (!p) return NULL;

	p = strstr(p + strlen(str1), str2);
	return p;
}

/* FIXME: split it. --Zas */
static void
end_smb_connection(struct connection *conn)
{
	struct smb_connection_info *si = conn->info;

	if (smb_get_cache(conn)) return;

	if (conn->from) {
		truncate_entry(conn->cached, conn->from, 1);
		conn->cached->incomplete = 0;
		goto bye;
	}

	if (si->textlen && si->text[si->textlen - 1] != '\n')
		si->text[si->textlen++] = '\n';
	si->text[si->textlen] = '\0';

	if ((strstr(si->text, "NT_STATUS_FILE_IS_A_DIRECTORY")
	     || strstr(si->text, "NT_STATUS_ACCESS_DENIED")
	     || strstr(si->text, "ERRbadfile"))
	    && conn->uri->datalen
	    && conn->uri->data[conn->uri->datalen - 1] != '/'
	    && conn->uri->data[conn->uri->datalen - 1] != '\\') {
		redirect_cache(conn->cached, "/", 1, 0);

	} else {
		unsigned char *line_start, *line_end, *line_end2;
		struct string page;
		enum {
			SMB_TYPE_NONE,
			SMB_TYPE_SHARE,
			SMB_TYPE_SERVER,
			SMB_TYPE_WORKGROUP
		} type = SMB_TYPE_NONE;
		int pos = 0;

		if (!init_string(&page)) {
			abort_conn_with_state(conn, S_OUT_OF_MEM);
			return;
		}

		add_to_string(&page, "<html><head><title>/");
		add_bytes_to_string(&page, conn->uri->data, conn->uri->datalen);
		add_to_string(&page, "</title></head><body><pre>");

		line_start = si->text;
		while ((line_end = strchr(line_start, '\n'))) {
			unsigned char *line;

			/* FIXME: Just look if '\r' is right in front of '\n'?
			 * --pasky */
			line_end2 = strchr(line_start, '\r');
			if (!line_end2 || line_end2 > line_end)
				line_end2 = line_end;
			line = memacpy(line_start, line_end2 - line_start);

			/* And I got bored here with cleaning it up. --pasky */

			if (si->list_type == SMB_LIST_SHARES) {
				unsigned char *ll, *lll, *found;

				if (!*line) type = SMB_TYPE_NONE;

				found = find_strs(line, "Sharename", "Type");
				if (found) {
					pos = found - line;
					type = SMB_TYPE_SHARE;
					goto print_as_is;
				}

				found = find_strs(line, "Server", "Comment");
				if (found) {
					type = SMB_TYPE_SERVER;
					goto print_as_is;
				}

				found = find_strs(line, "Workgroup", "Master");
				if (found) {
					pos = found - line;
					type = SMB_TYPE_WORKGROUP;
					goto print_as_is;
				}

				if (type == SMB_TYPE_NONE)
					goto print_as_is;

				for (ll = line; *ll; ll++)
					if (!isspace(*ll) && *ll != '-')
						goto print_next;

				goto print_as_is;

print_next:
				for (ll = line; *ll; ll++)
					if (!isspace(*ll))
						break;

				for (lll = ll; *lll/* && lll[1]*/; lll++)
					if (isspace(*lll)/* && isspace(lll[1])*/)
						break;

				switch (type) {
				case SMB_TYPE_SHARE:
				{
					unsigned char *llll;

					if (!strstr(lll, "Disk"))
						goto print_as_is;

					if (pos && pos < strlen(line)
					    && isspace(*(llll = line + pos - 1))
					    && llll > ll) {
						while (llll > ll && isspace(*llll))
							llll--;
						if (!isspace(*llll))
							lll = llll + 1;
					}

					add_bytes_to_string(&page, line, ll - line);
					add_to_string(&page, "<a href=\"");
					add_bytes_to_string(&page, ll, lll - ll);
					add_to_string(&page, "/\">");
					add_bytes_to_string(&page, ll, lll - ll);
					add_to_string(&page, "</a>");
					add_to_string(&page, lll);
					break;
				}

				case SMB_TYPE_WORKGROUP:
					if (pos < strlen(line) && pos
					    && isspace(line[pos - 1])
					    && !isspace(line[pos])) {
						ll = line + pos;
					} else {
						for (ll = lll; *ll; ll++)
							if (!isspace(*ll))
								break;
					}
					for (lll = ll; *lll; lll++)
						if (isspace(*lll))
							break;
					/* Fall-through */

				case SMB_TYPE_SERVER:
					add_bytes_to_string(&page, line, ll - line);
					add_to_string(&page, "<a href=\"smb://");
					add_bytes_to_string(&page, ll, lll - ll);
					add_to_string(&page, "/\">");
					add_bytes_to_string(&page, ll, lll - ll);
					add_to_string(&page, "</a>");
					add_to_string(&page, lll);
					break;

				case SMB_TYPE_NONE:
					goto print_as_is;
				}

			} else if (si->list_type == SMB_LIST_DIR) {
				if (strstr(line, "NT_STATUS")) {
					line_end[1] = '\0';
					goto print_as_is;
				}

				if (line_end2 - line_start >= 5
				    && line_start[0] == ' '
				    && line_start[1] == ' '
				    && line_start[2] != ' ') {
					int dir = 0;
					unsigned char *pp;
					unsigned char *p = line_start + 3;
					unsigned char *url = p - 1;

					while (p + 2 <= line_end2) {
						if (p[0] == ' ' && p[1] == ' ')
							goto is_a_file_entry;
						p++;
					}
					goto print_as_is;

is_a_file_entry:
					pp = p;
					while (pp < line_end2 && *pp == ' ')
						pp++;
					while (pp < line_end2 && *pp != ' ') {
						if (*pp == 'D') {
							dir = 1;
							break;
						}
						pp++;
					}

					if (*url == '.' && p - url == 1) goto ignored;

					add_to_string(&page, "  <a href=\"");
					add_bytes_to_string(&page, url, p - url);
					if (dir) add_char_to_string(&page, '/');
					add_to_string(&page, "\">");
					add_bytes_to_string(&page, url, p - url);
					add_to_string(&page, "</a>");
					add_bytes_to_string(&page, p, line_end - p);

				} else {
					goto print_as_is;
				}

			} else {
print_as_is:
				add_bytes_to_string(&page, line_start, line_end2 - line_start);
			}

			add_char_to_string(&page, '\n');
ignored:
			line_start = line_end + 1;
			mem_free(line);
		}

		add_to_string(&page, "</pre></body></html>");

		add_fragment(conn->cached, 0, page.source, page.length);
		conn->from += page.length;
		truncate_entry(conn->cached, page.length, 1);
		conn->cached->incomplete = 0;
		done_string(&page);

		if (!conn->cached->head)
			conn->cached->head = stracpy("\r\n");
		add_to_strn(&conn->cached->head, "Content-Type: text/html\r\n");
	}

bye:
	close_socket(conn, &conn->socket);
	close_socket(conn, &conn->data_socket);
	abort_conn_with_state(conn, S_OK);
}


/* Close all non-terminal file descriptors. */
static void
close_all_non_term_fd(void)
{
	int n;
	int max = 1024;
#ifdef RLIMIT_NOFILE
	struct rlimit lim;

	if (!getrlimit(RLIMIT_NOFILE, &lim))
		max = lim.rlim_max;
#endif
	for (n = 3; n < max; n++)
		close(n);
}

static void
smb_func(struct connection *conn)
{
	int out_pipe[2] = { -1, -1 };
	int err_pipe[2] = { -1, -1 };
	unsigned char *share, *dir;
	unsigned char *p;
	int cpid, dirlen;
	struct smb_connection_info *si;

	si = mem_calloc(1, sizeof(struct smb_connection_info) + 2);
	if (!si) {
		abort_conn_with_state(conn, S_OUT_OF_MEM);
		return;
	}
	conn->info = si;

	p = strchr(conn->uri->data, '/');
	if (p && p - conn->uri->data < conn->uri->datalen) {
		share = memacpy(conn->uri->data, p - conn->uri->data);
		dir = p + 1;
		/* FIXME: ensure @dir do not contain dangerous chars. --Zas */

	} else if (conn->uri->datalen) {
		if (smb_get_cache(conn)) return;

		redirect_cache(conn->cached, "/", 1, 0);
		abort_conn_with_state(conn, S_OK);
		return;

	} else {
		share = stracpy("");
		dir = "";
	}

	if (!share) {
		abort_conn_with_state(conn, S_OUT_OF_MEM);
		return;
	}

	dirlen = strlen(dir);
	if (!*share) {
		si->list_type = SMB_LIST_SHARES;
	} else if (!dirlen || dir[dirlen - 1] == '/'
		   || dir[dirlen - 1] == '\\') {
		si->list_type = SMB_LIST_DIR;
	}

	if (c_pipe(out_pipe) || c_pipe(err_pipe)) {
		int s_errno = errno;

		if (out_pipe[0] >= 0) close(out_pipe[0]);
		if (out_pipe[1] >= 0) close(out_pipe[1]);
		mem_free(share);
		abort_conn_with_state(conn, -s_errno);
		return;
	}

	conn->from = 0;

	cpid = fork();
	if (cpid == -1) {
		int s_errno = errno;

		close(out_pipe[0]);
		close(out_pipe[1]);
		close(err_pipe[0]);
		close(err_pipe[1]);
		mem_free(share);
		retry_conn_with_state(conn, -s_errno);
		return;
	}

	if (!cpid) {
#define MAX_SMBCLIENT_ARGS 32
		int n = 0;
		unsigned char *v[MAX_SMBCLIENT_ARGS];

		close(1);
		dup2(out_pipe[1], 1);
		close(2);
		dup2(err_pipe[1], 2);
		close(0);
		dup2(open("/dev/null", O_RDONLY), 0);

		close_all_non_term_fd();
		close(out_pipe[0]);
		close(err_pipe[0]);

		v[n++] = "smbclient";

		/* FIXME: handle alloc failures. */
		/* At this point, we are the child process.
		 * Maybe we just don't care if the child kills itself
		 * dereferencing a NULL pointer... -- Miciah */
		/* Leaving random core files after itself is not what a nice
		 * program does. Also, the user might also want to know, why
		 * the hell does he see nothing on the screen. --pasky */

		if (!*share) {
			v[n++] = "-L";	/* get a list of shares available on a host */
			v[n++] = memacpy(conn->uri->host, conn->uri->hostlen);

		} else {
			/* Construct path. */
			asprintf((char **) &v[n++], "//%.*s/%s",
				 conn->uri->hostlen, conn->uri->host, share);
			/* XXX: add password to argument if any. TODO: Recheck
			 * if correct. --Zas. */
			if (conn->uri->passwordlen && !conn->uri->userlen) {
				v[n++] = memacpy(conn->uri->password, conn->uri->passwordlen);
			}
		}

		v[n++] = "-N";		/* don't ask for a password */
		v[n++] = "-E";		/* write messages to stderr instead of stdout */
		v[n++] = "-d 0";	/* disable debug mode. */

		if (conn->uri->portlen) {
			v[n++] = "-p";	/* connect to the specified port */
			v[n++] = memacpy(conn->uri->port, conn->uri->portlen);
		}

		if (conn->uri->userlen) {
			v[n++] = "-U";	/* set the network username */
			if (!conn->uri->passwordlen) {
				/* No password. */
				v[n++] = memacpy(conn->uri->user, conn->uri->userlen);
			} else {
				/* With password. */
				asprintf((char **) &v[n++], "%.*s%%%.*s",
					 conn->uri->userlen, conn->uri->user,
					 conn->uri->passwordlen, conn->uri->password);
			}
		}

		if (*share) {
			/* FIXME: use si->list_type here ?? --Zas */
			if (!dirlen || dir[dirlen - 1] == '/' || dir[dirlen - 1] == '\\') {
				if (dirlen) {
					v[n++] = "-D";	/* start from directory */
					v[n++] = dir;
				}
				v[n++] = "-c"; /* execute semicolon separated commands */
				v[n++] = "ls";

			} else {
				unsigned char *s = straconcat("get \"", dir, "\" -", NULL);
				unsigned char *ss = s;

				v[n++] = "-c"; /* execute semicolon separated commands */
				while ((ss = strchr(ss, '/'))) *ss = '\\';
				v[n++] = s;
			}
		}

		v[n++] = NULL;
		assert(n < MAX_SMBCLIENT_ARGS);

		execvp("smbclient", (char **) v);

		fprintf(stderr, "smbclient not found in $PATH");
		_exit(1);
#undef MAX_SMBCLIENT_ARGS
	}

	mem_free(share);

	conn->data_socket = out_pipe[0];
	conn->socket = err_pipe[0];

	close(out_pipe[1]);
	close(err_pipe[1]);

	set_handlers(out_pipe[0], (void (*)(void *)) smb_got_data, NULL, NULL, conn);
	set_handlers(err_pipe[0], (void (*)(void *)) smb_got_text, NULL, NULL, conn);
	set_connection_state(conn, S_CONN);
}


struct protocol_backend smb_protocol_backend = {
	/* name: */			"smb",
	/* port: */			139,
	/* handler: */			smb_func,
	/* external_handler: */		NULL,
	/* free_syntax: */		0,
	/* need_slashes: */		1,
	/* need_slash_after_host: */	1,
};

#endif /* CONFIG_SMB */
