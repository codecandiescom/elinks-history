#include "links.h"

struct dnsentry {
	struct dnsentry *next;
	struct dnsentry *prev;
	ttime get_time;
	struct sockaddr *addr; /* pointer to array of addresses */
	int addrno; /* array len / sizeof(sockaddr) */
	char name[1];
};

#define DNS_PIPE_BUFFER	32

#ifndef THREAD_SAFE_LOOKUP
struct dnsquery *dns_queue = NULL;
#endif

struct dnsquery {
#ifndef THREAD_SAFE_LOOKUP
	struct dnsquery *next_in_queue;
#endif
	void (*fn)(void *, int);
	void *data;
	void (*xfn)(struct dnsquery *, int);
	int h;
	struct dnsquery **s;
	struct sockaddr **addr; /* addr of pointer to array of addresses */
	int *addrno; /* array len / sizeof(sockaddr) */
	char name[1];
};

struct list_head dns_cache = {&dns_cache, &dns_cache};

int do_real_lookup(unsigned char *name, struct sockaddr **addrs, int *addrno)
{
	struct hostent *hostent;
	int i;
	
	hostent = gethostbyname(name);
	if (!hostent) return -1;

	for (i = 0; hostent->h_addr_list[i] != NULL; i++);

	*addrno = i;
	*addrs = mem_alloc(i * sizeof(struct sockaddr));

	for (i = 0; hostent->h_addr_list[i] != NULL; i++) {
		struct sockaddr_in *addr = (struct sockaddr_in *) *addrs;
		
		addr[i].sin_family = hostent->h_addrtype;
		memcpy(&addr[i].sin_addr.s_addr, hostent->h_addr_list[i], sizeof(struct in_addr));
	}
	
	return 0;
}

void lookup_fn(void *data, int h)
{
	unsigned char *name = (unsigned char *) data;
	struct sockaddr *addrs;
	int addrno, i;
	
	if (do_real_lookup(name, &addrs, &addrno) < 0) return;

	/* We will do blocking I/O here, however it's only local communication
	 * and it's supposed to be just a flash talk, so it shouldn't matter.
	 * And it would be incredibly more complicated and messy (and mainly
	 * useless) to do this in non-blocking way. */
	fcntl(h, F_SETFL, ~O_NONBLOCK & fcntl(h, F_GETFL));

	write(h, &addrno, sizeof(int));
	
	for (i = 0; i < addrno; i++) {
		int done = 0;

		do {
			int w;

			w = write(h, &addrs[i] + done, sizeof(struct sockaddr) - done);
			if (w < 0) return;
			done += w;
		} while (done < sizeof(struct sockaddr));
	}
}

void end_real_lookup(void *data)
{
	struct dnsquery *query = (struct dnsquery *) data;
	int res = -1;
	int i;
	
	if (!query->addr || !query->addrno)
		goto done;

	*query->addr = NULL;

	/* We will do blocking I/O here, however it's only local communication
	 * and it's supposed to be just a flash talk, so it shouldn't matter.
	 * And it would be incredibly more complicated and messy (and mainly
	 * useless) to do this in non-blocking way. */
	fcntl(query->h, F_SETFL, ~O_NONBLOCK & fcntl(query->h, F_GETFL));
	
	if (read(query->h, query->addrno, sizeof(int)) != sizeof(int))
		goto done;

	*query->addr = mem_alloc((*query->addrno + 1) * sizeof(struct sockaddr));

	for (i = 0; i < *query->addrno; i++) {
		int done = 0;

		do {
			int r;

			r = read(query->h, &(*query->addr)[i] + done, sizeof(struct sockaddr) - done);
			if (r < 0) goto done;
			done += r;
		} while (done < sizeof(struct sockaddr));
	}

	res = 0;
	
done:
	if (res < 0 && query->addr && *query->addr) mem_free(*query->addr);
		
	set_handlers(query->h, NULL, NULL, NULL, NULL);
	close(query->h);
	query->xfn(query, res);
}

void failed_real_lookup(void *data)
{
	struct dnsquery *query = (struct dnsquery *) data;
	
	set_handlers(query->h, NULL, NULL, NULL, NULL);
	close(query->h);
	query->xfn(query, -1);
}

int do_lookup(struct dnsquery *query, int force_async)
{
	/* debug("starting lookup for %s", q->name); */
#ifndef NO_ASYNC_LOOKUP
	if (!async_lookup && !force_async) {
#endif
		int res;
		
		sync_lookup:
		res = do_real_lookup(query->name, query->addr, query->addrno);
		query->xfn(query, res);
		
		return 0;
		
#ifndef NO_ASYNC_LOOKUP
	} else {
		query->h = start_thread(lookup_fn, query->name, strlen(query->name) + 1);
		if (query->h == -1) goto sync_lookup;
		set_handlers(query->h, end_real_lookup, NULL, failed_real_lookup, query);
		
		return 1;
	}
#endif
}

int do_queued_lookup(struct dnsquery *query)
{
#ifndef THREAD_SAFE_LOOKUP
	q->next_in_queue = NULL;
	
	if (!dns_queue) {
		dns_queue = query;
		/* debug("direct lookup"); */
#endif
		
		return do_lookup(query, 0);
		
#ifndef THREAD_SAFE_LOOKUP
	} else {
		/* debug("queuing lookup for %s", q->name); */
		if (dns_queue->next_in_queue) internal("DNS queue corrupted");
		dns_queue->next_in_queue = query;
		dns_queue = query;
		return -1;
	}
#endif
}

int find_in_dns_cache(char *name, struct dnsentry **dnsentry)
{
	struct dnsentry *e;
	foreach(e, dns_cache)
		if (!strcasecmp(e->name, name)) {
			del_from_list(e);
			add_to_list(dns_cache, e);
			*dnsentry=e;
			return 0;
		}
	return -1;
}

void end_dns_lookup(struct dnsquery *q, int res)
{
	struct dnsentry *dnsentry;
	void (*fn)(void *, int);
	void *data;
	
	/* debug("end lookup %s (%d)", q->name, res); */
	
#ifndef THREAD_SAFE_LOOKUP
	if (q->next_in_queue) {
		/* debug("processing next in queue: %s", q->next_in_queue->name); */
		do_lookup(q->next_in_queue, 1);
	} else {
	       	dns_queue = NULL;
	}
#endif
	
	if (!q->fn || !q->addr) {
		if (q->addr && *q->addr) mem_free(*q->addr);
		mem_free(q);
		return;
	}
	
	if (!(find_in_dns_cache(q->name, &dnsentry) < 0)) {
		if (res < 0) {
			/* q->addr(no) is pointer to something already allocated */

			*q->addr = mem_alloc(sizeof(struct sockaddr) * dnsentry->addrno);
			if (!*q->addr) goto done;
			
			memcpy(*q->addr, dnsentry->addr, sizeof(struct sockaddr) * dnsentry->addrno);
			
			*q->addrno = dnsentry->addrno;
			
			res = 0;
			goto done;
		}
	
		del_from_list(dnsentry);
		mem_free(dnsentry->addr);
		mem_free(dnsentry);
	}
	
	if (res < 0) goto done;
		
	dnsentry = mem_alloc(sizeof(struct dnsentry) + strlen(q->name) + 1);
	
	if (dnsentry) {
		strcpy(dnsentry->name, q->name);

		dnsentry->addr = mem_alloc(sizeof(struct sockaddr) * *q->addrno);
		if (!dnsentry->addr) goto done;
		
		memcpy(dnsentry->addr, *q->addr, sizeof(struct sockaddr) * *q->addrno);
		
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

int find_host_no_cache(unsigned char *name, struct sockaddr **addr, int *addrno,
		       void **query_p, void (*fn)(void *, int), void *data)
{
	struct dnsquery *query;
	
	query = mem_alloc(sizeof(struct dnsquery) + strlen(name) + 1);
	if (!query) {
		fn(data, -1);
		return 0;
	}
	
	query->fn = fn;
	query->data = data;
	query->s = (struct dnsquery **) query_p;
	query->addr = addr;
	query->addrno = addrno;

	strcpy(query->name, name);
	if (query_p) *((struct dnsquery **) query_p) = query;
	query->xfn = end_dns_lookup;
	
	return do_queued_lookup(query);
}

int find_host(unsigned char *name, struct sockaddr **addr, int *addrno,
	      void **query_p, void (*fn)(void *, int), void *data)
{
	struct dnsentry *dnsentry;
	
	if (query_p) *query_p = NULL;
	
	if (!(find_in_dns_cache(name, &dnsentry) < 0)) {
		if (dnsentry->get_time + DNS_TIMEOUT < get_time())
			goto timeout;
		
		*addr = mem_alloc(sizeof(struct sockaddr) * dnsentry->addrno);
		memcpy(*addr, dnsentry->addr, sizeof(struct sockaddr) * dnsentry->addrno);
		*addrno = dnsentry->addrno;
		fn(data, 0);
		return 0;
	}
	
	timeout:
	return find_host_no_cache(name, addr, addrno, query_p, fn, data);
}

void kill_dns_request(void **qp)
{
	struct dnsquery *q = *qp;
	
	q->fn = NULL;
	mem_free(q->addr); q->addr = NULL;
	*qp = NULL;
}

void shrink_dns_cache(int whole)
{
	struct dnsentry *d, *e;

	foreach(d, dns_cache) {
		if (whole || d->get_time + DNS_TIMEOUT < get_time()) {
			e = d;
			d = d->prev;
			del_from_list(e);
			
			mem_free(e->addr);
			mem_free(e);
		}
	}
}
