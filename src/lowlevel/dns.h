/* $Id: dns.h,v 1.4 2003/06/28 10:47:53 jonas Exp $ */

#ifndef EL__LOWLEVEL_DNS_H
#define EL__LOWLEVEL_DNS_H

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
