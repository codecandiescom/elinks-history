#include "links.h"

struct {
	unsigned char *prot;
	int port;
	void (*func)(struct connection *);
	void (*nc_func)(struct session *, unsigned char *);
	int free_syntax;
	int need_slashes;
	int need_slash_after_host;
} protocols[]= {
		{"file", 0, file_func, NULL, 1, 1, 0},
		{"https", 443, https_func, NULL, 0, 1, 1},
		{"http", 80, http_func, NULL, 0, 1, 1},
		{"proxy", 3128, proxy_func, NULL, 0, 1, 1},
		{"ftp", 21, ftp_func, NULL, 0, 1, 1},
		{"finger", 79, finger_func, NULL, 0, 1, 1},
		{"mailto", 0, NULL, mailto_func, 0, 0, 0},
		{"telnet", 0, NULL, telnet_func, 0, 0, 0},
		{"tn3270", 0, NULL, tn3270_func, 0, 0, 0},
		{"user", 0, NULL, NULL, 0, 0, 0},
		{NULL, 0, NULL}
};

int check_protocol(unsigned char *p, int l)
{
	int i;
	for (i = 0; protocols[i].prot; i++)
		if (!casecmp(protocols[i].prot, p, l)) {
			return i;
		}
	return -1;
}

int get_prot_info(unsigned char *prot, int *port, void (**func)(struct connection *), void (**nc_func)(struct session *ses, unsigned char *))
{
	int i;
	for (i = 0; protocols[i].prot; i++)
		if (!strcasecmp(protocols[i].prot, prot)) {
			if (port) *port = protocols[i].port;
			if (func) *func = protocols[i].func;
			if (nc_func) *nc_func = protocols[i].nc_func;
			return 0;
		}
	return -1;
}

int parse_url(unsigned char *url, int *prlen,
	      unsigned char **user, int *uslen,
	      unsigned char **pass, int *palen,
	      unsigned char **host, int *holen,
	      unsigned char **port, int *polen,
	      unsigned char **data, int *dalen,
	      unsigned char **post)
{
	unsigned char *prefix_end, *host_end;
#ifdef IPV6
	unsigned char *lbracket, *rbracket;
	static unsigned char hostbuf[NI_MAXHOST];
#endif
	int protocol;
	
	if (prlen) *prlen = 0;
	if (user) *user = NULL;
	if (uslen) *uslen = 0;
	if (pass) *pass = NULL;
	if (palen) *palen = 0;
	if (host) *host = NULL;
	if (holen) *holen = 0;
	if (port) *port = NULL;
	if (polen) *polen = 0;
	if (data) *data = NULL;
	if (dalen) *dalen = 0;
	if (post) *post = NULL;

	if (!url) return -1;

#ifdef IPV6
	/* Get brackets enclosing IPv6 address */
	lbracket = strchr(url, '[');
	rbracket = strchr(url, ']');
	if (lbracket > rbracket) return -1;
#endif
	
	/* Isolate prefix */
	
	prefix_end = strchr(url, ':');
	if (!prefix_end) return -1;
	
	if (prlen) *prlen = prefix_end - url;

	/* Get protocol */
	
	protocol = check_protocol(url, prefix_end - url);
	if (protocol == -1) return -1;

	prefix_end++; /* ':' */

	/* Skip slashes */
	
	if (prefix_end[0] == '/' && prefix_end[1] == '/') {
		prefix_end += 2;
	} else {
		if (protocols[protocol].need_slashes) return -1;
	}
	
	if (protocols[protocol].free_syntax) {
		if (data) *data = prefix_end;
		if (dalen) *dalen = strlen(prefix_end);
		return 0;
	}

	/* Isolate host */
	
	host_end = prefix_end + strcspn(prefix_end, "@");
	if (prefix_end + strcspn(prefix_end, "/") > host_end
	    && *host_end) { /* we have auth info here */
		unsigned char *user_end = strchr(prefix_end, ':');
		
		if (!user_end || user_end > host_end) {
			if (user) *user = prefix_end;
			if (uslen) *uslen = host_end - prefix_end;
		} else {
			if (user) *user = prefix_end;
			if (uslen) *uslen = user_end - prefix_end;
			if (pass) *pass = user_end + 1;
			if (palen) *palen = host_end - user_end - 1;
		}
		prefix_end = host_end + 1;
	}
	
#ifdef IPV6
	/* [address] is permitted only inside hostname part. */
	if (prefix_end + strcspn(prefix_end, "/") < rbracket)
		lbracket = rbracket = NULL;
	
	if (lbracket && rbracket)
		host_end = rbracket + strcspn(rbracket, ":/");
	else
#endif
		host_end = prefix_end + strcspn(prefix_end, ":/");
		
	if (!*host_end && protocols[protocol].need_slash_after_host) return -1;

#ifdef IPV6
	if (lbracket && rbracket) {
		safe_strncpy(hostbuf, lbracket + 1, rbracket - lbracket - 1);
	}
#endif
	if (host) {
#ifdef IPV6
		if (lbracket && rbracket)
			*host = hostbuf;
		else
#endif
			*host = prefix_end;
	}
	if (holen) {
#ifdef IPV6
		if (lbracket && rbracket)
			*holen = strlen(hostbuf);
		else
#endif
			*holen = host_end - prefix_end;
	}
				
	
	if (*host_end == ':') { /* we have port here */
		unsigned char *port_end = host_end + strcspn(host_end, "/");
		int idx;
		
		if (port) *port = host_end + 1;
		if (polen) *polen = port_end - host_end - 1;
		
		/* test if port is number */
		/* TODO: possibly lookup for the service otherwise? --pasky */
		for (idx = 1; idx < port_end - host_end; idx++)
			if (host_end[idx] < '0' || host_end[idx] > '9')
				return -1;
		
		host_end = port_end;
	}
	
	if (*host_end) host_end++;
	
	prefix_end = strchr(host_end, POST_CHAR);
	if (data) *data = host_end;
	if (dalen) *dalen = prefix_end ? (prefix_end - host_end) : strlen(host_end);
	if (post) *post = prefix_end ? (prefix_end + 1) : NULL;
	
	return 0;
}

unsigned char *get_protocol_name(unsigned char *url)
{
	int l;
	if (parse_url(url, &l, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)) return NULL;
	return memacpy(url, l);
}

unsigned char *get_host_and_pass(unsigned char *url)
{
	unsigned char *u, *h, *p, *z, *k;
	int hl, pl;
	if (parse_url(url, NULL, &u, NULL, NULL, NULL, &h, &hl, &p, &pl, NULL, NULL, NULL)) return NULL;
	z = u ? u : h;
	k = p ? p + pl : h + hl;
	return memacpy(z, k - z);
}

unsigned char *get_host_name(unsigned char *url)
{
	unsigned char *h;
	int hl;
	if (parse_url(url, NULL, NULL, NULL, NULL, NULL, &h, &hl, NULL, NULL, NULL, NULL, NULL)) return stracpy("");
	return memacpy(h, hl);
}

unsigned char *get_user_name(unsigned char *url)
{
	unsigned char *h;
	int hl;
	if (parse_url(url, NULL, &h, &hl, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)) return NULL;
	return memacpy(h, hl);
}

unsigned char *get_pass(unsigned char *url)
{
	unsigned char *h;
	int hl;
	if (parse_url(url, NULL,NULL,  NULL, &h, &hl, NULL, NULL, NULL, NULL, NULL, NULL, NULL)) return NULL;
	return memacpy(h, hl);
}

unsigned char *get_port_str(unsigned char *url)
{
	unsigned char *h;
	int hl;
	if (parse_url(url, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &h, &hl, NULL, NULL, NULL)) return NULL;
	return hl ? memacpy(h, hl) : NULL;
}

int get_port(unsigned char *url)
{
	unsigned char *h;
	int hl;
	int n = -1;
	if (parse_url(url, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &h, &hl, NULL, NULL, NULL)) return -1;
	if (h) {
		n = strtol(h, NULL, 10);
		if (n) return n;
	}
	if ((h = get_protocol_name(url))) {
		get_prot_info(h, &n, NULL, NULL);
		mem_free(h);
	}
	return n;
}

void (*get_protocol_handle(unsigned char *url))(struct connection *)
{
	unsigned char *p;
	void (*f)(struct connection *) = NULL;
	if (!(p = get_protocol_name(url))) return NULL;
	get_prot_info(p, NULL, &f, NULL);
	mem_free(p);
	return f;
}

void (*get_external_protocol_function(unsigned char *url))(struct session *, unsigned char *)
{
	unsigned char *p;
	void (*f)(struct session *, unsigned char *) = NULL;
	if (!(p = get_protocol_name(url))) return NULL;
	get_prot_info(p, NULL, NULL, &f);
	mem_free(p);
	return f;
}

unsigned char *get_url_data(unsigned char *url)
{
	unsigned char *d;
	if (parse_url(url, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &d, NULL, NULL)) return NULL;
	return d;
}

/*void translate_directories(unsigned char *url)
{
	unsigned char *p;
	unsigned char *dd = get_url_data(url);
	unsigned char *d = dd;
	if (!d || d == url || *--d != '/') return;
	r:
	p = d + strcspn(d, "/;?#");
	if (p[0] != '/') return;
	if (p[1] == '.' && p[2] == '/') {
		memmove(p, p + 2, strlen(p + 2) + 1);
		d = p;
		goto r;
	}
	if (p[1] == '.' && p[2] == '.' && p[3] == '/') {
		unsigned char *e;
		for (e = p - 1; e >= dd; e--) if (*e == '/') {
			memmove(e, p + 3, strlen(p + 3) + 1);
			d = e;
			goto r;
		}
		memmove(dd, p + 3, strlen(p + 3) + 1);
		d = dd;
		goto r;
	}
	d = p + 1;
	goto r;
}*/

#define dsep(x) (lo ? dir_sep(x) : (x) == '/')

void translate_directories(unsigned char *url)
{
	unsigned char *dd = get_url_data(url);
	unsigned char *s, *d;
	int lo = !casecmp(url, "file://", 7);
	if (!dd || dd == url/* || *--dd != '/'*/) return;
	if (!dsep(*dd)) dd--;
	s = dd;
	d = dd;
	r:
	if (end_of_dir(s[0])) {
		memmove(d, s, strlen(s) + 1);
		return;
	}
	if (dsep(s[0]) && s[1] == '.' && dsep(s[2])) {
		/**d++ = s[0];*/
		if (s == dd && !s[3]) goto p;
		s += 2;
		goto r;
	}
	if (dsep(s[0]) && s[1] == '.' && s[2] == '.' && dsep(s[3])) {
		unsigned char *d1 = d;
		while (d > dd) {
			d--;
			if (dsep(*d)) {
				if (d + 3 == d1 && d[1] == '.' && d[2] == '.') {
					d = d1;
					goto p;
				}
				goto b;
			}
		}
		/*d = d1;
		goto p;*/
		b:
		s += 3;
		goto r;
	}
	p:
	if ((*d++ = *s++)) goto r;
}

void insert_wd(unsigned char **up, unsigned char *cwd)
{
	unsigned char *url = *up;
	if (!url || !cwd || !*cwd) return;
	if (casecmp(url, "file://", 7)) return;
	if (dir_sep(url[7])) return;
#ifdef DOS_FS
	if (upcase(url[7]) >= 'A' && upcase(url[7]) <= 'Z' && url[8] == ':' && dir_sep(url[9])) return;
#endif
	if (!(url = mem_alloc(strlen(*up) + strlen(cwd) + 2))) return;
	memcpy(url, *up, 7);
	strcpy(url + 7, cwd);
	if (!dir_sep(cwd[strlen(cwd) - 1])) strcat(url, "/");
	strcat(url, *up + 7);
	mem_free(*up);
	*up = url;
}

unsigned char *join_urls(unsigned char *base, unsigned char *rel)
{
	unsigned char *p, *n, *pp;
	int l;
	int lo = !casecmp(base, "file://", 7);
	
	/* See RFC 1808 */

	if (rel[0] == '#') {
		if (!(n = stracpy(base))) return NULL;
		for (p = n; *p && *p != POST_CHAR && *p != '#'; p++) ;
		*p = 0;
		add_to_strn(&n, rel);
		translate_directories(n);
		return n;
	}
	if (rel[0] == '?') {
		if (!(n = stracpy(base))) return NULL;
		for (p = n; *p && *p != POST_CHAR && *p != '?' && *p != '#'; p++) ;
		*p = 0;
		add_to_strn(&n, rel);
		translate_directories(n);
		return n;
	}
	if (rel[0] == '/' && rel[1] == '/') {
		unsigned char *s, *n;
		if (!(s = strstr(base, "//"))) {
			internal("bad base url: %s", base);
			return NULL;
		}
		n = memacpy(base, s - base);
		add_to_strn(&n, rel);
		return n;
	}
	if (!casecmp("proxy://", rel, 8)) goto prx;
	if (!parse_url(rel, &l, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)) {
		n = stracpy(rel);
		translate_directories(n);
		return n;
	}
	if ((n = stracpy(rel))) {
		while (n[0] && n[strlen(n) - 1] <= ' ') n[strlen(n) - 1] = 0;
		add_to_strn(&n, "/");
		if (!parse_url(n, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)) {
			translate_directories(n);
			return n;
		}
		mem_free(n);
	}
	prx:
	if (parse_url(base, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &p, NULL, NULL) || !p) {
		internal("bad base url");
		return NULL;
	}
	if (!dsep(*p)) p--;
	if (!dsep(rel[0])) for (pp = p; *pp; pp++) {
		if (end_of_dir(*pp)) break;
		if (dsep(*pp)) p = pp + 1;
	}
	if (!(n = mem_alloc(p - base + strlen(rel) + 1))) return NULL;
	memcpy(n, base, p - base);
	strcpy(n + (p - base), rel);
	translate_directories(n);
	return n;
}

unsigned char *translate_url(unsigned char *url, unsigned char *cwd)
{
	unsigned char *ch;
	unsigned char *newurl;
	
	/* Strip starting spaces */
	while (*url == ' ') url++;
	
	if (!casecmp("proxy://", url, 8)) goto proxy;
	
	/* Ordinary parse */
	if (!parse_url(url, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)) {
		newurl = stracpy(url);
		insert_wd(&newurl, cwd);
		translate_directories(newurl);
		return newurl;
	}
	
	/* Try to add slash to end */
	if (strstr(url, "//") && (newurl = stracpy(url))) {
		add_to_strn(&newurl, "/");
		if (!parse_url(newurl, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)) {
			insert_wd(&newurl, cwd);
			translate_directories(newurl);
			return newurl;
		}
		mem_free(newurl);
	}
	
proxy:
	/* No protocol name */
	ch = url + strcspn(url, ".:/@");
#ifdef IPV6
	if (*ch != ':' || *url == '[' || url[strcspn(url, "/@")] == '@') {
#else
	if (*ch != ':' || url[strcspn(url, "/@")] == '@') {
#endif
		unsigned char *prefix = "file://";
		int not_file = 0;
		
		/* Yes, it would be simpler to make test for IPv6 address first,
		 * but it would result in confusing mix of ifdefs ;-). */

		if (*ch == '@' || (*ch == ':' && *url != '[') || !cmpbeg(url, "ftp.")) {
			/* Contains user/password/ftp-hostname */
			prefix = "ftp://";
			not_file = 1;
					
#ifdef IPV6	
		} else if (*url == '[' && *ch == ':') {
			/* Candidate for IPv6 address */
			char *bracket2, *colon2;
			
			ch++;
			bracket2 = strchr(ch, ']');
			colon2 = strchr(ch, ':');
			if (bracket2 && colon2 && bracket2 > colon2)
				goto http;
#endif

		} else if (*url != '.' && *ch == '.') {
			/* Contains domain name? */
			unsigned char *host_end, *domain;
			int i;
			
			/* Process the hostname */
			for (domain = ch + 1;
			     *(host_end = domain + strcspn(domain, ".:/")) == '.';
			     domain = host_end + 1);
			
			/* It's IP? */
			for (i = 0; i < host_end - domain; i++)
				if (domain[i] >= '0' && domain[i] <= '9')
					goto http;

			/* FIXME: Following is completely braindead.
			 * TODO: Remove it. We should rather first try file:// and
			 * then http://, if failed. But this will require wider
			 * modifications. :| --pasky */
		
			/* It's two-letter TLD? */
			if (host_end - domain == 2) {
http:				prefix = "http://";
				not_file = 1;
			
			} else if (host_end - domain == 3) {
				unsigned char *tld[] = { "com", "edu", "net", "org", "gov", "mil", "int", NULL };
				
				for (i = 0; tld[i]; i++)
					if (!casecmp(tld[i], domain, 3))
						goto http;
			}
		}
		
		newurl = stracpy(prefix);
		if (!newurl) return NULL;
		add_to_strn(&newurl, url);
		if (not_file && !strchr(url, '/')) add_to_strn(&newurl, "/");
		
		if (!parse_url(newurl, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)) {
			insert_wd(&newurl, cwd);
			translate_directories(newurl);
			return newurl;
		}
		
		mem_free(newurl);
		return NULL;
	}
	
	newurl = memacpy(url, ch - url + 1);
	if (!newurl) return NULL;
	add_to_strn(&newurl, "//");
	add_to_strn(&newurl, ch + 1);
	
	if (!parse_url(newurl, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)) {
		insert_wd(&newurl, cwd);
		translate_directories(newurl);
		return newurl;
	}
	
	add_to_strn(&newurl, "/");
	
	if (!parse_url(newurl, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)) {
		insert_wd(&newurl, cwd);
		translate_directories(newurl);
		return newurl;
	}
	
	mem_free(newurl);
	return NULL;
}

unsigned char *extract_position(unsigned char *url)
{
	unsigned char *u, *uu, *r;
	if (!(u = strchr(url, POST_CHAR))) u = url + strlen(url);
	uu = u;
	while (--uu >= url && *uu != '#') ;
	if (uu < url || !(r = mem_alloc(u - uu))) return NULL;
	memcpy(r, uu + 1, u - uu - 1);
	r[u - uu - 1] = 0;
	memmove(uu, u, strlen(u) + 1);
	return r;
}

void get_filename_from_url(unsigned char *url, unsigned char **s, int *l)
{
	int lo = !casecmp(url, "file://", 7);
	unsigned char *uu;
	if ((uu = get_url_data(url))) url = uu;
	*s = url;
	while (*url && !end_of_dir(*url)) {
		if (dsep(*url)) *s = url + 1;
		url++;
	}
	*l = url - *s;
}
