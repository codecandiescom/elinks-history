/* Internal "ftp" protocol implementation */
/* $Id: ftp.c,v 1.33 2002/09/12 14:24:03 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include "links.h"

#include "config/options.h"
#include "document/cache.h"
#include "lowlevel/connect.h"
#include "lowlevel/select.h"
#include "lowlevel/sched.h"
#include "protocol/ftp.h"
#include "protocol/url.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"

/* Constants */

#define FTP_BUF_SIZE	16384


/* Types and structs */

struct ftp_connection_info {
	int pending_commands; /* Num of commands queued */
	int opc;	/* Total num of commands queued */
	int dir;	/* Directory listing in progress */
	int rest_sent;	/* Sent RESTor command */
	int conn_state;
	int has_data;   /* Do we have data socket? */
	int dpos;
	int buf_pos;
	unsigned char ftp_buffer[FTP_BUF_SIZE];
	unsigned char cmd_buffer[1];
};


/* Global variables */
unsigned char ftp_dirlist_head[] = "<html>\n<head><title>/";
unsigned char ftp_dirlist_head2[] = "</title></head>\n<body>\n<h2>Directory /";
unsigned char ftp_dirlist_head3[] = "</h2>\n<pre>";
unsigned char ftp_dirlist_end[] = "</pre>\n<hr>\n</body>\n</html>";


/* Prototypes */

void ftp_login(struct connection *);
void ftp_sent_passwd(struct connection *);
void ftp_logged(struct connection *);
void ftp_got_reply(struct connection *, struct read_buffer *);
void ftp_got_info(struct connection *, struct read_buffer *);
void ftp_got_user_info(struct connection *, struct read_buffer *);
void ftp_pass_info(struct connection *, struct read_buffer *);
void ftp_send_retr_req(struct connection *, int);
void ftp_retr_1(struct connection *);
void ftp_retr_file(struct connection *, struct read_buffer *);
void ftp_got_final_response(struct connection *, struct read_buffer *);
void got_something_from_data_connection(struct connection *);
void ftp_end_request(struct connection *);

struct ftp_connection_info *add_file_cmd_to_str(struct connection *);


/* Returns 0 if there's no numeric response, -1 if error, the positive response
 * number otherwise. */
int
get_ftp_response(struct connection *conn, struct read_buffer *rb, int part)
{
	int pos;

	set_timeout(conn);

again:
	for (pos = 0; pos < rb->len; pos++) {
		if (rb->data[pos] == 10) {
			unsigned char *num_end;
			int response = strtoul(rb->data, (char **) &num_end, 10);

			if (num_end != rb->data + 3 || response < 100)
				return -1;

			if (*num_end == '-') {
				int i;

				for (i = 0; i < rb->len - 5; i++)
					if (rb->data[i] == 10
					    && !memcmp(rb->data+i+1, rb->data, 3)
					    && rb->data[i+4] == ' ') {
						for (i++; i < rb->len; i++)
							if (rb->data[i] == 10)
								goto ok;
						return 0;
					}
				return 0;
ok:
				pos = i;
			}

			if (!part && response >= 100 && response < 200) {
				kill_buffer_data(rb, pos + 1);
				goto again;
			}

			if (part == 2)
				return response;

			kill_buffer_data(rb, pos + 1);
			return response;
		}
	}

	return 0;
}


/* Initialize or continue ftp connection. */
void
ftp_func(struct connection *conn)
{
	set_timeout(conn);

	if (get_keepalive_socket(conn)) {
		int port = get_port(conn->url);

		if (port == -1) {
			abort_conn_with_state(conn, S_INTERNAL);
			return;
		}

		make_connection(conn, port, &conn->sock1, ftp_login);

	} else {
		ftp_send_retr_req(conn, S_SENT);
	}
}

/* Get connection response. */
void
get_resp(struct connection *conn)
{
	struct read_buffer *rb = alloc_read_buffer(conn);

	if (!rb) return;
	read_from_socket(conn, conn->sock1, rb, conn->read_func);
}


/* Send USER command. */
void
ftp_login(struct connection *conn)
{
	unsigned char *str;
	unsigned char *cmd = init_str();
	int cmdl = 0;

	if (!cmd) {
		abort_conn_with_state(conn, S_OUT_OF_MEM);
		return;
	}

	add_to_str(&cmd, &cmdl, "USER ");
	str = get_user_name(conn->url);

	if (str && *str) {
		add_to_str(&cmd, &cmdl, str);
	} else {
		add_to_str(&cmd, &cmdl, "anonymous");
	}
	if (str) mem_free(str);
	add_to_str(&cmd, &cmdl, "\r\n");

	conn->read_func = (void *) ftp_got_info;
	write_to_socket(conn, conn->sock1, cmd, cmdl, get_resp);
	mem_free(cmd);
	setcstate(conn, S_SENT);
}

/* Parse connection response. */
void
ftp_got_info(struct connection *conn, struct read_buffer *rb)
{
	int response = get_ftp_response(conn, rb, 0);

	if (response == -1) {
		abort_conn_with_state(conn, S_FTP_ERROR);
		return;
	}

	if (!response) {
		read_from_socket(conn, conn->sock1, rb, ftp_got_info);
		return;
	}

	/* RFC959 says that possible response codes on connection are:
	 * 120 Service ready in nnn minutes.
	 * 220 Service ready for new user.
	 * 421 Service not available, closing control connection. */

	if (response != 220) {
		/* TODO? Retry in case of ... ?? */
		retry_conn_with_state(conn, S_FTP_UNAVAIL);
		return;
	}

	ftp_got_user_info(conn, rb);
}


/* Parse USER response and send PASS command if needed. */
void
ftp_got_user_info(struct connection *conn, struct read_buffer *rb)
{
	int response = get_ftp_response(conn, rb, 0);

	if (response == -1) {
		abort_conn_with_state(conn, S_FTP_ERROR);
		return;
	}

	if (!response) {
		read_from_socket(conn, conn->sock1, rb, ftp_got_user_info);
		return;
	}

	/* RFC959 says that possible response codes for USER are:
	 * 230 User logged in, proceed.
	 * 331 User name okay, need password.
	 * 332 Need account for login.
	 * 421 Service not available, closing control connection.
	 * 500 Syntax error, command unrecognized.
	 * 501 Syntax error in parameters or arguments.
	 * 530 Not logged in. */

	/* FIXME? Since ACCT command isn't implemented, login to a ftp server
	 * requiring it will fail (332). */

	if (response == 332 || response >= 500) {
		abort_conn_with_state(conn, S_FTP_LOGIN);
		return;
	}

	/* We don't require exact match here, as this is always error and some
	 * non-RFC compliant servers may return even something other than 421.
	 * --Zas */
	if (response >= 400) {
		abort_conn_with_state(conn, S_FTP_UNAVAIL);
		return;
	}

	if (response == 230) {
		ftp_send_retr_req(conn, S_GETH);
		return;
	}

	{
		/* Send PASS command. */
		unsigned char *str;
		unsigned char *cmd;
		int cmdl = 0;

		cmd = init_str();
		if (!cmd) {
			abort_conn_with_state(conn, S_OUT_OF_MEM);
			return;
		}

		add_to_str(&cmd, &cmdl, "PASS ");
		str = get_pass(conn->url);
		if (str && *str) {
			add_to_str(&cmd, &cmdl, str);
		} else {
			add_to_str(&cmd, &cmdl, get_opt_str("protocol.ftp.anon_passwd"));
		}
		if (str) mem_free(str);
		add_to_str(&cmd, &cmdl, "\r\n");

		conn->read_func = (void *) ftp_pass_info;
		write_to_socket(conn, conn->sock1, cmd, cmdl, get_resp);
		mem_free(cmd);
		setcstate(conn, S_LOGIN);
	}
}

/* Parse PASS command response. */
void
ftp_pass_info(struct connection *conn, struct read_buffer *rb)
{
	int response = get_ftp_response(conn, rb, 0);

	if (response == -1) {
		abort_conn_with_state(conn, S_FTP_ERROR);
		return;
	}

	if (!response) {
		read_from_socket(conn, conn->sock1, rb, ftp_pass_info);
		setcstate(conn, S_LOGIN);
		return;
	}

	/* RFC959 says that possible response codes for PASS are:
	 * 202 Command not implemented, superfluous at this site.
	 * 230 User logged in, proceed.
	 * 332 Need account for login.
	 * 421 Service not available, closing control connection.
	 * 500 Syntax error, command unrecognized.
	 * 501 Syntax error in parameters or arguments.
	 * 503 Bad sequence of commands.
	 * 530 Not logged in. */

	if (response == 332 || response >= 500) {
		abort_conn_with_state(conn, S_FTP_LOGIN);
		return;
	}

	if (response >= 400) {
		abort_conn_with_state(conn, S_FTP_UNAVAIL);
		return;
	}

	ftp_send_retr_req(conn, S_GETH);
}

/* Construct PORT command. */
static void
add_portcmd_to_str(unsigned char **str, int *strl, unsigned char *pc)
{
	/* From RFC 959: DATA PORT (PORT)
	 *
	 * The argument is a HOST-PORT specification for the data port
	 * to be used in data connection.  There are defaults for both
	 * the user and server data ports, and under normal
	 * circumstances this command and its reply are not needed.  If
	 * this command is used, the argument is the concatenation of a
	 * 32-bit internet host address and a 16-bit TCP port address.
	 * This address information is broken into 8-bit fields and the
	 * value of each field is transmitted as a decimal number (in
	 * character string representation).  The fields are separated
	 * by commas.  A port command would be:
	 *
	 *    PORT h1,h2,h3,h4,p1,p2
	 *
	 * where h1 is the high order 8 bits of the internet host
	 * address. */
	add_to_str(str, strl, "PORT ");
	add_num_to_str(str, strl, pc[0]);
	add_chr_to_str(str, strl, ',');
	add_num_to_str(str, strl, pc[1]);
	add_chr_to_str(str, strl, ',');
	add_num_to_str(str, strl, pc[2]);
	add_chr_to_str(str, strl, ',');
	add_num_to_str(str, strl, pc[3]);
	add_chr_to_str(str, strl, ',');
	add_num_to_str(str, strl, pc[4]);
	add_chr_to_str(str, strl, ',');
	add_num_to_str(str, strl, pc[5]);
	add_to_str(str, strl, "\r\n");
}

#ifdef IPV6
/* Construct EPRT command. */
static void
add_eprtcmd_to_str(unsigned char **str, int *strl, struct sockaddr_in6 *addr)
{
	unsigned char addr_str[INET6_ADDRSTRLEN];

	inet_ntop(AF_INET6, &addr->sin6_addr, addr_str, INET6_ADDRSTRLEN);

	/* From RFC 2428: EPRT
	 *
	 * The format of EPRT is:
	 *
	 * EPRT<space><d><net-prt><d><net-addr><d><tcp-port><d>
	 *
	 * <net-prt>:
	 * AF Number   Protocol
	 * ---------   --------
	 * 1           Internet Protocol, Version 4 [Pos81a]
	 * 2           Internet Protocol, Version 6 [DH96] */
	add_to_str(str, strl, "EPRT |2|");
	add_to_str(str, strl, addr_str);
	add_chr_to_str(str, strl, '|');
	add_num_to_str(str, strl, ntohs(addr->sin6_port));
	add_to_str(str, strl, "|\r\n");
}
#endif

/* Create passive socket and add appropriate announcing commands to str. Then
 * go and retrieve appropriate object from server.
 * Returns NULL if error. */
struct ftp_connection_info *
add_file_cmd_to_str(struct connection *conn)
{
#ifdef IPV6
	struct sockaddr_in6 data_addr;
#endif
	unsigned char pc[6];
	unsigned char *data;
	unsigned char *data_end;
	int data_sock;
	struct ftp_connection_info *c_i;
	unsigned char *str;
	int strl = 0;

	c_i = mem_alloc(sizeof(struct ftp_connection_info));
	if (!c_i) {
		abort_conn_with_state(conn, S_OUT_OF_MEM);
		return NULL;
	}
	memset(c_i, 0, sizeof(struct ftp_connection_info));
	conn->info = c_i;

	str = init_str();
	if (!str) {
		mem_free(c_i);
		abort_conn_with_state(conn, S_OUT_OF_MEM);
		return NULL;
	}

#ifdef IPV6
	memset(&data_addr, 0, sizeof(struct sockaddr_in6));
#endif
	memset(pc, 0, 6);

#ifdef IPV6
	if (conn->pf == 2)
		data_sock = get_pasv6_socket(conn, conn->sock1,
				(struct sockaddr_storage *) &data_addr);
	else
#endif
		data_sock = get_pasv_socket(conn, conn->sock1, pc);
	if (data_sock < 0)
		return NULL;
	conn->sock2 = data_sock;

	data = get_url_data(conn->url);
	if (!data) {
		internal("get_url_data failed");
		abort_conn_with_state(conn, S_INTERNAL);
		return NULL;
	}

	data_end = strchr(data, POST_CHAR);
	if (!data_end)
		data_end = data + strlen(data);

	if (data == data_end || data_end[-1] == '/') {
		/* Commands to get directory listing. */

		c_i->dir = 1;
		c_i->pending_commands = 4;

		/* ASCII */
		add_to_str(&str, &strl, "TYPE A\r\n");

		add_to_str(&str, &strl, "CWD /");
		add_bytes_to_str(&str, &strl, data, data_end - data);
		add_to_str(&str, &strl, "\r\n");

#ifdef IPV6
		if (conn->pf == 2)
			add_eprtcmd_to_str(&str, &strl, &data_addr);
		else
#endif
			add_portcmd_to_str(&str, &strl, pc);

		add_to_str(&str, &strl, "LIST\r\n");

		conn->from = 0;

	} else {
		/* Commands to get a file. */

		c_i->dir = 0;
		c_i->pending_commands = 3;

		/* BINARY */
		add_to_str(&str, &strl, "TYPE I\r\n");

		if (conn->from) {
			add_to_str(&str, &strl, "REST ");
			add_num_to_str(&str, &strl, conn->from);
			add_to_str(&str, &strl, "\r\n");

			c_i->rest_sent = 1;
			c_i->pending_commands++;
		}

#ifdef IPV6
		if (conn->pf == 2)
			add_eprtcmd_to_str(&str, &strl, &data_addr);
		else
#endif
			add_portcmd_to_str(&str, &strl, pc);

		add_to_str(&str, &strl, "RETR /");
		add_bytes_to_str(&str, &strl, data, data_end - data);
		add_to_str(&str, &strl, "\r\n");
	}

	c_i->opc = c_i->pending_commands;

	c_i = mem_realloc(c_i, sizeof(struct ftp_connection_info)
			       + strl + 1);
	if (!c_i) {
		mem_free(str);
		abort_conn_with_state(conn, S_OUT_OF_MEM);
		return NULL;
	}

	strcpy(c_i->cmd_buffer, str);
	mem_free(str);
	conn->info = c_i;

	return c_i;
}


/* Send commands to retrieve file or directory. */
void
ftp_send_retr_req(struct connection *conn, int state)
{
	struct ftp_connection_info *c_i;
	unsigned char *cmd;
	int cmdl = 0;

	cmd = init_str();
	if (!cmd) {
		abort_conn_with_state(conn, S_OUT_OF_MEM);
		return;
	}

	/* We don't save return value from add_file_cmd_to_str(), as it's saved
	 * in conn->info as well. */
	if (!conn->info && !add_file_cmd_to_str(conn)) {
		mem_free(cmd);
		return;
	}
	c_i = conn->info;

	{
		/* Send it line-by-line. */
		unsigned char *nl = strchr(c_i->cmd_buffer, '\n');

		if (!nl) {
			add_to_str(&cmd, &cmdl, c_i->cmd_buffer);
		} else {
			nl++;
			add_bytes_to_str(&cmd, &cmdl, c_i->cmd_buffer,
					 nl - c_i->cmd_buffer);
			memmove(c_i->cmd_buffer, nl, strlen(nl) + 1);
		}
	}

	conn->read_func = (void *) ftp_retr_file;
	write_to_socket(conn, conn->sock1, cmd, strlen(cmd), get_resp);
	mem_free(cmd);
	setcstate(conn, state);
}

/* Parse RETR response and return file size or -1 on error. */
long int
get_filesize_from_RETR(unsigned char *data, int data_len)
{
	long int file_len;
	int pos, pos_file_len = 0;

	/* Getting file size from text response.. */
	/* 150 Opening BINARY mode data connection for hello-1.0-1.1.diff.gz (16452 bytes). */

	for (pos = 0; pos < data_len && data[pos] != 10; pos++)
		if (data[pos] == '(')
			pos_file_len = pos;

	if (!pos_file_len || pos_file_len == data_len - 1)
		return -1;

	pos_file_len++;
	if (data[pos_file_len] < '0' || data[pos_file_len] > '9')
		return -1;

	for (pos = pos_file_len; pos < data_len; pos++)
		if (data[pos] < '0' || data[pos] > '9')
			goto next;

	return -1;

next:
	for (; pos < data_len; pos++)
		if (data[pos] != ' ')
			break;

	if (pos + 4 > data_len)
		return -1;

	if (strncasecmp(&data[pos], "byte", 4))
		return -1;

	file_len = strtol(&data[pos_file_len], NULL, 10);

	if (errno == ERANGE) return -1;

	return file_len;
}

void
ftp_retr_file(struct connection *conn, struct read_buffer *rb)
{
	struct ftp_connection_info *c_i = conn->info;
	int response;

	if (c_i->pending_commands > 1) {
		response = get_ftp_response(conn, rb, 0);

		if (response == -1) {
			abort_conn_with_state(conn, S_FTP_ERROR);
			return;
		}

		if (!response) {
			read_from_socket(conn, conn->sock1, rb, ftp_retr_file);
			setcstate(conn, S_GETH);
			return;
		}

		c_i->pending_commands--;

		/* XXX: The case values are order numbers of commands. */
		switch (c_i->opc - c_i->pending_commands) {
			case 1:	/* TYPE */
				break;

			case 2:	/* REST / CWD */
				if (response >= 400) {
					if (c_i->dir) {
						abort_conn_with_state(conn, S_FTP_NO_FILE);
						return;
					}
					conn->from = 0;
				}
				break;

			case 3:	/* PORT */
				if (response >= 400) {
					abort_conn_with_state(conn, S_FTP_PORT);
					return;
				}
				break;

			default:
				internal("WHAT???");
		}

		ftp_send_retr_req(conn, S_GETH);
		return;
	}

	response = get_ftp_response(conn, rb, 2);

	if (response == -1) {
		abort_conn_with_state(conn, S_FTP_ERROR);
		return;
	}

	if (!response) {
		read_from_socket(conn, conn->sock1, rb, ftp_retr_file);
		setcstate(conn, S_GETH);
		return;
	}

	if (response >= 100 && response < 200) {
		/* We only need to parse response after RETR to
		 * get filesize if needed. */
		if (!c_i->dir && !conn->from) {
			long int file_len =
				get_filesize_from_RETR(rb->data, rb->len);

			if (file_len > 0) {
				/* FIXME: ..when downloads resuming
				 * implemented.. */
				conn->est_length = file_len;
			}
		}
	}

	set_handlers(conn->sock2,
		     (void (*)(void *)) got_something_from_data_connection,
		     NULL, NULL, conn);

	/* read_from_socket(conn, conn->sock1, rb, ftp_got_final_response); */
	ftp_got_final_response(conn, rb);
}

void
ftp_got_final_response(struct connection *conn, struct read_buffer *rb)
{
	struct ftp_connection_info *c_i = conn->info;
	int response = get_ftp_response(conn, rb, 0);

	if (response == -1) {
		abort_conn_with_state(conn, S_FTP_ERROR);
		return;
	}

	if (!response) {
		read_from_socket(conn, conn->sock1, rb, ftp_got_final_response);
		if (conn->state != S_TRANS)
			setcstate(conn, S_GETH);
		return;
	}

	if (response >= 550) {
		/* Requested action not taken.
		 * File unavailable (e.g., file not found, no access). */

		if (!conn->cache && get_cache_entry(conn->url, &conn->cache)) {
			abort_conn_with_state(conn, S_OUT_OF_MEM);
			return;
		}

		if (conn->cache->redirect)
			mem_free(conn->cache->redirect);

		conn->cache->redirect = stracpy(conn->url);
		if (!conn->cache->redirect) {
			abort_conn_with_state(conn, S_OUT_OF_MEM);
			return;
		}

		conn->cache->redirect_get = 1;
		add_to_strn(&conn->cache->redirect, "/");
		conn->cache->incomplete = 0;

		/* setcstate(conn, S_FTP_NO_FILE); */
		abort_conn_with_state(conn, S_OK);
		return;
	}

	if (response >= 400) {
		abort_conn_with_state(conn, S_FTP_FILE_ERROR);
		return;
	}

	if (c_i->conn_state == 2) {
		setcstate(conn, S_OK);
		ftp_end_request(conn);
	} else {
		c_i->conn_state = 1;
		if (conn->state != S_TRANS)
			setcstate(conn, S_GETH);
	}

	return;
}


/* List a directory in html format. */
int
ftp_process_dirlist(struct cache_entry *c_e, int *pos, int *dpos,
		    unsigned char *buffer, int buflen, int last,
		    int *tries)
{
	int ret = 0;
	unsigned char dircolor[8];
	int colorize_dir = get_opt_int("document.browse.links.color_dirs");

	if (colorize_dir) {
		color_to_string((struct rgb *) get_opt_ptr("document.colors.dirs"),
				(unsigned char *) &dircolor);
	}

	while (1) {
		unsigned char *str;
		unsigned char *buf = buffer + ret;
		int bufl = buflen - ret;
		int bufp;
		int strl;
		int newline = 0;

		/* Newline quest. */

		for (bufp = 0; bufp < bufl; bufp++) {
			if (buf[bufp] == '\n') {
				newline = 1;
				break;
			}
		}

		if (newline) {
			ret += bufp + 1;
			if (bufp && buf[bufp - 1] == '\r') bufp--;
		} else {
			if (!bufp || (!last && bufl < FTP_BUF_SIZE))
				return ret;
			ret += bufp;
		}

		/* Process line whose end we've already found. */

		str = init_str();
		if (!str) return -1;

		strl = 0;
		/* add_to_str(&str, &strl, "   "); */

		if (*dpos && *dpos < bufp && WHITECHAR(buf[*dpos - 1])) {
			int symlinkpos;
direntry:
			for (symlinkpos = *dpos;
			     symlinkpos <= bufp - strlen(" -> "); symlinkpos++)
				if (!memcmp(buf + symlinkpos, " -> ", 4))
					break;

			if (symlinkpos > bufp - strlen(" -> ")) {
				/* It's not a symlink. */
				symlinkpos = bufp;
			}

			add_htmlesc_str(&str, &strl, buf, *dpos);

			add_to_str(&str, &strl, "<a href=\"");
			add_htmlesc_str(&str, &strl, buf + *dpos,
					symlinkpos - *dpos);
			if (buf[0] == 'd')
				add_chr_to_str(&str, &strl, '/');
			add_to_str(&str, &strl, "\">");

			if (buf[0] == 'd' && colorize_dir) {
				/* The <b> is here for the case when we've
				 * use_document_colors off. */
				add_to_str(&str, &strl, "<font color=\"");
				add_to_str(&str, &strl, dircolor);
				add_to_str(&str, &strl, "\"><b>");
			}

			add_htmlesc_str(&str, &strl, buf + *dpos,
					symlinkpos - *dpos);

			if (buf[0] == 'd' && colorize_dir) {
				add_to_str(&str, &strl, "</b></font>");
			}

			add_to_str(&str, &strl, "</a>");

			/* The symlink stuff ;) */
			add_htmlesc_str(&str, &strl, buf + symlinkpos,
					bufp - symlinkpos);

		} else {
			int symlinkpos;

			if (bufp > strlen("total") &&
			    !strncasecmp(buf, "total", 5))
				goto rawentry;

			for (symlinkpos = bufp - 1; symlinkpos >= 0;
			     symlinkpos--)
				if (!WHITECHAR(buf[symlinkpos]))
					break;
			if (symlinkpos < 0)
				goto rawentry;

			for (; symlinkpos >= 0; symlinkpos--)
				if (WHITECHAR(buf[symlinkpos])
				    && (symlinkpos < 3
					|| memcmp(buf + symlinkpos - 3, " -> ", 4))
				    && (symlinkpos > bufp - 4
					|| memcmp(buf + symlinkpos, " -> ", 4)))
					break;
			*dpos = symlinkpos + 1;
			goto direntry;
rawentry:
			add_htmlesc_str(&str, &strl, buf, bufp);
		}

		add_chr_to_str(&str, &strl, '\n');

		if (add_fragment(c_e, *pos, str, strl)) *tries = 0;
		*pos += strl;

		mem_free(str);
	}
}

void
got_something_from_data_connection(struct connection *conn)
{
	struct ftp_connection_info *c_i = conn->info;
	int len;

	/* XXX: This probably belongs rather to connect.c ? */

	set_timeout(conn);

	if (!c_i->has_data) {
		int newsock;

		c_i->has_data = 1;

		set_handlers(conn->sock2, NULL, NULL, NULL, NULL);
		newsock = accept(conn->sock2, NULL, NULL);
		if (newsock < 0) {
error:
			retry_conn_with_state(conn, -errno);
			return;
		}

		close(conn->sock2);
		conn->sock2 = newsock;

		set_handlers(newsock,
			     (void (*)(void *)) got_something_from_data_connection,
			     NULL, NULL, conn);
		return;
	}

	if (!conn->cache && get_cache_entry(conn->url, &conn->cache)) {
out_of_mem:
		abort_conn_with_state(conn, S_OUT_OF_MEM);
		return;
	}

#define A(str) { \
	int slen = strlen(str); \
	add_fragment(conn->cache, conn->from, str, slen); \
	conn->from += slen; }

	if (c_i->dir && !conn->from) {
		unsigned char *url_data;
		unsigned char *postchar;

		url_data = stracpy(get_url_data(conn->url));
		if (!url_data) goto out_of_mem;

		postchar = strchr(url_data, POST_CHAR);
		if (postchar) *postchar = 0;

		A(ftp_dirlist_head);
		if (url_data) A(url_data);
		A(ftp_dirlist_head2);
		if (url_data) A(url_data);
		A(ftp_dirlist_head3);

		if (url_data)
			mem_free(url_data);

		if (!conn->cache->head) {
			conn->cache->head = stracpy("\r\n");
			if (!conn->cache->head) goto out_of_mem;
		}

		add_to_strn(&conn->cache->head, "Content-Type: text/html\r\n");
	}

	len = read(conn->sock2, c_i->ftp_buffer + c_i->buf_pos,
		   FTP_BUF_SIZE - c_i->buf_pos);

	if (len < 0)
		goto error;

	if (len > 0) {
		if (!c_i->dir) {
			conn->received += len;
			if (add_fragment(conn->cache, conn->from,
					 c_i->ftp_buffer, len) == 1)
				conn->tries = 0;
			conn->from += len;

		} else {
			int proceeded;

			conn->received += len;
			proceeded = ftp_process_dirlist(conn->cache,
							&conn->from,
							&c_i->dpos,
							c_i->ftp_buffer,
							len + c_i->buf_pos,
							0, &conn->tries);

			if (proceeded == -1) goto out_of_mem;

			c_i->buf_pos += len - proceeded;

			memmove(c_i->ftp_buffer, c_i->ftp_buffer + proceeded,
				c_i->buf_pos);

		}

		setcstate(conn, S_TRANS);
		return;
	}

	if (ftp_process_dirlist(conn->cache, &conn->from, &c_i->dpos,
				c_i->ftp_buffer, c_i->buf_pos, 1,
				&conn->tries) == -1)
		goto out_of_mem;

	if (c_i->dir) A(ftp_dirlist_end);

#undef A

	set_handlers(conn->sock2, NULL, NULL, NULL, NULL);
	close_socket(NULL, &conn->sock2);

	if (c_i->conn_state == 1) {
		setcstate(conn, S_OK);
		ftp_end_request(conn);
	} else {
		c_i->conn_state = 2;
		setcstate(conn, S_TRANS);
	}

	return;
}

void
ftp_end_request(struct connection *conn)
{
	if (conn->state == S_OK) {
		if (conn->cache) {
			truncate_entry(conn->cache, conn->from, 1);
			conn->cache->incomplete = 0;
		}
	}

	add_keepalive_socket(conn, FTP_KEEPALIVE_TIMEOUT);
}
