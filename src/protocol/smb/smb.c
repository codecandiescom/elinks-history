/* Internal SMB protocol implementation */
/* $Id: smb.c,v 1.16 2003/12/09 13:38:59 pasky Exp $ */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* Needed for asprintf() */
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h> /* FreeBSD needs this before resource.h */
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

#include "lowlevel/connect.h"
#include "lowlevel/select.h"
#include "osdep/osdep.h"
#include "protocol/protocol.h"
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

#define READ_SIZE	4096

static int
smb_read_data(struct connection *conn, int sock, unsigned char *dst, ssize_t len)
{
	int r;
	struct smb_connection_info *si = conn->info;

	r = read(sock, dst, len);
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

	r = smb_read_data(conn, sock, si->text + si->textlen, READ_SIZE);
	if (r <= 0) return;

	if (!conn->from) set_connection_state(conn, S_GETH);
	si->textlen += r;
}

/* Return 0 if @conn->cache was set. */
static int
smb_get_cache(struct connection *conn)
{
	if (conn->cache) return 0;

	conn->cache = get_cache_entry(struri(conn->uri));
	if (conn->cache) return 0;

	abort_conn_with_state(conn, S_OUT_OF_MEM);
	return -1;
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

	r = smb_read_data(conn, conn->data_socket, buffer, READ_SIZE);
	if (r <= 0) return;

	set_connection_state(conn, S_TRANS);

	if (smb_get_cache(conn)) return;

	conn->received += r;
	if (add_fragment(conn->cache, conn->from, buffer, r) == 1)
		conn->tries = 0;
	conn->from += r;
}

#undef READ_SIZE

static void
smb_got_text(struct connection *conn)
{
	smb_read_text(conn, conn->socket);
}

/* FIXME: split it. --Zas */
static void
end_smb_connection(struct connection *conn)
{
	struct smb_connection_info *si = conn->info;

	if (smb_get_cache(conn)) return;

	if (conn->from) {
		truncate_entry(conn->cache, conn->from, 1);
		conn->cache->incomplete = 0;
		goto bye;
	}

	if (si->textlen && si->text[si->textlen - 1] != '\n')
		si->text[si->textlen++] = '\n';
	si->text[si->textlen] = '\0';

	/* FIXME: what to do if conn->uri.datalen is zero ? --Zas */
	if ((strstr(si->text, "NT_STATUS_FILE_IS_A_DIRECTORY")
	     || strstr(si->text, "NT_STATUS_ACCESS_DENIED")
	     || strstr(si->text, "ERRbadfile"))
	    && *struri(conn->uri)
	    && conn->uri.data[conn->uri.datalen - 1] != '/'
	    && conn->uri.data[conn->uri.datalen - 1] != '\\') {
		if (conn->cache->redirect) mem_free(conn->cache->redirect);
		conn->cache->redirect = stracpy(struri(conn->uri));
		conn->cache->redirect_get = 1;
		add_to_strn(&conn->cache->redirect, "/");
		conn->cache->incomplete = 0;

	} else {
		unsigned char *line_start, *line_end, *line_end2;
		struct string page;
		int type = 0;
		int pos = 0;

		if (!init_string(&page)) {
			abort_conn_with_state(conn, S_OUT_OF_MEM);
			return;
		}

		add_to_string(&page, "<html><head><title>/");
		add_bytes_to_string(&page, conn->uri.data, conn->uri.datalen);
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
				unsigned char *ll, *lll;

				if (!*line) type = 0;
				if (strstr(line, "Sharename")
				    && strstr(line, "Type")) {
					if (strstr(line, "Type")) {
						pos = (unsigned char *)
							strstr(line, "Type") - line;
					} else {
						pos = 0;
					}
					type = 1;
					goto print_as_is;
				}
				if (strstr(line, "Server")
				    && strstr(line, "Comment")) {
					type = 2;
					goto print_as_is;
				}
				if (strstr(line, "Workgroup")
				    && strstr(line, "Master")) {
					pos = (unsigned char *) strstr(line, "Master") - line;
					type = 3;
					goto print_as_is;
				}

				if (!type) goto print_as_is;
				for (ll = line; *ll; ll++)
					if (!WHITECHAR(*ll) && *ll != '-')
						goto np;
				goto print_as_is;
np:

				for (ll = line; *ll; ll++)
					if (!WHITECHAR(*ll))
						break;

				for (lll = ll; *lll/* && lll[1]*/; lll++)
					if (WHITECHAR(*lll)/* && WHITECHAR(lll[1])*/)
						break;

				switch (type) {
				case 1:
				{
					unsigned char *llll;

					if (!strstr(lll, "Disk"))
						goto print_as_is;

					if (pos && pos < strlen(line)
					    && WHITECHAR(*(llll = line + pos - 1))
					    && llll > ll) {
						while (llll > ll && WHITECHAR(*llll))
							llll--;
						if (!WHITECHAR(*llll))
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

				case 3:
					if (pos < strlen(line) && pos
					    && WHITECHAR(line[pos - 1])
					    && !WHITECHAR(line[pos])) {
						ll = line + pos;
					} else {
						for (ll = lll; *ll; ll++)
							if (!WHITECHAR(*ll))
								break;
					}
					for (lll = ll; *lll; lll++)
						if (WHITECHAR(*lll))
							break;
					/* Fall-through */

				case 2:
					add_bytes_to_string(&page, line, ll - line);
					add_to_string(&page, "<a href=\"smb://");
					add_bytes_to_string(&page, ll, lll - ll);
					add_to_string(&page, "/\">");
					add_bytes_to_string(&page, ll, lll - ll);
					add_to_string(&page, "</a>");
					add_to_string(&page, lll);
					break;

				default:
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

		add_fragment(conn->cache, 0, page.source, page.length);
		conn->from += page.length;
		truncate_entry(conn->cache, page.length, 1);
		conn->cache->incomplete = 0;
		done_string(&page);

		if (!conn->cache->head)
			conn->cache->head = stracpy("\r\n");
		add_to_strn(&conn->cache->head, "Content-Type: text/html\r\n");
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

	p = strchr(conn->uri.data, '/');
	if (p && p - conn->uri.data < conn->uri.datalen) {
		share = memacpy(conn->uri.data, p - conn->uri.data);
		dir = p + 1;
		/* FIXME: ensure @dir do not contain dangerous chars. --Zas */

	} else if (conn->uri.datalen) {
		if (smb_get_cache(conn)) return;

		if (conn->cache->redirect) mem_free(conn->cache->redirect);
		conn->cache->redirect = stracpy(struri(conn->uri));
		conn->cache->redirect_get = 1;
		add_to_strn(&conn->cache->redirect, "/");

		conn->cache->incomplete = 0;
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

		if (!*share) {
			v[n++] = "-L";	/* get a list of shares available on a host */
			v[n++] = memacpy(conn->uri.host, conn->uri.hostlen);

		} else {
			/* Construct path. */
			asprintf((char **) &v[n++], "//%.*s/%s",
				 conn->uri.hostlen, conn->uri.host, share);
			/* XXX: add password to argument if any. TODO: Recheck
			 * if correct. --Zas. */
			if (conn->uri.passwordlen && !conn->uri.userlen) {
				v[n++] = memacpy(conn->uri.password, conn->uri.passwordlen);
			}
		}

		v[n++] = "-N";		/* don't ask for a password */
		v[n++] = "-E";		/* write messages to stderr instead of stdout */
		v[n++] = "-d 0";	/* disable debug mode. */

		if (conn->uri.portlen) {
			v[n++] = "-p";	/* connect to the specified port */
			v[n++] = memacpy(conn->uri.port, conn->uri.portlen);
		}

		if (conn->uri.userlen) {
			v[n++] = "-U";	/* set the network username */
			if (!conn->uri.passwordlen) {
				/* No password. */
				v[n++] = memacpy(conn->uri.user, conn->uri.userlen);
			} else {
				/* With password. */
				asprintf((char **) &v[n++], "%.*s%%%.*s",
					 conn->uri.userlen, conn->uri.user,
					 conn->uri.passwordlen, conn->uri.password);
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
				unsigned char *ss;

				v[n++] = "-c"; /* execute semicolon separated commands */
				while ((ss = strchr(s, '/'))) *ss = '\\';
				v[n++] = s;
			}
		}

		v[n++] = NULL;
		assert(n < MAX_SMBCLIENT_ARGS);

		execvp("smbclient", (char **) v);

		fprintf(stderr, "smbclient not found in $PATH");
		_exit(1);
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
	/* port: */			0,
	/* handler: */			smb_func,
	/* external_handler: */		NULL,
	/* free_syntax: */		0,
	/* need_slashes: */		1,
	/* need_slash_after_host: */	1,
};
