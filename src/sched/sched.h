/* $Id: sched.h,v 1.11 2003/07/02 10:23:31 jonas Exp $ */

#ifndef EL__SCHED_SCHED_H
#define EL__SCHED_SCHED_H

#include "elinks.h" /* SSL stuff */

#include "document/cache.h"
#include "lowlevel/ttime.h"
#include "ssl/ssl.h"
#include "util/encoding.h"
#include "util/error.h"
#include "util/lists.h"
#include "util/types.h"

#define PRI_MAIN	0
#define PRI_DOWNLOAD	0
#define PRI_FRAME	1
#define PRI_NEED_IMG	2
#define PRI_IMG		3
#define PRI_PRELOAD	4
#define PRI_CANCEL	5
#define N_PRI		6

struct remaining_info {
	ttime elapsed;
	ttime last_time;
	ttime dis_b;

	int valid;
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

	tcount count;

	int pf; /* 1 == PF_INET, 2 == PF_INET6 */
	int running;
	int state;
	int prev_error;
	int from;
	int sock1;
	int sock2;
	int tries;
	int received;
	int est_length;
	int unrestartable;
	int timer;
	int detached;
	int no_tsl;
	int stream_pipes[2];

	/* Each document is downloaded with some priority. When downloading a
	 * document, the existing connections are checked to see if a
	 * connection to the host already exists before creating a new one.  If
	 * it finds out that something had that idea earlier and connection for
	 * download of the very same URL is active already, it just attaches
	 * the struct status it got to the connection, _and_ updates its @pri
	 * array by the priority it has thus, sum of values in all fields of
	 * @pri is also kinda refcount of the connection. */
	int pri[N_PRI];

	enum cache_mode cache_mode;
	enum stream_encoding content_encoding;
};

/* Connection states */
#define S_WAIT		0
#define S_DNS		1
#define S_CONN		2
#define S_SSL_NEG	3
#define S_SENT		4
#define S_LOGIN		5
#define S_GETH		6
#define S_PROC		7
#define S_TRANS		8
#define S_QUESTIONS	9

/* Numbers < 0 and > -10000 are reserved for system errors reported via
 * errno/strerror(), see session.c and sched.c for further information. */

/* WARNING: an errno value <= -10000 may cause some bad things... */

#define S_OK			-10000
#define S_INTERRUPTED		-10001
#define S_EXCEPT		-10002
#define S_INTERNAL		-10003
#define S_OUT_OF_MEM		-10004
#define S_NO_DNS		-10005
#define S_CANT_WRITE		-10006
#define S_CANT_READ		-10007
#define S_MODIFIED		-10008
#define S_BAD_URL		-10009
#define S_TIMEOUT		-10010
#define S_RESTART		-10011
#define S_STATE			-10012
#define S_WAIT_REDIR		-10013

#define S_HTTP_ERROR		-10100
#define S_HTTP_100		-10101
#define S_HTTP_204		-10102

#define S_FILE_TYPE		-10200
#define S_FILE_ERROR		-10201

#define S_FTP_ERROR		-10300
#define S_FTP_UNAVAIL		-10301
#define S_FTP_LOGIN		-10302
#define S_FTP_PORT		-10303
#define S_FTP_NO_FILE		-10304
#define S_FTP_FILE_ERROR	-10305

#define S_SSL_ERROR		-10400
#define S_NO_SSL		-10401


struct status {
	/* XXX: order matters there, there's some hard initialization in
	 * src/sched/session.c and src/viewer/text/view.c */
	LIST_HEAD(struct status);

	struct connection *c;
	struct cache_entry *ce;
	void (*end)(struct status *, void *);
	void *data;
	struct remaining_info *prg;

	int state;
	int prev_error;
	int pri;
};

extern struct list_head queue;

void check_queue(void);
long connect_info(int);
/* void send_connection_info(struct connection *c); */
void setcstate(struct connection *, int);

int get_keepalive_socket(struct connection *);
void add_keepalive_socket(struct connection *, ttime);

/* void run_connection(struct connection *); */
void retry_connection(struct connection *);
void abort_connection(struct connection *);
/* void end_connection(struct connection *); */

void abort_conn_with_state(struct connection *, int);
void retry_conn_with_state(struct connection *, int);

int load_url(unsigned char *, unsigned char *, struct status *, int, enum cache_mode, int start);

void change_connection(struct status *, struct status *, int, int);
void detach_connection(struct status *, int);
void abort_all_connections(void);
void abort_background_connections(void);

/* void connection_timeout(struct connection *); */
void set_timeout(struct connection *);

#endif
