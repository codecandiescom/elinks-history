/* Domain Name System Resolver Department */
/* $Id: dns.c,v 1.40 2003/12/21 14:51:20 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <netdb.h> /* OS/2 needs this after sys/types.h */
#include <sys/socket.h> /* OS/2 needs this after sys/types.h */
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* Go and say 'thanks' to BSD. */
#include <netinet/in.h>
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include "elinks.h"

#include "config/options.h"
#include "lowlevel/dns.h"
#include "lowlevel/select.h"
#include "osdep/osdep.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/ttime.h"


struct dnsentry {
	LIST_HEAD(struct dnsentry);

	struct sockaddr_storage *addr; /* pointer to array of addresses */
	ttime get_time;
	int addrno; /* array len / sizeof(sockaddr_storage) */
	unsigned char name[1]; /* Must be last */
};


struct dnsquery {
#ifdef THREAD_SAFE_LOOKUP
	struct dnsquery *next_in_queue;
#endif
	void (*fn)(void *, int);
	void *data;
	void (*xfn)(struct dnsquery *, int);
	struct dnsquery **s;
	/* addr and addrno lifespan exceeds life of this structure, the caller
	 * holds memory being pointed upon be these functions. Thus, when
	 * free()ing, *always* set pointer to NULL ! */
	struct sockaddr_storage **addr; /* addr of pointer to array of addresses */
	int *addrno; /* array len / sizeof(sockaddr_storage) */
	int h;
	unsigned char name[1]; /* Must be last */
};


#ifdef THREAD_SAFE_LOOKUP
static struct dnsquery *dns_queue = NULL;
#endif

INIT_LIST_HEAD(dns_cache);


int
do_real_lookup(unsigned char *name, struct sockaddr_storage **addrs, int *addrno,
	       int in_thread)
{
#ifdef IPV6
	struct addrinfo hint, *ai, *ai_cur;
#else
	struct hostent *hostent;
#endif
	int i;

	if (!name || !addrs || !addrno)
		return -1;

#ifdef IPV6
	/* I had a strong preference for the following, but the glibc is really
	 * obsolete so I had to rather use much more complicated getaddrinfo().
	 * But we duplicate the code terribly here :|. */
	/* hostent = getipnodebyname(name, AF_INET6, AI_ALL | AI_ADDRCONFIG, NULL); */
	memset(&hint, 0, sizeof(hint));
	hint.ai_family = AF_UNSPEC;
	hint.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(name, NULL, &hint, &ai) != 0) return -1;

#else
	/* Seems there are problems on Mac, so we first need to try
	 * gethostbyaddr(). */
#ifdef HAVE_GETHOSTBYADDR
	hostent = gethostbyaddr(name, strlen(name), AF_INET);
	if (!hostent)
#endif
	{
		hostent = gethostbyname(name);
		if (!hostent) return -1;
	}
#endif

#ifdef IPV6
	for (i = 0, ai_cur = ai; ai_cur; i++, ai_cur = ai_cur->ai_next);
#else
	for (i = 0; hostent->h_addr_list[i] != NULL; i++);
#endif

	/* We cannot use mem_*() in thread ("It will chew memory on OS/2 and
	 * BeOS because there are no locks around the memory debugging code."
	 * -- Mikulas).  So we don't if in_thread != 0. */
	*addrs = in_thread ? calloc(i, sizeof(struct sockaddr_storage))
			   : mem_calloc(i, sizeof(struct sockaddr_storage));
	if (!*addrs) return -1;
	*addrno = i;

#ifdef IPV6
	for (i = 0, ai_cur = ai; ai_cur; i++, ai_cur = ai_cur->ai_next) {
		struct sockaddr_in6 *addr = (struct sockaddr_in6 *) &(*addrs)[i];

		memcpy(addr, ai_cur->ai_addr, ai_cur->ai_addrlen);
	}

	freeaddrinfo(ai);

#else
	for (i = 0; hostent->h_addr_list[i] != NULL; i++) {
		struct sockaddr_in *addr = (struct sockaddr_in *) &(*addrs)[i];

		addr->sin_family = hostent->h_addrtype;
		memcpy(&addr->sin_addr.s_addr, hostent->h_addr_list[i], hostent->h_length);
	}
#endif

	return 0;
}

static void
lookup_fn(void *data, int h)
{
	unsigned char *name = (unsigned char *) data;
	struct sockaddr_storage *addrs;
	int addrno, i, w;

	if (do_real_lookup(name, &addrs, &addrno, 1) < 0) return;

	/* We will do blocking I/O here, however it's only local communication
	 * and it's supposed to be just a flash talk, so it shouldn't matter.
	 * And it would be incredibly more complicated and messy (and mainly
	 * useless) to do this in non-blocking way. */
	if (set_blocking_fd(h) < 0) return;

	w = safe_write(h, &addrno, sizeof(int));
	if (w != sizeof(int)) return;

	for (i = 0; i < addrno; i++) {
		int done = 0;

		do {
			struct sockaddr_storage *addr = &addrs[i];

			w = safe_write(h, addr + done, sizeof(struct sockaddr_storage) - done);
			if (w < 0) return;
			done += w;
		} while (done < sizeof(struct sockaddr_storage));
	}

	/* We're in thread, thus we must do plain free(). */
	free(addrs);
}

static void
end_real_lookup(void *data)
{
	struct dnsquery *query = (struct dnsquery *) data;
	int res = -1;
	int i, r;

	if (!query->addr || !query->addrno) goto done;

	*query->addr = NULL; /* XXX: is this correct ?? --Zas */

	/* We will do blocking I/O here, however it's only local communication
	 * and it's supposed to be just a flash talk, so it shouldn't matter.
	 * And it would be incredibly more complicated and messy (and mainly
	 * useless) to do this in non-blocking way. */
	if (set_blocking_fd(query->h) < 0) goto done;

	r = safe_read(query->h, query->addrno, sizeof(int));
	if (r != sizeof(int)) goto done;

	*query->addr = mem_calloc(*query->addrno, sizeof(struct sockaddr_storage));
	if (!*query->addr) goto done;

	for (i = 0; i < *query->addrno; i++) {
		int done = 0;

		do {
			struct sockaddr_storage *addr = &(*query->addr)[i];

			r = safe_read(query->h, addr + done, sizeof(struct sockaddr_storage) - done);
			if (r <= 0) goto done;
			done += r;
		} while (done < sizeof(struct sockaddr_storage));
	}

	res = 0;

done:
	if (res < 0 && query->addr && *query->addr) {
		mem_free(*query->addr);
		*query->addr = NULL;
	}

	set_handlers(query->h, NULL, NULL, NULL, NULL);
	close(query->h);
	query->xfn(query, res);
}

static void
failed_real_lookup(void *data)
{
	struct dnsquery *query = (struct dnsquery *) data;

	set_handlers(query->h, NULL, NULL, NULL, NULL);
	close(query->h);
	query->xfn(query, -1);
}

static int
do_lookup(struct dnsquery *query, int force_async)
{
	int res;

	/* DBG("starting lookup for %s", q->name); */

#ifndef NO_ASYNC_LOOKUP
	if (!force_async && !get_opt_int("connection.async_dns")) {
#endif
		goto sync_lookup; /* To use label, even if NO_ASYNC_LOOKUP is defined. */

#ifndef NO_ASYNC_LOOKUP
	} else {
		query->h = start_thread(lookup_fn, query->name, strlen(query->name) + 1);
		if (query->h == -1) goto sync_lookup;
		set_handlers(query->h, end_real_lookup, NULL, failed_real_lookup, query);

		return 1;
	}
#endif

sync_lookup:
	res = do_real_lookup(query->name, query->addr, query->addrno, 0);
	query->xfn(query, res);

	return 0;
}

static int
do_queued_lookup(struct dnsquery *query)
{
#ifdef THREAD_SAFE_LOOKUP
	query->next_in_queue = NULL;

	if (!dns_queue) {
		dns_queue = query;
		/* DBG("direct lookup"); */
		return do_lookup(query, 0);

	} else {
		/* DBG("queuing lookup for %s", q->name); */
		assertm(!dns_queue->next_in_queue, "DNS queue corrupted");
		dns_queue->next_in_queue = query;
		dns_queue = query;
		return -1;
	}
#else
	return do_lookup(query, 0);
#endif
}

static int
find_in_dns_cache(unsigned char *name, struct dnsentry **dnsentry)
{
	struct dnsentry *e;

	foreach (e, dns_cache)
		if (!strcasecmp(e->name, name)) {
			del_from_list(e);
			add_to_list(dns_cache, e);
			*dnsentry = e;
			return 0;
		}
	return -1;
}

static void
end_dns_lookup(struct dnsquery *q, int res)
{
	struct dnsentry *dnsentry;
	void (*fn)(void *, int);
	void *data;
	int namelen;

	/* DBG("end lookup %s (%d)", q->name, res); */

#ifdef THREAD_SAFE_LOOKUP
	if (q->next_in_queue) {
		/* DBG("processing next in queue: %s", q->next_in_queue->name); */
		do_lookup(q->next_in_queue, 1);
	} else {
	       	dns_queue = NULL;
	}
#endif

	if (!q->fn || !q->addr) {
		if (q->addr && *q->addr) {
			mem_free(*q->addr);
			*q->addr = NULL; /* This may not be so pointless. */
		}
		mem_free(q);
		return;
	}

	if (find_in_dns_cache(q->name, &dnsentry) >= 0) {
		if (res < 0) {
			/* q->addr(no) is pointer to something already allocated */

			assert(dnsentry && dnsentry->addrno > 0);

			*q->addr = mem_calloc(dnsentry->addrno,
					      sizeof(struct sockaddr_storage));
			if (!*q->addr) goto done;

			memcpy(*q->addr, dnsentry->addr, sizeof(struct sockaddr_storage)
							 * dnsentry->addrno);

			*q->addrno = dnsentry->addrno;

			res = 0;
			goto done;
		}

		del_from_list(dnsentry);
		mem_free(dnsentry->addr);
		mem_free(dnsentry);
	}

	if (res < 0) goto done;

	namelen = strlen(q->name);
	dnsentry = mem_calloc(1, sizeof(struct dnsentry) + namelen);

	if (dnsentry) {
		memcpy(dnsentry->name, q->name, namelen); /* calloc() sets nul char for us. */

		assert(*q->addrno > 0);

		dnsentry->addr = mem_calloc(*q->addrno, sizeof(struct sockaddr_storage));
		if (!dnsentry->addr) goto done;

		memcpy(dnsentry->addr, *q->addr, *q->addrno * sizeof(struct sockaddr_storage));

		dnsentry->addrno = *q->addrno;

		dnsentry->get_time = get_time();
		add_to_list(dns_cache, dnsentry);
	}

done:
	fn = q->fn;
	data = q->data;

	if (q->s) *q->s = NULL;
	/* q->addr is freed later by dns_found() */
	mem_free(q);

	fn(data, res);
}

int
find_host_no_cache(unsigned char *name, struct sockaddr_storage **addr, int *addrno,
		   void **query_p, void (*fn)(void *, int), void *data)
{
	struct dnsquery *query;
	int namelen = strlen(name);

	query = mem_calloc(1, sizeof(struct dnsquery) + namelen);
	if (!query) {
		fn(data, -1);
		return 0;
	}

	query->fn = fn;
	query->data = data;
	query->s = (struct dnsquery **) query_p;
	query->addr = addr;
	query->addrno = addrno;
	memcpy(query->name, name, namelen); /* calloc() sets nul char for us. */

	if (query_p) *((struct dnsquery **) query_p) = query;
	query->xfn = end_dns_lookup;

	return do_queued_lookup(query);
}

int
find_host(unsigned char *name, struct sockaddr_storage **addr, int *addrno,
	  void **query_p, void (*fn)(void *, int), void *data)
{
	struct dnsentry *dnsentry;

	if (query_p) *query_p = NULL;

	if (find_in_dns_cache(name, &dnsentry) >= 0) {
		if (dnsentry->get_time + DNS_TIMEOUT < get_time())
			goto timeout;

		assert(dnsentry && dnsentry->addrno);

		*addr = mem_calloc(dnsentry->addrno, sizeof(struct sockaddr_storage));
		if (*addr) {
			memcpy(*addr, dnsentry->addr, sizeof(struct sockaddr_storage)
						      * dnsentry->addrno);
			*addrno = dnsentry->addrno;
			fn(data, 0);
		}
		return 0;
	}

timeout:
	return find_host_no_cache(name, addr, addrno, query_p, fn, data);
}

void
kill_dns_request(void **qp)
{
	struct dnsquery *query = *qp;

	query->fn = NULL;
	failed_real_lookup(query);
	*qp = NULL;
}

static void
del_dns_cache_entry(struct dnsentry **d)
{
	struct dnsentry *e = *d;

	*d = (*d)->prev;
	del_from_list(e);
	if (e->addr) mem_free(e->addr);
	mem_free(e);
}

void
shrink_dns_cache(int whole)
{
	struct dnsentry *d;

	if (whole) {
		foreach (d, dns_cache)
			del_dns_cache_entry(&d);

	} else {
		ttime oldest = get_time() - DNS_TIMEOUT;

		foreach (d, dns_cache)
			if (d->get_time < oldest)
				del_dns_cache_entry(&d);
	}
}
