/* Internal "cgi" protocol implementation */
/* $Id: cgi.c,v 1.5 2003/12/01 10:57:04 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h> /* OS/2 needs this after sys/types.h */
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "config/options.h"
#include "mime/backend/common.h"
#include "osdep/osdep.h"
#include "protocol/file/cgi.h"
#include "protocol/http/http.h"
#include "protocol/uri.h"
#include "util/conv.h"
#include "util/string.h"

static void
close_pipe_and_read(struct connection *conn)
{
	struct read_buffer *rb = alloc_read_buffer(conn);

	if (!rb) {
		abort_conn_with_state(conn, S_OUT_OF_MEM);
		return;
	}
	memcpy(rb->data, "HTTP/1.0 200 OK\r\n", 17);
	rb->len = 17;
	rb->freespace -= 17;
	rb->close = 1;
	conn->unrestartable = 1;
	close(conn->cgi_input[1]);
	conn->cgi_input[1] = -1;
	set_connection_timeout(conn);
	read_from_socket(conn, conn->socket, rb, http_got_header);
}

static void
send_post_data(struct connection *conn)
{
#define POST_BUFFER_SIZE 4096
	unsigned char *post = conn->uri.post;
	unsigned char *postend;
	unsigned char buffer[POST_BUFFER_SIZE];
	struct string data;
	int n = 0;

	if (!init_string(&data)) {
		abort_conn_with_state(conn, S_OUT_OF_MEM);
		return;
	}
	postend = strchr(post, '\n');
	if (postend) post = postend + 1;

/* FIXME: Code duplication with protocol/http/http.c! --witekfl */	
	while (post[0] && post[1]) {
		register int h1, h2;

		h1 = unhx(post[0]);
		assert(h1 >= 0 && h1 < 16);
		if_assert_failed h1 = 0;

		h2 = unhx(post[1]);
		assert(h2 >= 0 && h2 < 16);
		if_assert_failed h2 = 0;

		buffer[n++] = (h1<<4) + h2;
		post += 2;
		if (n == POST_BUFFER_SIZE) {
			add_bytes_to_string(&data, buffer, n);
			n = 0;
		}
	}
	if (n)
		add_bytes_to_string(&data, buffer, n);

	set_connection_timeout(conn);
	write_to_socket(conn, conn->cgi_input[1], data.source, data.length, close_pipe_and_read);
	done_string(&data);
	set_connection_state(conn, S_SENT);
#undef POST_BUFFER_SIZE
}

static void
send_request(struct connection *conn)
{
	if (conn->uri.post) send_post_data(conn);
	else close_pipe_and_read(conn);
}

/* This function sets CGI environment variables */
static int
set_vars(struct connection *conn, unsigned char *script)
{
	unsigned char *post = conn->uri.post;
	
	if (post) {
		unsigned char *postend = strchr(post, '\n');
		unsigned char buf[16];
		
		if (postend) {
			int res;
			
			*postend = '\0';
			res = setenv("CONTENT_TYPE", post, 1);
			*postend = '\n';
			if (res) return -1;
			post = postend + 1;
		}
		snprintf(buf, 16, "%d", strlen(post) / 2);
		if (setenv("CONTENT_LENGTH", buf, 1)) return -1;
		if (setenv("REQUEST_METHOD", "POST", 1)) return -1;
	} else {
		unsigned char *question_mark = strchr(conn->uri.data, '?');

		if (setenv("REQUEST_METHOD", "GET", 1)) return -1;
		if (question_mark) {
			if (setenv("QUERY_STRING", question_mark + 1, 1)) return -1;
		} else {
			if (setenv("QUERY_STRING", "", 1)) return -1;
		}
	}

	return setenv("SCRIPT_NAME", script, 1);
}

static int
test_path(unsigned char *path, int pathlen)
{
	unsigned char *cgi_path = get_opt_str("protocol.file.cgi.path");
	unsigned char **path_ptr;
	unsigned char *filename;

	for (path_ptr = &cgi_path; (filename = get_next_path_filename(path_ptr, ':'));) {
		int res = strncmp(path, filename, pathlen);

		mem_free(filename);
		if (!res) return 0;
	}
	return 1;
}

int
execute_cgi(struct connection *conn)
{
	unsigned char *last_slash;
	unsigned char *question_mark;
	unsigned char *post_char;
	unsigned char *script;
	int scriptlen = conn->uri.datalen;
	struct stat buf;
	int res;
	enum connection_state state = S_OK;

	if (!get_opt_bool("protocol.file.cgi.policy")) return 1;

	/* Not file referrer */
	if (conn->ref_url && strncmp(conn->ref_url, "file:", 5)) {
		return 1;
	}

	question_mark = memchr(conn->uri.data, '?', scriptlen);
	if (question_mark) scriptlen = question_mark - conn->uri.data;

	script = memacpy(conn->uri.data, scriptlen);
	if (!script) {
		state = S_OUT_OF_MEM;
		goto end2;
	}

	if (stat(script, &buf) || !(S_ISREG(buf.st_mode))
		|| !(buf.st_mode & S_IEXEC)) {
		mem_free(script);
		return 1;
	}

	last_slash = strrchr(script, '/');
	if (last_slash) {
		int res;

		res = test_path(script, last_slash - script);
		if (res) {
		/* If script is not in cgi_path and hasn't got extension .cgi: */
			if (scriptlen < 4
			    || strcasecmp(script + scriptlen - 4, ".cgi")) {
				state = S_FILE_CGI_BAD_PATH;
				goto end1;
			}
		}
	} else {
		state = S_FILE_CGI_BAD_PATH;
		goto end1;
	}

	if (c_pipe(conn->cgi_input) || c_pipe(conn->cgi_output)) {
		mem_free(script);
		retry_conn_with_state(conn, -errno);
		return 0;
	}

	res = fork();
	if (res < 0) {
		state = -errno;
		goto end1;
	}
	if (!res) { /* CGI script */
		int i;

		if (set_vars(conn, script)) {
			_exit(1);
		}
		if ((dup2(conn->cgi_input[0], STDIN_FILENO) < 0)
			|| (dup2(conn->cgi_output[1], STDOUT_FILENO) < 0)) {
			_exit(2);
		}
		for (i = 2; i < 1024; i++) {
			close(i);
		}
		if (execl(script, script, NULL)) {
			_exit(3);
		}
	} else { /* elinks */
		struct http_connection_info *info;
		
		info = mem_calloc(1, sizeof(struct http_connection_info));
		if (!info) {
			state = S_OUT_OF_MEM;
			goto end1;
		}
		mem_free(script);
		conn->info = info;
		info->sent_version.major = 1;
		info->sent_version.minor = 0;
		info->close = 1;
		conn->socket = conn->cgi_output[0];
		send_request(conn);
		return 0;
	}
end1:
	mem_free(script);
end2:
	abort_conn_with_state(conn, state);
	return 0;
}
