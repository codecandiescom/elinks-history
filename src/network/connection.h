/* $Id: connection.h,v 1.22 2003/07/04 00:25:36 jonas Exp $ */

#ifndef EL__SCHED_CONNECTION_H
#define EL__SCHED_CONNECTION_H

#include "elinks.h" /* SSL stuff */

#include "document/cache.h"
#include "lowlevel/ttime.h"
#include "ssl/ssl.h"
#include "util/encoding.h"
#include "util/error.h"
#include "util/lists.h"
#include "util/types.h"

enum connection_priority {
	PRI_MAIN	= 0,
	PRI_DOWNLOAD	= 0,
	PRI_FRAME	= 1,
	PRI_NEED_IMG	= 2,
	PRI_IMG		= 3,
	PRI_PRELOAD	= 4,
	PRI_CANCEL	= 5,
	PRIORITIES	= 6,
};

/* Numbers < 0 and > -10000 are reserved for system errors reported via
 * errno/strerror(), see session.c and connection.c for further information. */
/* WARNING: an errno value <= -10000 may cause some bad things... */
enum connection_state {
	/* States >= 0 are used for connections still in progress. */
	S_WAIT			= 0,
	S_DNS			= 1,
	S_CONN			= 2,
	S_SSL_NEG		= 3,
	S_SENT			= 4,
	S_LOGIN			= 5,
	S_GETH			= 6,
	S_PROC			= 7,
	S_TRANS			= 8,
	S_QUESTIONS		= 9,

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

	struct list_head statuss;
	struct remaining_info prg;

	unsigned char *url;
	unsigned char *ref_url;
	void *dnsquery;
	void *conn_info;
	void *info;
	void *buffer;
	void *read_func;
	void (*conn_func)(void *);
	struct cache_entry *cache;
	ssl_t *ssl;
	struct stream_encoded *stream;

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
	int unrestartable;
	int timer;
	int no_tsl;
	int stream_pipes[2];

	unsigned int running:1;
	unsigned int detached:1;

	/* Each document is downloaded with some priority. When downloading a
	 * document, the existing connections are checked to see if a
	 * connection to the host already exists before creating a new one.  If
	 * it finds out that something had that idea earlier and connection for
	 * download of the very same URL is active already, it just attaches
	 * the struct status it got to the connection, _and_ updates its @pri
	 * array by the priority it has thus, sum of values in all fields of
	 * @pri is also kinda refcount of the connection. */
	int pri[PRIORITIES];

	enum cache_mode cache_mode;
	enum stream_encoding content_encoding;
};


struct status {
	/* XXX: order matters there, there's some hard initialization in
	 * src/sched/session.c and src/viewer/text/view.c */
	LIST_HEAD(struct status);

	struct connection *c;
	struct cache_entry *ce;
	void (*end)(struct status *, void *);
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

int get_keepalive_socket(struct connection *);
void add_keepalive_socket(struct connection *, ttime);

void retry_connection(struct connection *);
void abort_connection(struct connection *);

void abort_conn_with_state(struct connection *, int);
void retry_conn_with_state(struct connection *, int);

void change_connection(struct status *, struct status *, int, int);
void detach_connection(struct status *, int);
void abort_all_connections(void);
void abort_background_connections(void);

void set_timeout(struct connection *);

/* Initiates a connection to get the given @url. */
/* Note that stat's data _MUST_ be struct download * if start > 0! Yes, that
 * should be probably something else than data, but... ;-) */
/* Returns 0 on success and -1 on failure. */
int load_url(unsigned char *url, unsigned char *ref_url, struct status *stat,
	     enum connection_priority pri, enum cache_mode cache_mode, int start);

#endif
