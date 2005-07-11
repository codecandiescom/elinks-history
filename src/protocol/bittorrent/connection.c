/* Internal "bittorrent" protocol implementation */
/* $Id: connection.c,v 1.1 2005/07/11 10:59:04 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "cache/cache.h"
#include "config/options.h"
#include "main/timer.h"
#include "network/connection.h"
#include "network/progress.h"
#include "network/socket.h"
#include "protocol/bittorrent/bencoding.h"
#include "protocol/bittorrent/bittorrent.h"
#include "protocol/bittorrent/common.h"
#include "protocol/bittorrent/connection.h"
#include "protocol/bittorrent/tracker.h"
#include "protocol/bittorrent/peerconnect.h"
#include "protocol/bittorrent/peerwire.h"
#include "protocol/bittorrent/piececache.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "session/download.h"
#include "util/bitfield.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"


/* ************************************************************************** */
/* Peer selection and connection scheduling: */
/* ************************************************************************** */

/* Reschedule updating of the connection state. */
static void
set_bittorrent_connection_timer(struct connection *conn)
{
	struct bittorrent_connection *bittorrent = conn->info;
	int interval = get_opt_int("protocol.bittorrent.choke_interval");

	/* Times 1000 to get the interval in milliseconds. */
	install_timer(&bittorrent->timer, interval * 1000,
		      (void (*)(void *)) update_bittorrent_connection_state,
		      conn);
}

static int
compare_bittorrent_download_rate(const void *p1, const void *p2)
{
	const struct bittorrent_peer_connection *peer1 = p1;
	const struct bittorrent_peer_connection *peer2 = p2;

	if (peer1->stats.download_rate < peer2->stats.download_rate)
		return -1;

	if (peer1->stats.download_rate > peer2->stats.download_rate)
		return 1;

	return 0;
}

static int
compare_bittorrent_have_rate(const void *p1, const void *p2)
{
	const struct bittorrent_peer_connection *peer1 = p1;
	const struct bittorrent_peer_connection *peer2 = p2;

	if (peer1->stats.have_rate < peer2->stats.have_rate)
		return -1;

	if (peer1->stats.have_rate > peer2->stats.have_rate)
		return 1;

	return 0;
}

static int
compare_bittorrent_bitfield_rate(const void *p1, const void *p2)
{
	const struct bittorrent_peer_connection *peer1 = p1;
	const struct bittorrent_peer_connection *peer2 = p2;

	if (peer1->stats.bitfield_rate < peer2->stats.bitfield_rate)
		return 1;

	if (peer1->stats.bitfield_rate > peer2->stats.bitfield_rate)
		return -1;

	return 0;
}

static int
compare_bittorrent_loyalty_rate(const void *p1, const void *p2)
{
	const struct bittorrent_peer_connection *peer1 = p1;
	const struct bittorrent_peer_connection *peer2 = p2;

	if (peer1->stats.loyalty_rate < peer2->stats.loyalty_rate)
		return -1;

	if (peer1->stats.loyalty_rate > peer2->stats.loyalty_rate)
		return 1;

	return 0;
}

static void
rank_bittorrent_peer_connections(struct bittorrent_connection *bittorrent)
{
	struct bittorrent_peer_connection *peer, **peers;
	size_t size = list_size(&bittorrent->peers);
	double behaviour_scales[][4] = {
		/* Default: */		{ 1.0, 0.0,  0.0,  0.0 },
		/* Free rider: */	{ 0.7, 0.1,  0.2,  0.0 },
		/* Merchant: */		{ 0.6, 0.15, 0.15, 0.1 },
		/* Altruistic: */	{ 0.4, 0.15, 0.25, 0.2 },
	};
	enum bittorrent_behaviour behaviour;
	double prev_rank = 2.0;
	int index;

	peers = mem_alloc(size * sizeof(*peers));
	if (!peers) return;

	index = 0;
	foreach (peer, bittorrent->peers) {
		peers[index++] = peer;
		if (!peer->remote.handshake)
			continue;

		update_bittorrent_peer_connection_rates(peer);
	}

	qsort(peers, size, sizeof(*peers), compare_bittorrent_download_rate);

	for (index = 0; index < size; index++) {
		peers[index]->stats.download_rank = (double) (size - index) / size;
		assert(prev_rank > peers[index]->stats.download_rank);
		prev_rank = peers[index]->stats.download_rank;
	}

	qsort(peers, size, sizeof(*peers), compare_bittorrent_have_rate);

	prev_rank = 2.0;
	for (index = 0; index < size; index++) {
		peers[index]->stats.have_rank = (double) (size - index) / size;
		assert(prev_rank > peers[index]->stats.have_rank);
		prev_rank = peers[index]->stats.have_rank;
	}

	qsort(peers, size, sizeof(*peers), compare_bittorrent_bitfield_rate);

	prev_rank = 2.0;
	for (index = 0; index < size; index++) {
		peers[index]->stats.bitfield_rank = (double) (size - index) / size;
		assert(prev_rank > peers[index]->stats.bitfield_rank);
		prev_rank = peers[index]->stats.bitfield_rank;
	}

	qsort(peers, size, sizeof(*peers), compare_bittorrent_loyalty_rate);

	prev_rank = 2.0;
	for (index = 0; index < size; index++) {
		peers[index]->stats.loyalty_rank = (double) (size - index) / size;
		assert(prev_rank > peers[index]->stats.loyalty_rank);
		assert(peers[index]->stats.loyalty_rank <= 1.0);
		assert(peers[index]->stats.loyalty_rank >= 0.0);
		prev_rank = peers[index]->stats.loyalty_rank;
	}

	behaviour = get_opt_int("protocol.bittorrent.peer_selection");

	foreach (peer, bittorrent->peers) {
		struct bittorrent_peer_stats *stats = &peer->stats;

		stats->rank  = 0.0;
		stats->rank += behaviour_scales[behaviour][0] * stats->download_rank;
		stats->rank += behaviour_scales[behaviour][1] * stats->have_rank;
		stats->rank += behaviour_scales[behaviour][2] * stats->bitfield_rank;
		stats->rank += behaviour_scales[behaviour][3] * stats->loyalty_rank;
	}

	mem_free(peers);
}

static int
compare_bittorrent_opt_unchoke_rating(const void *p1, const void *p2)
{
	const struct bittorrent_peer_connection *peer1 = p1;
	const struct bittorrent_peer_connection *peer2 = p2;

	if (peer1->stats.rank < peer2->stats.rank)
		return -1;

	if (peer1->stats.rank > peer2->stats.rank)
		return 1;

	return 0;
}

static void
opt_unchoke_bittorrent_peer_connections(struct bittorrent_connection *bittorrent)
{
	struct bittorrent_peer_connection *peer, **peers;
	enum bittorrent_behaviour behaviour = get_opt_int("protocol.bittorrent.peer_selection");
	double prev_rank = 2.0;
	int index, min_uploads;
	size_t size = list_size(&bittorrent->peers);
	int unflagged_peers;

	peers = mem_alloc(size * sizeof(*peers));
	if (!peers) return;

	size = 0;
	foreach (peer, bittorrent->peers) {
		struct bittorrent_peer_stats *stats = &peer->stats;

		if (!peer->remote.handshake || !peer->local.choked) {
			peer->local.opt_unchoke = 1;
			continue;
		}

		peers[size++] = peer;

		stats->rank  = stats->have_rank
			     + stats->bitfield_rank
			     + stats->loyalty_rank;
	}

	qsort(peers, size, sizeof(*peers), compare_bittorrent_opt_unchoke_rating);

	for (index = 0, unflagged_peers = 0; index < size; index++) {
		peers[index]->stats.rank = (double) (size - index) / size;
		assert(prev_rank > peers[index]->stats.rank);
		prev_rank = peers[index]->stats.rank;
		if (!peer->local.opt_unchoke)
			unflagged_peers++;
	}

	min_uploads = get_opt_int("protocol.bittorrent.min_uploads");
	if (min_uploads > size) min_uploads = size;

	while (min_uploads > 0) {
		enum { BITTORRENT_MAX, BITTORRENT_MIN } select;

		switch (behaviour) {
		case BITTORRENT_BEHAVIOUR_FREERIDER:
			/* Always prefer good peers */
			select = BITTORRENT_MAX;
			break;

		case BITTORRENT_BEHAVIOUR_MERCHANT:
			/* Alternate */
			select = (index % 2) ? BITTORRENT_MIN : BITTORRENT_MAX;
			break;

		case BITTORRENT_BEHAVIOUR_ALTRUISTIC:
		case BITTORRENT_BEHAVIOUR_DEFAULT:
		default:
			/* Always prefer new peers */
			select = BITTORRENT_MIN;
		};

		/* Handle round robin */

		if (!unflagged_peers) {
			for (index = 0; index < size; index++) {
				if (!peer->remote.choked)
					continue;
				peers[index]->local.opt_unchoke = 0;
				unflagged_peers++;
			}

			if (!unflagged_peers)
				break;
		}

		peer = NULL;

		if (select == BITTORRENT_MAX) {
			for (index = 0; index < size; index++) {
				if (peers[index]->local.opt_unchoke)
					continue;

				peer = peers[index];
			}
		} else {
			for (index = size - 1; index >= 0; index--) {
				if (peers[index]->local.opt_unchoke)
					continue;

				peer = peers[index];
			}
		}

		if (!peer) break;

		if (peer->remote.choked)
			unchoke_bittorrent_peer(peer);
		peer->remote.choked = 0;
	}

	mem_free(peers);
}

/* Sort the peers based on the stats rate, bubbaly style! */
static void
sort_bittorrent_peer_connections(struct bittorrent_connection *bittorrent)
{
	struct bittorrent_peer_connection *peer, *prev;
	int resort = 0;

	do {
		struct bittorrent_peer_connection *next;

		prev = NULL;
		resort = 0;

		foreachsafe (peer, next, bittorrent->peers) {
			if (prev && prev->stats.rank < peer->stats.rank) {
				resort = 1;
				del_from_list(prev);
				add_at_pos(peer, prev);
			}

			prev = peer;
		}

	} while (resort);

#ifdef CONFIG_DEBUG
	prev = NULL;
	foreach (peer, bittorrent->peers) {
		assertm(!prev || prev->stats.rank >= peer->stats.rank,
			"%10.3f %10.3f", prev->stats.rank, peer->stats.rank);
		prev = peer;
	}
#endif
}

/* This is basically the choke period handler. */
void
update_bittorrent_connection_state(struct connection *conn)
{
	struct bittorrent_connection *bittorrent = conn->info;
	struct bittorrent_peer_connection *peer, *next_peer;
	int peer_conns, max_peer_conns;
	int min_uploads = get_opt_int("protocol.bittorrent.min_uploads");
	int max_uploads = get_opt_int("protocol.bittorrent.max_uploads");

	set_bittorrent_connection_timer(conn);
	set_connection_timeout(conn);

	bittorrent->min_sharing_rate = get_bittorrent_sharing_rate();

	peer_conns = list_size(&bittorrent->peers);
	max_peer_conns = get_opt_int("protocol.bittorrent.peerwire.connections");

	update_bittorrent_piece_cache_state(bittorrent);

	rank_bittorrent_peer_connections(bittorrent);

	/* Sort the peers so that the best peers are at the list start. */
	sort_bittorrent_peer_connections(bittorrent);

	/* Unchoke all the optimal peers. In good spirit, also unchoke all
	 * uninterested peers until the maximum number of interested peers have
	 * been unchoked. The rest is choked. */
	foreachsafe (peer, next_peer, bittorrent->peers) {
		if (!peer->remote.handshake)
			continue;

		if (min_uploads < max_uploads) {
			if (peer->remote.choked)
				unchoke_bittorrent_peer(peer);

			peer->remote.choked = 0;

			/* Uninterested peers are not counted as uploads. */
			if (peer->remote.interested)
				max_uploads--;

		} else {
			if (!peer->remote.choked)
				choke_bittorrent_peer(peer);

			peer->remote.choked = 1;
		}

		/* Can remove the peer so we use foreachsafe(). */
		update_bittorrent_peer_connection_state(peer);
	}

#if 0
	/* Find peer(s) to optimistically unchoke. */
	opt_unchoke_bittorrent_peer_connections(bittorrent);
#endif
	/* Close or open peers connections. */
	if (peer_conns > max_peer_conns) {
		struct bittorrent_peer_connection *prev;

		foreachsafe (peer, prev, bittorrent->peers) {
			done_bittorrent_peer_connection(peer);
			if (--peer_conns <= max_peer_conns)
				break;
		}

	} else if (peer_conns < max_peer_conns) {
		struct bittorrent_peer *peer_info, *next_peer_info;

		foreachsafe (peer_info, next_peer_info, bittorrent->peer_pool) {
			enum bittorrent_state state;

			state = make_bittorrent_peer_connection(bittorrent, peer_info);
			if (state != BITTORRENT_STATE_OK)
				break;

			del_from_list(peer_info);
			mem_free(peer_info);
			if (++peer_conns >= max_peer_conns)
				break;
		}
	}

	assert(peer_conns <= max_peer_conns);

	/* Shrink the peer pool. */
	if (!list_empty(bittorrent->peers)) {
		struct bittorrent_peer *peer_info, *next_peer_info;
		int pool_size = get_opt_int("protocol.bittorrent.peerwire.pool_size");
		int pool_peers = 0;

		foreachsafe (peer_info, next_peer_info, bittorrent->peer_pool) {
			/* Unlimited. */
			if (!pool_size) break;

			if (pool_peers < pool_size) {
				pool_peers++;
				continue;
			}

			del_from_list(peer_info);
			mem_free(peer_info);
		}
	}
}

static void
update_bittorrent_connection_upload(void *data)
{
	struct bittorrent_connection *bittorrent = data;

	update_progress(&bittorrent->upload_progress,
			bittorrent->uploaded,
			bittorrent->downloaded,
			bittorrent->uploaded);

}

void
update_bittorrent_connection_stats(struct bittorrent_connection *bittorrent,
				   off_t downloaded, off_t uploaded,
				   off_t received)
{
	struct bittorrent_meta *meta = &bittorrent->meta;

	if (bittorrent->conn->est_length == -1) {
		off_t length = (off_t) (meta->pieces - 1) * meta->piece_length
			     + meta->last_piece_length;

		bittorrent->conn->est_length = length;
		bittorrent->left = length;
		start_update_progress(&bittorrent->upload_progress,
				      update_bittorrent_connection_upload,
				      bittorrent);
	}

	if (bittorrent->upload_progress.timer == TIMER_ID_UNDEF
	    && bittorrent->uploaded)
		update_bittorrent_connection_upload(bittorrent);

	bittorrent->conn->received += received;
	bittorrent->conn->from	   += downloaded;
	if (downloaded > 0)
		bittorrent->downloaded	   += downloaded;
	bittorrent->uploaded	   += uploaded;
	bittorrent->left	   -= downloaded;

	if (!bittorrent->downloaded) return;

	bittorrent->sharing_rate    = (double) bittorrent->uploaded
					     / bittorrent->downloaded;
}


/* ************************************************************************** */
/* The ``main'' BitTorrent connection setup: */
/* ************************************************************************** */

/* Callback which is attached to the ELinks connection and invoked when the
 * connection is shutdown. */
static void
done_bittorrent_connection(struct connection *conn)
{
	struct bittorrent_connection *bittorrent = conn->info;
	struct bittorrent_peer_connection *peer, *next;

	assert(bittorrent);

	/* We don't want the tracker to see the fetch. */
	if (bittorrent->fetch)
		done_bittorrent_fetch(&bittorrent->fetch);

	foreachsafe (peer, next, bittorrent->peers)
		done_bittorrent_peer_connection(peer);

	done_bittorrent_tracker_connection(conn);
	done_bittorrent_listening_socket(conn);
	if (bittorrent->cache)
		done_bittorrent_piece_cache(bittorrent);
	done_bittorrent_meta(&bittorrent->meta);

	kill_timer(&bittorrent->timer);
	kill_timer(&bittorrent->upload_progress.timer);

	free_list(bittorrent->peer_pool);

	mem_free_set(&conn->info, NULL);
}

static struct bittorrent_connection *
init_bittorrent_connection(struct connection *conn)
{
	struct bittorrent_connection *bittorrent;

	bittorrent = mem_calloc(1, sizeof(*bittorrent));
	if (!bittorrent) return NULL;

	init_list(bittorrent->peers);
	init_list(bittorrent->peer_pool);

	conn->info = bittorrent;
	conn->done = done_bittorrent_connection;

	init_bittorrent_peer_id(bittorrent->peer_id);

	bittorrent->conn = conn;
	bittorrent->tracker.timer = TIMER_ID_UNDEF;

	return bittorrent;
}

void
bittorrent_resume_callback(struct bittorrent_connection *bittorrent)
{
	enum connection_state state;

	/* Failing to create the listening socket is fatal. */
	state = init_bittorrent_listening_socket(bittorrent->conn);
	if (state != S_OK) {
		retry_connection(bittorrent->conn, state);
		return;
	}

	set_connection_state(bittorrent->conn, S_CONN_TRACKER);
	send_bittorrent_tracker_request(bittorrent->conn);
}

/* Metainfo file download callback */
static void
bittorrent_metainfo_callback(void *data, enum connection_state state,
			     struct string *response)
{
	struct connection *conn = data;
	struct bittorrent_connection *bittorrent = conn->info;

	bittorrent->fetch = NULL;

	if (state != S_OK) {
		abort_connection(conn, state);
		return;
	}

	switch (parse_bittorrent_metafile(&bittorrent->meta, response)) {
	case BITTORRENT_STATE_OK:
	{
		size_t size = list_size(&bittorrent->meta.files);
		int *selection;

		assert(bittorrent->tracker.event == BITTORRENT_EVENT_STARTED);

		selection = get_bittorrent_selection(conn->uri, size);
		if (selection) {
			struct bittorrent_file *file;
			int index = 0;

			foreach (file, bittorrent->meta.files)
				file->selected = selection[index++];

			mem_free(selection);
		}

		switch (init_bittorrent_piece_cache(bittorrent, response)) {
		case BITTORRENT_STATE_OK:
			bittorrent_resume_callback(bittorrent);
			return;

		case BITTORRENT_STATE_CACHE_RESUME:
			set_connection_state(bittorrent->conn, S_RESUME);
			return;

		case BITTORRENT_STATE_OUT_OF_MEM:
			state = S_OUT_OF_MEM;
			break;

		default:
			state = S_BITTORRENT_ERROR;
		}

		break;
	}
	case BITTORRENT_STATE_OUT_OF_MEM:
		state = S_OUT_OF_MEM;
		break;

	case BITTORRENT_STATE_ERROR:
	default:
		/* XXX: This can also happen when passing bittorrent:<uri> and
		 * <uri> gives an HTTP 404 response. It might be worth fixing by
		 * looking at the protocol header, however, direct usage of the
		 * internal bittorrent: is at your own risk ... at least for
		 * now. --jonas */
		state = S_BITTORRENT_METAINFO;
	}

	abort_connection(conn, state);
}

/* The entry point for BitTorrent downloads. */
void
bittorrent_protocol_handler(struct connection *conn)
{
	struct uri *uri;
	struct bittorrent_connection *bittorrent;

	bittorrent = init_bittorrent_connection(conn);
	if (!bittorrent) {
		abort_connection(conn, S_OUT_OF_MEM);
		return;
	}

	assert(conn->uri->datalen);

	uri = get_uri(conn->uri->data, 0);
	if (!uri) {
		abort_connection(conn, S_OUT_OF_MEM);
		return;
	}

	set_connection_state(conn, S_CONN);
	set_connection_timeout(conn);
	conn->from = 0;

	init_bittorrent_fetch(&bittorrent->fetch, uri,
			      bittorrent_metainfo_callback, conn, 0);
	done_uri(uri);
}
