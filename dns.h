#ifndef EL__DNS_H
#define EL__DNS_H

#include <sys/socket.h>
#include <sys/types.h>

int do_real_lookup(unsigned char *, struct sockaddr **, int *);
void shrink_dns_cache(int);
int find_host(unsigned char *, struct sockaddr **, int *, void **, void (*)(void *, int), void *);
int find_host_no_cache(unsigned char *, struct sockaddr **, int *, void **, void (*)(void *, int), void *);
void kill_dns_request(void **);

#endif
