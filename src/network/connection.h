/* $Id: connection.h,v 1.37 2003/07/24 11:35:11 miciah Exp $ */

#ifndef EL__SCHED_CONNECTION_H
#define EL__SCHED_CONNECTION_H

#include "elinks.h" /* SSL stuff */

/* We need to declare these first :/. Damn cross-dependencies. */
struct connection;
struct read_buffer;

#include "document/cache.h"
#include "lowlevel/connect.h"
#include "lowlevel/ttime.h"
#include "protocol/uri.h"
#include "util/encoding.h"
#include "util/error.h"
#include "util/lists.h"

enum connection_priority {
	PRI_MAIN	= 0,
	PRI_DOWNLOAD	= 0,
	PRI_FRAME,
	PRI_NEED_IMG,
	PRI_IMG,
	PRI_PRELOAD,
	PRI_CANCEL,
	PRIORITIES,
};

/* Numbers < 0 and > -10000 are reserved for system errors reported via
 * errno/strerror(), see session.c and connection.c for further information. */
/* WARNING: an errno value <= -10000 may cause some bad things... */
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

	S_HTTP_ERROR		= -10100,
	S_HTTP_100		= -10101,
	S_HTTP_204		= -10102,

	S_FILE_TYPE		= -10200,
	S_FILE_ERROR		= -10201,

	S_FTP_ERROR		= -10300,
	S_FTP_UNAVAIL		= -10301,
	S_FTP_LOGIN		= -10302,
	S_FTP_PORT		= -10303,
	S_FTP_NO_FILE		= -10304,
	S_FTP_FILE_ERROR	= -10305,

	S_SSL_ERROR		= -10400,
	S_NO_SSL		= -10401,
};

struct remaining_info {
	ttime elapsed;
	ttime last_time;
	ttime dis_b;

	unsigned int valid:1;
	int size, loaded, last_loaded, cur_loaded;

	/* This is offset where the download was resumed possibly */
	/* prg->start == -1 means normal session, not download
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

struct connection {
	LIST_HEAD(struct connection);

	struct list_head downloads;
	struct remaining_info prg;

	struct uri uri;
	unsigned char *ref_url;
	void *dnsquery;
	struct conn_info *conn_info;
	void *info;
	void *buffer;
	struct cache_entry *cache;
	/* This is in fact (ssl_t *), but we don't want to know. Noone cares
	 * and ssl.h inclusion costs a lot of compilation time. --pasky */
	void *ssl;
	struct stream_encoded *stream;

	/* Only used by ftp in send_cmd/get_resp. Put here
	 * since having no connection->info is apparently valid. */
	void (*read_func)(struct connection *, struct read_buffer *);

	unsigned int id;

	int pf; /* 1 == PF_INET, 2 == PF_INET6 */
	enum connection_state state;
	int prev_error;
	int from;
	int sock1;
	int sock2;
	int tries;
	int received;
	int est_length;
	int timer;
	int stream_pipes[2];

	unsigned int running:1;
	unsigned int unrestartable:1;
	unsigned int detached:1;
	unsigned int no_tsl:1;

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

	struct connection *c;
	struct cache_entry *ce;
	void (*end)(struct download *, void *);
	void *data;
	struct remaining_info *prg;

	enum connection_state state;
	int prev_error;
	enum connection_priority pri;
};

extern struct list_head queue;

void check_queue(void);
long connect_info(int);
void set_connection_state(struct connection *, int);

int has_keepalive_connection(struct connection *);
void add_keepalive_connection(struct connection *, ttime);

void retry_connection(struct connection *);
void abort_connection(struct connection *);

void abort_conn_with_state(struct connection *, int);
void retry_conn_with_state(struct connection *, int);

void change_connection(struct download *, struct download *, int, int);
void detach_connection(struct download *, int);
void abort_all_connections(void);
void abort_background_connections(void);

void set_connection_timeout(struct connection *);

/* Initiates a connection to get the given @url. */
/* Note that stat's data _MUST_ be struct file_download * if start > 0! Yes,
 * that should be probably something else than data, but... ;-) */
/* Returns 0 on success and -1 on failure. */
int load_url(unsigned char *url, unsigned char *ref_url, struct download *download,
	     enum connection_priority pri, enum cache_mode cache_mode, int start);

#endif
