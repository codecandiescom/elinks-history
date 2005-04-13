/* $Id: dns.h,v 1.11 2005/04/13 21:22:59 jonas Exp $ */

#ifndef EL__LOWLEVEL_DNS_H
#define EL__LOWLEVEL_DNS_H

#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

typedef void (*dns_callback_T)(void *, struct sockaddr_storage *, int);

/* Look up the specified @host using synchronious querying. An array of found
 * addresses will be allocated in @addr with the array length stored in
 * @addrlen. The boolean @called_from_thread is a hack used internally to get
 * the correct allocation method. */
/* Returns non-zero on error and zero on success. */
int do_real_lookup(unsigned char *host, struct sockaddr_storage **addr, int *addrlen,
		   int called_from_thread);

/* Look up the specified @host storing private query information in struct
 * pointed to by @queryref. An array of found addresses will be allocated in
 * @addr with the array length stored in @addrlen. When the query is done the
 * @callback will be called with @data and an appropriate error state code. If
 * the boolean @no_cache is non-zero cached DNS queries are ignored. */
/* Returns whether the query is asynchronious. */
int find_host(unsigned char *host, struct sockaddr_storage **addr, int *addrlen,
	      void **queryref, dns_callback_T callback, void *data, int no_cache);

/* Stop the DNS request pointed to by the @queryref reference. */
void kill_dns_request(void **queryref);

/* Manage the cache of DNS lookups. If the boolean @whole is non-zero all DNS
 * cache entries will be removed. */
void shrink_dns_cache(int whole);

#endif
