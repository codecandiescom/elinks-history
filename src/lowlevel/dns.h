/* $Id: dns.h,v 1.9 2005/04/13 14:48:25 jonas Exp $ */

#ifndef EL__LOWLEVEL_DNS_H
#define EL__LOWLEVEL_DNS_H

#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

typedef void (*dns_callback_T)(void *, int);

int do_real_lookup(unsigned char *, struct sockaddr_storage **, int *, int);
void shrink_dns_cache(int);
int find_host(unsigned char *, struct sockaddr_storage **, int *, void **, dns_callback_T, void *, int);
void kill_dns_request(void **);

#endif
