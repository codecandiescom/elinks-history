/* Internal "ftp" protocol implementation */
/* $Id: ftp.c,v 1.7 2002/03/28 00:46:27 pasky Exp $ */

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

#include <links.h>

#include <config/default.h>
#include <document/cache.h>
#include <lowlevel/connect.h>
#include <lowlevel/select.h>
#include <lowlevel/sched.h>
#include <protocol/ftp.h>
#include <protocol/url.h>
#include <util/error.h>

/* Constants */

#define FTP_BUF_SIZE	16384
#define FTP_DIR_COLOR	"yellow"


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
unsigned char ftp_dirlist_head[] = "<html><head><title>/";
unsigned char ftp_dirlist_head2[] = "</title></head><body><h2>Directory /";
unsigned char ftp_dirlist_head3[] = "</h2><pre>";


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
int get_ftp_response(struct connection *conn, struct read_buffer *rb, int part)
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


/* ftp_func() */
void ftp_func(struct connection *c)
{
	set_timeout(c);

	if (get_keepalive_socket(c)) {
		int p = get_port(c->url);

		if (p == -1) {
			setcstate(c, S_INTERNAL);
			abort_connection(c);
			return;
		}

		make_connection(c, p, &c->sock1, ftp_login);

	} else {
		ftp_send_retr_req(c, S_SENT);
	}
}


/* ftp_login() */
void ftp_login(struct connection *conn)
{
	unsigned char *str;
	unsigned char *cmd = init_str();
	int cmdl = 0;

	if (!cmd) {
		setcstate(conn, S_OUT_OF_MEM);
		abort_connection(conn);
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

	write_to_socket(conn, conn->sock1, cmd, cmdl, ftp_logged);
	mem_free(cmd);
	setcstate(conn, S_SENT);
}


/* ftp_logged() */
void ftp_logged(struct connection *conn)
{
	struct read_buffer *rb;

	rb = alloc_read_buffer(conn);
	if (!rb) return;

	read_from_socket(conn, conn->sock1, rb, ftp_got_info);
}


/* ftp_got_info() */
void ftp_got_info(struct connection *conn, struct read_buffer *rb)
{
	int response = get_ftp_response(conn, rb, 0);

	if (response == -1) {
		setcstate(conn, S_FTP_ERROR);
		abort_connection(conn);
		return;
	}

	if (!response) {
		read_from_socket(conn, conn->sock1, rb, ftp_got_info);
		return;
	}

	if (response >= 400) {
		setcstate(conn, S_FTP_UNAVAIL);
		retry_connection(conn);
		return;
	}

	ftp_got_user_info(conn, rb);
}


/* ftp_got_user_info() */
void ftp_got_user_info(struct connection *conn, struct read_buffer *rb)
{
	int response = get_ftp_response(conn, rb, 0);

	if (response == -1) {
		setcstate(conn, S_FTP_ERROR);
		abort_connection(conn);
		return;
	}

	if (!response) {
		read_from_socket(conn, conn->sock1, rb, ftp_got_user_info);
		return;
	}

	if (response >= 530 && response < 540) {
		setcstate(conn, S_FTP_LOGIN);
		retry_connection(conn);
		return;
	}

	if (response >= 400) {
		setcstate(conn, S_FTP_UNAVAIL);
		retry_connection(conn);
		return;
	}

	if (response >= 200 && response < 300) {
		ftp_send_retr_req(conn, S_GETH);
		return;
	}

	{
		unsigned char *str;
		unsigned char *cmd;
		int cmdl = 0;
		
		cmd = init_str();
		if (!cmd) {
			setcstate(conn, S_OUT_OF_MEM);
			abort_connection(conn);
			return;
		}

		add_to_str(&cmd, &cmdl, "PASS ");
		str = get_pass(conn->url);
		if (str && *str) {
			add_to_str(&cmd, &cmdl, str);
		} else {
			add_to_str(&cmd, &cmdl, default_anon_pass);
		}
		if (str) mem_free(str);
		add_to_str(&cmd, &cmdl, "\r\n");

		write_to_socket(conn, conn->sock1, cmd, cmdl, ftp_sent_passwd);
		mem_free(cmd);
		setcstate(conn, S_LOGIN);
	}
}


/* ftp_sent_passwd() */
void ftp_sent_passwd(struct connection *conn)
{
	struct read_buffer *rb = alloc_read_buffer(conn);
	
	if (!rb) return;
	read_from_socket(conn, conn->sock1, rb, ftp_pass_info);
}


/* ftp_pass_info() */
void ftp_pass_info(struct connection *conn, struct read_buffer *rb)
{
	int response = get_ftp_response(conn, rb, 0);

	if (response == -1) {
		setcstate(conn, S_FTP_ERROR);
		abort_connection(conn);
		return;
	}

	if (!response) {
		read_from_socket(conn, conn->sock1, rb, ftp_pass_info);
		setcstate(conn, S_LOGIN);
		return;
	}

	if (response >= 530 && response < 540) {
		setcstate(conn, S_FTP_LOGIN);
		abort_connection(conn);
		return;
	}

	if (response >= 400) {
		setcstate(conn, S_FTP_UNAVAIL);
		abort_connection(conn);
		return;
	}

	ftp_send_retr_req(conn, S_GETH);
}


/* Create passive socket and add appropriate announcing commands to str. Then
 * go and retrieve appropriate object from server. */
/* Returns NULL if error. */
struct ftp_connection_info *add_file_cmd_to_str(struct connection *conn)
{
	unsigned char *data;
	unsigned char *data_end;
	unsigned char pc[6];
	int pasv_sock;
	struct ftp_connection_info *c_i, *new_c_i;
	unsigned char *str;
	int strl = 0;

	c_i = mem_alloc(sizeof(struct ftp_connection_info));
	if (!c_i) {
		setcstate(conn, S_OUT_OF_MEM);
		abort_connection(conn);
		return NULL;
	}
	memset(c_i, 0, sizeof(struct ftp_connection_info));
	conn->info = c_i;

	str = init_str();
	if (!str) {
		mem_free(c_i);
		setcstate(conn, S_OUT_OF_MEM);
		abort_connection(conn);
		return NULL;
	}

	pasv_sock = get_pasv_socket(conn, conn->sock1, pc);
	if (pasv_sock < 0)
		return NULL;
	conn->sock2 = pasv_sock;

	data = get_url_data(conn->url);
	if (!data) {
		internal("get_url_data failed");
		setcstate(conn, S_INTERNAL);
		abort_connection(conn);
		return NULL;
	}

	data_end = strchr(data, POST_CHAR);
	if (!data_end)
		data_end = data + strlen(data);

	if (data == data_end || data_end[-1] == '/') {
		/* ASCII */

		c_i->dir = 1;
		c_i->pending_commands = 4;

		add_to_str(&str, &strl, "TYPE A\r\n");

		add_to_str(&str, &strl, "PORT ");
		add_num_to_str(&str, &strl, pc[0]);
		add_chr_to_str(&str, &strl, ',');
		add_num_to_str(&str, &strl, pc[1]);
		add_chr_to_str(&str, &strl, ',');
		add_num_to_str(&str, &strl, pc[2]);
		add_chr_to_str(&str, &strl, ',');
		add_num_to_str(&str, &strl, pc[3]);
		add_chr_to_str(&str, &strl, ',');
		add_num_to_str(&str, &strl, pc[4]);
		add_chr_to_str(&str, &strl, ',');
		add_num_to_str(&str, &strl, pc[5]);
		add_to_str(&str, &strl, "\r\n");

		add_to_str(&str, &strl, "CWD /");
		add_bytes_to_str(&str, &strl, data, data_end - data);
		add_to_str(&str, &strl, "\r\n");

		add_to_str(&str, &strl, "LIST\r\n");

		conn->from = 0;

	} else {
		/* BINARY */

		c_i->dir = 0;
		c_i->pending_commands = 3;

		add_to_str(&str, &strl, "TYPE I\r\n");

		add_to_str(&str, &strl, "PORT ");
		add_num_to_str(&str, &strl, pc[0]);
		add_chr_to_str(&str, &strl, ',');
		add_num_to_str(&str, &strl, pc[1]);
		add_chr_to_str(&str, &strl, ',');
		add_num_to_str(&str, &strl, pc[2]);
		add_chr_to_str(&str, &strl, ',');
		add_num_to_str(&str, &strl, pc[3]);
		add_chr_to_str(&str, &strl, ',');
		add_num_to_str(&str, &strl, pc[4]);
		add_chr_to_str(&str, &strl, ',');
		add_num_to_str(&str, &strl, pc[5]);
		add_to_str(&str, &strl, "\r\n");

		if (conn->from) {
			add_to_str(&str, &strl, "REST ");
			add_num_to_str(&str, &strl, conn->from);
			add_to_str(&str, &strl, "\r\n");

			c_i->rest_sent = 1;
			c_i->pending_commands++;
		}

		add_to_str(&str, &strl, "RETR /");
		add_bytes_to_str(&str, &strl, data, data_end - data);
		add_to_str(&str, &strl, "\r\n");
	}

	c_i->opc = c_i->pending_commands;

	new_c_i = mem_realloc(c_i, sizeof(struct ftp_connection_info)
				   + strl + 1);
	if (new_c_i) {
		c_i = new_c_i;
		strcpy(c_i->cmd_buffer, str);
	}
	mem_free(str);
	conn->info = c_i;
	return c_i;
}


/* ftp_send_retr_req() */
void ftp_send_retr_req(struct connection *conn, int state)
{
	struct ftp_connection_info *c_i;
	unsigned char *cmd;
	int cmdl = 0;

	cmd = init_str();
	if (!cmd) {
		setcstate(conn, S_OUT_OF_MEM);
		abort_connection(conn);
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

	write_to_socket(conn, conn->sock1, cmd, strlen(cmd), ftp_retr_1);
	mem_free(cmd);
	setcstate(conn, state);
}


/* ftp_retr_1() */
void ftp_retr_1(struct connection *conn)
{
	struct read_buffer *rb = alloc_read_buffer(conn);

	if (!rb) return;
	read_from_socket(conn, conn->sock1, rb, ftp_retr_file);
}


/* ftp_retr_file() */
void ftp_retr_file(struct connection *conn, struct read_buffer *rb)
{
	struct ftp_connection_info *c_i = conn->info;
	int response;

	if (c_i->pending_commands > 1) {
		response = get_ftp_response(conn, rb, 0);

		if (response == -1) {
			setcstate(conn, S_FTP_ERROR);
			abort_connection(conn);
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

			case 2:	/* PORT */
				if (response >= 400) {
					setcstate(conn, S_FTP_PORT);
					abort_connection(conn);
					return;
				}
				break;

			case 3:	/* REST / CWD */
				if (response >= 400) {
					if (c_i->dir) {
						setcstate(conn, S_FTP_NO_FILE);
						abort_connection(conn);
						return;
					}
					conn->from = 0;
				}
				break;
				
			default:
				internal("WHAT???");
		}
		
		ftp_send_retr_req(conn, S_GETH);
		return;
	}

	response = get_ftp_response(conn, rb, 2);

	if (!response) {
		read_from_socket(conn, conn->sock1, rb, ftp_retr_file);
		setcstate(conn, S_GETH);
		return;
	}

	if (response >= 100 && response < 200) {
		int file_len;
		unsigned char *data = rb->data;
		int pos, pos_file_len = 0;

		/* 150 Opening BINARY mode data connection for hello-1.0-1.1.diff.gz (16452 bytes). */

		for (pos = 0; pos < rb->len && data[pos] != 10; pos++)
			if (data[pos] == '(')
				pos_file_len = pos;

		if (!pos_file_len || pos_file_len == rb->len - 1)
			goto nol;

		pos_file_len++;
		if (data[pos_file_len] < '0' || data[pos_file_len] > '9')
			goto nol;

		for (pos = pos_file_len; pos < rb->len; pos++)
			if (data[pos] < '0' || data[pos] > '9')
				goto quak;
		goto nol;

quak:
		for (; pos < rb->len; pos++)
			if (data[pos] != ' ')
				break;

		if (pos + 4 > rb->len)
			goto nol;

		if (casecmp(&data[pos], "byte", 4))
			goto nol;

		file_len = strtol(&data[pos_file_len], NULL, 10);
		if (file_len && !conn->from) {
			/* FIXME: ..when downloads resuming implemented.. */
			conn->est_length = file_len;
		}
nol:
	}

	set_handlers(conn->sock2,
		     (void (*)(void *)) got_something_from_data_connection,
		     NULL, NULL, conn);

	/* read_from_socket(conn, conn->sock1, rb, ftp_got_final_response); */
	ftp_got_final_response(conn, rb);
}


/* ftp_got_final_response() */
void ftp_got_final_response(struct connection *conn, struct read_buffer *rb)
{
	struct ftp_connection_info *c_i = conn->info;
	int response = get_ftp_response(conn, rb, 0);

	if (response == -1) {
		setcstate(conn, S_FTP_ERROR);
		abort_connection(conn);
		return;
	}

	if (!response) {
		read_from_socket(conn, conn->sock1, rb, ftp_got_final_response);
		if (conn->state != S_TRANS)
			setcstate(conn, S_GETH);
		return;
	}

	if (response == 550) {
		/* Requested action not taken.
		 * File unavailable (e.g., file not found, no access). */

		if (!conn->cache && get_cache_entry(conn->url, &conn->cache)) {
out_of_mem:
			setcstate(conn, S_OUT_OF_MEM);
			abort_connection(conn);
			return;
		}

		if (conn->cache->redirect)
			mem_free(conn->cache->redirect);

		conn->cache->redirect = stracpy(conn->url);
		if (!conn->cache->redirect) goto out_of_mem;

		conn->cache->redirect_get = 1;
		add_to_strn(&conn->cache->redirect, "/");
		conn->cache->incomplete = 0;

		/* setcstate(conn, S_FTP_NO_FILE); */
		setcstate(conn, S_OK);
		abort_connection(conn);
		return;
	}

	if (response >= 400) {
		setcstate(conn, S_FTP_FILE_ERROR);
		abort_connection(conn);
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


/* add_conv_str() */
void add_conv_str(unsigned char **str, int *strl,
                  unsigned char *ostr, int ostrl)
{

#define accept_char(x) ((upcase((x)) >= 'A' && upcase((x)) <= 'Z') ||\
			((x) >= '0' && (x) <= '9') || (x) == ' ' ||\
			 (x) == '-' || (x) == '_' || (x) == '.' ||\
			 (x) == ':' || (x) == ';')

	for (; ostrl; ostrl--, ostr++) {
		if (accept_char(*ostr)) {
			add_chr_to_str(str, strl, *ostr);
		} else {
			add_to_str(str, strl, "&#");
			add_num_to_str(str, strl, (int) *ostr);
			add_chr_to_str(str, strl, ';');
		}
	}

#undef accept_char

}


/* ftp_process_dirlist() */
int ftp_process_dirlist(struct cache_entry *c_e, int *pos, int *dpos,
			unsigned char *buffer, int buflen, int last,
			int *tries)
{
	int ret = 0;

	while (1) {
		unsigned char *str;
		unsigned char *buf = buffer + ret;
		int bufl = buflen - ret;
		int bufp;
		int strl;

		/* Newline quest */

		for (bufp = 0; bufp < bufl; bufp++)
			if (buf[bufp] == '\n')
				break;

		if (buf[bufp] == '\n') {
			ret += bufp + 1;
			if (bufp && buf[bufp - 1] == '\r') bufp--;

		} else {
			if (!bufp || (!last && bufl < FTP_BUF_SIZE))
				return ret;
			ret += bufp;
		}

		/* Process line whose end we've already found */

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
				/* It's not a symlink */
				symlinkpos = bufp;
			}

			add_conv_str(&str, &strl, buf, *dpos);

			add_to_str(&str, &strl, "<a href=\"");
			add_conv_str(&str, &strl, buf + *dpos,
				     symlinkpos - *dpos);
			if (buf[0] == 'd')
				add_chr_to_str(&str, &strl, '/');
			add_to_str(&str, &strl, "\">");

			if (buf[0] == 'd' && color_dirs) {
				/* The <b> is here for the case when we've
				 * use_document_colors off. */
				add_to_str(&str, &strl, "<font color=\""
					   FTP_DIR_COLOR "\"><b>");
			}

			add_conv_str(&str, &strl, buf + *dpos,
				     symlinkpos - *dpos);

			if (buf[0] == 'd' && color_dirs) {
				add_to_str(&str, &strl, "</b></font>");
			}

			add_to_str(&str, &strl, "</a>");

			/* The symlink stuff ;) */
			add_conv_str(&str, &strl, buf + symlinkpos,
				     bufp - symlinkpos);

		} else {
			int symlinkpos;

			if (bufp > strlen("total") &&
			    !casecmp(buf, "total", 5))
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
			add_conv_str(&str, &strl, buf, bufp);
		}

		add_chr_to_str(&str, &strl, '\n');

		if (add_fragment(c_e, *pos, str, strl)) *tries = 0;
		*pos += strl;

		mem_free(str);
	}
}


/* got_something_from_data_connection() */
void got_something_from_data_connection(struct connection *conn)
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
			setcstate(conn, -errno);
			retry_connection(conn);
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
		setcstate(conn, S_OUT_OF_MEM);
		abort_connection(conn);
		return;
	}

	if (c_i->dir && !conn->from) {
		unsigned char *url_data;
		unsigned char *postchar;

#define A(str) { \
	int slen = strlen(str); \
	add_fragment(conn->cache, conn->from, str, slen); \
	conn->from += slen; }

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

#undef A
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
			proceeded = ftp_process_dirlist(conn->cache, &conn->from, &c_i->dpos,
							c_i->ftp_buffer, len + c_i->buf_pos,
							0, &conn->tries);

			if (proceeded == -1) goto out_of_mem;

			memmove(c_i->ftp_buffer, c_i->ftp_buffer + proceeded,
				c_i->buf_pos + len - proceeded);
			c_i->buf_pos += len - proceeded;
		}

		setcstate(conn, S_TRANS);
		return;
	}

	if (ftp_process_dirlist(conn->cache, &conn->from, &c_i->dpos,
				c_i->ftp_buffer, c_i->buf_pos, 1,
				&conn->tries) == -1)
		goto out_of_mem;

	set_handlers(conn->sock2, NULL, NULL, NULL, NULL);
	close_socket(&conn->sock2);

	if (c_i->conn_state == 1) {
		setcstate(conn, S_OK);
		ftp_end_request(conn);
	} else {
		c_i->conn_state = 2;
		setcstate(conn, S_TRANS);
	}

	return;
}


/* ftp_end_request() */
void ftp_end_request(struct connection *conn)
{
	if (conn->state == S_OK) {
		if (conn->cache) {
			truncate_entry(conn->cache, conn->from, 1);
			conn->cache->incomplete = 0;
		}
	}

	add_keepalive_socket(conn, FTP_KEEPALIVE_TIMEOUT);
}
