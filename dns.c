#include "links.h"

struct dnsentry {
	struct dnsentry *next;
	struct dnsentry *prev;
	ttime get_time;
	ip addr;
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
	ip *addr;
	char name[1];
};

struct list_head dns_cache = {&dns_cache, &dns_cache};

int do_real_lookup(unsigned char *name, ip *host_ip)
{
	struct hostent *host;
	
	host = gethostbyname(name);
	if (!host) return -1;
	
	memcpy(host_ip, host->h_addr_list[0], sizeof(ip));
	
	return 0;
}

void lookup_fn(void *data, int h)
{
	unsigned char *name = (unsigned char *) data;
	ip host_ip;
	
	if (do_real_lookup(name, &host_ip)) return;
	
	write(h, &host_ip, sizeof(ip));
}

void end_real_lookup(void *data)
{
	struct dnsquery *query = (struct dnsquery *) data;
	int res = 0;
	
	if (!query->addr || read(query->h, query->addr, sizeof(ip)) != sizeof(ip))
		res = 1;

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
		res = do_real_lookup(query->name, query->addr);
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
		return 1;
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

void end_dns_lookup(struct dnsquery *q, int a)
{
	struct dnsentry *dnsentry;
	void (*fn)(void *, int);
	void *data;
	/*debug("end lookup %s", q->name);*/
#ifndef THREAD_SAFE_LOOKUP
	if (q->next_in_queue) {
		/*debug("processing next in queue: %s", q->next_in_queue->name);*/
		do_lookup(q->next_in_queue, 1);
	} else dns_queue = NULL;
#endif
	if (!q->fn || !q->addr) {
		free(q);
		return;
	}
	if (!find_in_dns_cache(q->name, &dnsentry)) {
		if (a) {
			memcpy(q->addr, &dnsentry->addr, sizeof(ip));
			a = 0;
			goto e;
		}
		del_from_list(dnsentry);
		mem_free(dnsentry);
	}
	if (a) goto e;
	if ((dnsentry = mem_alloc(sizeof(struct dnsentry) + strlen(q->name) + 1))) {
		strcpy(dnsentry->name, q->name);
		memcpy(&dnsentry->addr, q->addr, sizeof(ip));
		dnsentry->get_time = get_time();
		add_to_list(dns_cache, dnsentry);
	}
	e:
	if (q->s) *q->s = NULL;
	fn = q->fn;
	data = q->data;
	free(q);
	fn(data, a);
}

int find_host_no_cache(unsigned char *name, ip *addr, void **query_p,
		       void (*fn)(void *, int), void *data)
{
	struct dnsquery *query;
	
	query = malloc(sizeof(struct dnsquery) + strlen(name) + 1);
	if (!query) {
		fn(data, -1);
		return 0;
	}
	
	query->fn = fn;
	query->data = data;
	query->s = (struct dnsquery **) query_p;
	query->addr = addr;

	strcpy(query->name, name);
	if (query_p) *(struct dnsquery **) query_p = query;
	query->xfn = end_dns_lookup;
	
	return do_queued_lookup(query);
}

int find_host(unsigned char *name, ip *addr, void **query_p,
	      void (*fn)(void *, int), void *data)
{
	struct dnsentry *dnsentry;
	
	if (query_p) *query_p = NULL;
	
	if (!find_in_dns_cache(name, &dnsentry)) {
		if (dnsentry->get_time + DNS_TIMEOUT < get_time())
			goto timeout;
		
		memcpy(addr, &dnsentry->addr, sizeof(ip));
		fn(data, 0);
		return 0;
	}
	
	timeout:
	return find_host_no_cache(name, addr, query_p, fn, data);
}

void kill_dns_request(void **qp)
{
	struct dnsquery *q = *qp;
	/*set_handlers(q->h, NULL, NULL, NULL, NULL);
	close(q->h);
	mem_free(q);*/
	q->fn = NULL;
	q->addr = NULL;
	*qp = NULL;
}

void shrink_dns_cache(int u)
{
	struct dnsentry *d, *e;
	foreach(d, dns_cache) if (u || d->get_time + DNS_TIMEOUT < get_time()) {
		e = d;
		d = d->prev;
		del_from_list(e);
		mem_free(e);
	}
}
