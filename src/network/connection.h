/* $Id: connection.h,v 1.84 2005/02/02 17:33:15 jonas Exp $ */

#ifndef EL__SCHED_CONNECTION_H
#define EL__SCHED_CONNECTION_H

#include "cache/cache.h"
#include "encoding/encoding.h"
#include "util/lists.h"
#include "util/ttime.h"

struct conn_info;
struct read_buffer;
struct uri;


enum connection_priority {
	PRI_MAIN	= 0,
	PRI_DOWNLOAD	= 0,
	PRI_FRAME,
	PRI_CSS,
	PRI_NEED_IMG,
	PRI_IMG,
	PRI_PRELOAD,
	PRI_CANCEL,
	PRIORITIES,
};

/* Numbers < 0 and > -10000 are reserved for system errors reported via
 * errno/strerror(), see session.c and connection.c for further information. */
/* WARNING: an errno value <= -10000 may cause some bad things... */

#define is_system_error(state)		(S_OK < (state) && (state) < S_WAIT)
#define is_in_result_state(state)	((state) < 0)
#define is_in_progress_state(state)	((state) >= 0)
#define is_in_connecting_state(state)	(S_WAIT < (state) && (state) < S_TRANS)
#define is_in_transfering_state(state)	((state) >= S_TRANS)
#define is_in_queued_state(state)	(is_in_connecting_state(state) || (state) == S_WAIT)

/* FIXME: Namespace clash with Windows headers. */
#undef S_OK

enum connection_state {
	/* States >= 0 are used for connections still in progress. */
	S_WAIT			= 0,
	S_DNS,
	S_CONN,
	S_SSL_NEG,
	S_SENT,
	S_LOGIN,
	S_GETH,
	S_PROC,
	S_TRANS,
	S_QUESTIONS,

	/* State < 0 are used for the final result of a connection
	 * (it's finished already and it ended up like this) */
	S_OK			= -10000,
	S_INTERRUPTED		= -10001,
	S_EXCEPT		= -10002,
	S_INTERNAL		= -10003,
	S_OUT_OF_MEM		= -10004,
	S_NO_DNS		= -10005,
	S_CANT_WRITE		= -10006,
	S_CANT_READ		= -10007,
	S_MODIFIED		= -10008,
	S_BAD_URL		= -10009,
	S_TIMEOUT		= -10010,
	S_RESTART		= -10011,
	S_STATE			= -10012,
	S_WAIT_REDIR		= -10013,
	S_LOCAL_ONLY		= -10014,
	S_UNKNOWN_PROTOCOL	= -10015,
	S_EXTERNAL_PROTOCOL	= -10016,
	S_ENCODE_ERROR		= -10017,
	S_SSL_ERROR		= -10018,

	S_HTTP_ERROR		= -10100,
	S_HTTP_100		= -10101,
	S_HTTP_204		= -10102,

	S_FILE_TYPE		= -10200,
	S_FILE_ERROR		= -10201,
	S_FILE_CGI_BAD_PATH	= -10202,

	S_FTP_ERROR		= -10300,
	S_FTP_UNAVAIL		= -10301,
	S_FTP_LOGIN		= -10302,
	S_FTP_PORT		= -10303,
	S_FTP_NO_FILE		= -10304,
	S_FTP_FILE_ERROR	= -10305,

	S_NNTP_ERROR		= -10400,
	S_NNTP_NEWS_SERVER	= -10401,

	S_GOPHER_CSO_ERROR	= -10500,

	S_NO_JAVASCRIPT		= -10600,

	S_PROXY_ERROR		= -10700,
};

struct progress {
	ttime elapsed;
	ttime last_time;
	ttime dis_b;

	unsigned int valid:1;
	int size, loaded, last_loaded, cur_loaded;

	/* This is offset where the download was resumed possibly */
	/* progress->start == -1 means normal session, not download
	 *            ==  0 means download
	 *             >  0 means resume
	 * --witekfl */
	int start;
	/* This is absolute position in the stream
	 * (relative_position = pos - start) (maybe our fictional
	 * relative_position is equiv to loaded, but I'd rather not rely on it
	 * --pasky). */
	int pos;
	/* If this is non-zero, it indicates that we should seek in the
	 * stream to the value inside before the next write (and zero this
	 * counter then, obviously). */
	int seek;

	int timer;
	int data_in_secs[CURRENT_SPD_SEC];
};

struct connection_socket {
	/* The socket descriptor */
	int fd;

	/* For connections using SSL this is in fact (ssl_t *), but we don't
	 * want to know. Noone cares and ssl.h inclusion costs a lot of
	 * compilation time. --pasky */
	void *ssl;
};

struct connection {
	LIST_HEAD(struct connection);

	struct list_head downloads;
	struct progress progress;

	struct uri *uri;
	struct uri *proxied_uri;
	struct uri *referrer;

	void *dnsquery;
	struct conn_info *conn_info;
	void *info;
	void *buffer;
	struct cache_entry *cached;
	struct stream_encoded *stream;

	/* Only used by ftp in send_cmd/get_resp. Put here
	 * since having no connection->info is apparently valid. */
	void (*read_func)(struct connection *, struct read_buffer *);

	unsigned int id;

	enum connection_state state;
	int prev_error;
	int from;

	/* The communication socket with the other side. */
	struct connection_socket socket;
	/* The data socket. It is used, when @socket is used for the control,
	 * and the actual data is transmitted through a different channel. */
	/* The only users now is FTP and SMB. */
	struct connection_socket data_socket;

	int tries;
	int received;
	int est_length;
	int timer;
	int cgi_pipes[2];
	int stream_pipes[2];

	unsigned int protocol_family:1; /* 0 == PF_INET, 1 == PF_INET6 */
	unsigned int running:1;
	unsigned int unrestartable:1;
	unsigned int detached:1;
	unsigned int no_tls:1;

	/* Each document is downloaded with some priority. When downloading a
	 * document, the existing connections are checked to see if a
	 * connection to the host already exists before creating a new one.  If
	 * it finds out that something had that idea earlier and connection for
	 * download of the very same URL is active already, it just attaches
	 * the struct download it got to the connection, _and_ updates its @pri
	 * array by the priority it has thus, sum of values in all fields of
	 * @pri is also kinda refcount of the connection. */
	int pri[PRIORITIES];

	enum cache_mode cache_mode;
	enum stream_encoding content_encoding;
};


struct download {
	/* XXX: order matters there, there's some hard initialization in
	 * src/sched/session.c and src/viewer/text/view.c */
	LIST_HEAD(struct download);

	struct connection *conn;
	struct cache_entry *cached;
	/* The callback is called when connection gets into a progress state,
	 * after it's over (in a result state), and also periodically after
	 * the download starts receiving some data. */
	void (*callback)(struct download *, void *);
	void *data;
	struct progress *progress;

	enum connection_state state;
	int prev_error;
	enum connection_priority pri;
};

extern struct list_head queue;

void check_queue(void);
long connect_info(int);
void set_connection_state(struct connection *, int);

int has_keepalive_connection(struct connection *);
void add_keepalive_connection(struct connection *, ttime,
			      void (*done)(struct connection *));

void retry_connection(struct connection *);
void abort_connection(struct connection *);

void abort_conn_with_state(struct connection *, int);
void retry_conn_with_state(struct connection *, int);

void change_connection(struct download *, struct download *, int, int);
void detach_connection(struct download *, int);
void abort_all_connections(void);
void abort_background_connections(void);

void set_connection_timeout(struct connection *);

/* Initiates a connection to get the given @uri. */
/* Note that stat's data _MUST_ be struct file_download * if start > 0! Yes,
 * that should be probably something else than data, but... ;-) */
/* Returns 0 on success and -1 on failure. */
int load_uri(struct uri *uri, struct uri *referrer, struct download *download,
	     enum connection_priority pri, enum cache_mode cache_mode, int start);

#endif
