#include "links.h"

#define FTP_BUF	16384

struct ftp_connection_info {
	int pending_commands; /* Num of commands queued */
	int opc;	/* Total num of commands queued */
	int dir;	/* Directory listing in progress */
	int rest_sent;	/* Sent RESTor command */
	int conn_state;
	int has_data;   /* Do we have data socket? */
	int dpos;
	int buf_pos;
	unsigned char ftp_buffer[FTP_BUF];
};

void ftp_login(struct connection *);
void ftp_logged(struct connection *);
void ftp_got_reply(struct connection *, struct read_buffer *);
void ftp_got_info(struct connection *, struct read_buffer *);
void ftp_got_user_info(struct connection *, struct read_buffer *);
void ftp_dummy_info(struct connection *, struct read_buffer *);
void ftp_pass_info(struct connection *, struct read_buffer *);
void ftp_send_retr_req(struct connection *);
int add_file_cmd_to_str(struct connection *, unsigned char **, int *);
void ftp_retr_1(struct connection *);
void ftp_retr_file(struct connection *, struct read_buffer *);
void ftp_got_final_response(struct connection *, struct read_buffer *);
void got_something_from_data_connection(struct connection *);
void ftp_end_request(struct connection *);

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

void ftp_func(struct connection *c)
{
	/* setcstate(c, S_CONN); */
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
		ftp_send_retr_req(c);
	}
}

void ftp_login(struct connection *conn)
{
	unsigned char *str;
	unsigned char *cmd;
	int cmdl = 0;
	
	if (!(cmd = init_str())) {
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
	
	add_to_str(&cmd, &cmdl, "PASS ");
	str = get_pass(conn->url);
	if (str && *str) {
		add_to_str(&cmd, &cmdl, str);
	} else {
		add_to_str(&cmd, &cmdl, default_anon_pass);
	}
	if (str) mem_free(str);
	add_to_str(&cmd, &cmdl, "\r\n");
	
	if (add_file_cmd_to_str(conn, &cmd, &cmdl) < 0) {
		mem_free(cmd);
		return;
	}
	
	write_to_socket(conn, conn->sock1, cmd, strlen(cmd), ftp_logged);
	mem_free(cmd);
	setcstate(conn, S_SENT);
}

void ftp_logged(struct connection *conn)
{
	struct read_buffer *rb;
	
	rb = alloc_read_buffer(conn);
	if (!rb) return;
	
	read_from_socket(conn, conn->sock1, rb, ftp_got_info);
}

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
		ftp_dummy_info(conn, rb);
		return;
	}

	ftp_pass_info(conn, rb);
}

void ftp_dummy_info(struct connection *conn, struct read_buffer *rb)
{
	int response = get_ftp_response(conn, rb, 0);
	
	if (response == -1) {
		setcstate(conn, S_FTP_ERROR);
		abort_connection(conn);
		return;
	}
	
	if (!response) {
		read_from_socket(conn, conn->sock1, rb, ftp_dummy_info);
		return;
	}
	
	ftp_retr_file(conn, rb);
}

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
	
	ftp_retr_file(conn, rb);
}

/* Create passive socket and add appropriate announcing commands to str. Then
 * go and retrieve appropriate object from server. */
/* Returns -1 if error, 0 for success. */
int add_file_cmd_to_str(struct connection *conn, unsigned char **str, int *strl)
{
	unsigned char *data;
	unsigned char *data_end;
	unsigned char pc[6];
	int pasv_sock;
	struct ftp_connection_info *c_i;

	c_i = mem_alloc(sizeof(struct ftp_connection_info));
	if (!c_i) {
		setcstate(conn, S_OUT_OF_MEM);
		abort_connection(conn);
		return -1;
	}
	memset(c_i, 0, sizeof(struct ftp_connection_info));
	conn->info = c_i;
	
	pasv_sock = get_pasv_socket(conn, conn->sock1, pc);
	if (pasv_sock < 0)
		return pasv_sock /* -1 */;
	conn->sock2 = pasv_sock;
	
	data = get_url_data(conn->url);
	if (!data) {
		internal("get_url_data failed");
		setcstate(conn, S_INTERNAL);
		abort_connection(conn);
		return -1;
	}
	
	data_end = strchr(data, POST_CHAR);
	if (!data_end)
		data_end = data + strlen(data);
	
	if (data == data_end || data_end[-1] == '/') {
		/* ASCII */
		
		c_i->dir = 1;
		c_i->pending_commands = 4;
		
		add_to_str(str, strl, "TYPE A\r\n");
		
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
		
		add_to_str(str, strl, "CWD /");
		add_bytes_to_str(str, strl, data, data_end - data);
		add_to_str(str, strl, "\r\n");
		
		add_to_str(str, strl, "LIST\r\n");

		conn->from = 0;
		
	} else {
		/* BINARY */
		
		c_i->dir = 0;
		c_i->pending_commands = 3;
		
		add_to_str(str, strl, "TYPE I\r\n");
		
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
		
		if (conn->from) {
			add_to_str(str, strl, "REST ");
			add_num_to_str(str, strl, conn->from);
			add_to_str(str, strl, "\r\n");
			
			c_i->rest_sent = 1;
			c_i->pending_commands++;
		}
		
		add_to_str(str, strl, "RETR /");
		add_bytes_to_str(str, strl, data, data_end - data);
		add_to_str(str, strl, "\r\n");
	}
	
	c_i->opc = c_i->pending_commands;

	return 0;
}


void ftp_send_retr_req(struct connection *conn)
{
	unsigned char *cmd;
	int cmdl = 0;
	
	cmd = init_str();
	if (!cmd) {
		setcstate(conn, S_OUT_OF_MEM);
		abort_connection(conn);
		return;
	}
	
	if (add_file_cmd_to_str(conn, &cmd, &cmdl) < 0) {
		mem_free(cmd);
		return;
	}
	
	write_to_socket(conn, conn->sock1, cmd, strlen(cmd), ftp_retr_1);
	mem_free(cmd);
	setcstate(conn, S_SENT);
}

void ftp_retr_1(struct connection *conn)
{
	struct read_buffer *rb;
	
	if (!(rb = alloc_read_buffer(conn))) return;
	read_from_socket(conn, conn->sock1, rb, ftp_retr_file);
}

void ftp_retr_file(struct connection *conn, struct read_buffer *rb)
{
	struct ftp_connection_info *c_i = conn->info;
	int response;
	
rep:
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
				goto rep;
				
			case 2:	/* PORT */
				if (response >= 400) {
					setcstate(conn, S_FTP_PORT);
					abort_connection(conn);
					return;
				}
				goto rep;
				
			case 3:	/* REST / CWD */
				if (response >= 400) {
					if (c_i->dir) {
						setcstate(conn, S_FTP_NO_FILE);
						abort_connection(conn);
						return;
					}
					conn->from = 0;
				}
				goto rep;
		}
		
		internal("WHAT???");
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
			setcstate(conn, S_OUT_OF_MEM);
			abort_connection(conn);
			return;
		}
		
		if (conn->cache->redirect)
			mem_free(conn->cache->redirect);
		
		conn->cache->redirect = stracpy(conn->url);
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
}

#define accept_char(x) ((upcase((x)) >= 'A' && upcase((x)) <= 'Z') ||\
			((x) >= '0' && (x) <= '9') || (x) == ' ' ||\
			 (x) == '-' || (x) == '_' || (x) == '.' ||\
			 (x) == ':' || (x) == ';')

void add_conv_str(unsigned char **str, int *strl,
		  unsigned char *ostr, int ostrl)
{
	for (; ostrl; ostrl--, ostr++) {
		if (accept_char(*ostr)) {
			add_chr_to_str(str, strl, *ostr);
		} else {
			add_to_str(str, strl, "&#");
			add_num_to_str(str, strl, (int) *ostr);
			add_chr_to_str(str, strl, ';');
		}
	}
}

int ftp_process_dirlist(struct cache_entry *c_e, int *pos, int *dpos,
			unsigned char *buffer, int buflen, int last, int *tries)
{
	int ret = 0;
	
	while (1) {
		unsigned char *buf = buffer + ret;
		int bufl = buflen - ret;
		int bufp;
		unsigned char *str;
		int strl;
		
		/* Newline quest */
		
		for (bufp = 0; bufp < bufl; bufp++)
			if (buf[bufp] == '\n')
				break;
	
		if (buf[bufp] == '\n') {
			ret += bufp + 1;
			if (bufp && buf[bufp - 1] == '\r') bufp--;
			
		} else {
			if (!bufp || (!last && bufl < FTP_BUF))
				return ret;
			ret += bufp;
		}

		/* Proccess line whose end we've already found */
		
		str = init_str(), strl = 0; /* FIXME: if (!str) ... */
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
			if (buf[0] == 'd') {
				/* TODO: Dirs highlighting */
				add_chr_to_str(&str, &strl, '/');
			}
			add_to_str(&str, &strl, "\">");
			add_conv_str(&str, &strl, buf + *dpos,
				     symlinkpos - *dpos);
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
		setcstate(conn, S_OUT_OF_MEM);
		abort_connection(conn);
		return;
	}
	
	if (c_i->dir && !conn->from) {
		unsigned char *url_data;
		static unsigned char ftp_head[] = "<html><head><title>/";
		static unsigned char ftp_head2[] = "</title></head><body><h2>Directory /";
		static unsigned char ftp_head3[] = "</h2><pre>";
		
#define A(str) { add_fragment(conn->cache, conn->from, str, strlen(str));\
		 conn->from += strlen(str); }
		
		url_data = stracpy(get_url_data(conn->url));
		if (strchr(url_data, POST_CHAR))
			*strchr(url_data, POST_CHAR) = 0;
		
		A(ftp_head);
		if (url_data) A(url_data);
		A(ftp_head2);
		if (url_data) A(url_data);
		A(ftp_head3);
		
		if (url_data)
			mem_free(url_data);
		
		if (!conn->cache->head) conn->cache->head = stracpy("\r\n");
		add_to_strn(&conn->cache->head, "Content-Type: text/html\r\n");
		
#undef A
	}
	
	len = read(conn->sock2, c_i->ftp_buffer + c_i->buf_pos,
		   FTP_BUF - c_i->buf_pos);
	
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
			memmove(c_i->ftp_buffer, c_i->ftp_buffer + proceeded, c_i->buf_pos + len - proceeded);
			c_i->buf_pos += len - proceeded;
		}
		
		setcstate(conn, S_TRANS);
		return;
	}
	
	ftp_process_dirlist(conn->cache, &conn->from, &c_i->dpos,
			    c_i->ftp_buffer, c_i->buf_pos,
			    1, &conn->tries);
	
	set_handlers(conn->sock2, NULL, NULL, NULL, NULL);
	close_socket(&conn->sock2);
	
	if (c_i->conn_state == 1) {
		setcstate(conn, S_OK);
		ftp_end_request(conn);
	} else {
		c_i->conn_state = 2;
		setcstate(conn, S_TRANS);
	}
}

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

