/* $Id: dns.h,v 1.3 2003/04/16 21:16:27 pasky Exp $ */

#ifndef EL__DNS_H
#define EL__DNS_H

/* We MAY have problems with this. If there will be any, just tell me, and
 * I will move it to start of links.h. */
#include <sys/socket.h>
#include <sys/types.h>

int do_real_lookup(unsigned char *, struct sockaddr_storage **, int *, int);
void shrink_dns_cache(int);
int find_host(unsigned char *, struct sockaddr_storage **, int *, void **, void (*)(void *, int), void *);
int find_host_no_cache(unsigned char *, struct sockaddr_storage **, int *, void **, void (*)(void *, int), void *);
void kill_dns_request(void **);

#endif
