/* AF_UNIX inter-instances socket interface */
/* $Id: af_unix.c,v 1.2 2002/03/17 13:54:13 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <links.h>

#include <config/default.h>
#include <document/session.h>
#include <lowlevel/af_unix.h>
#include <lowlevel/select.h>
#include <lowlevel/terminal.h>
#include <util/error.h>

#ifdef DONT_USE_AF_UNIX

int bind_to_af_unix()
{
	return -1;
}

void af_unix_close()
{
}

#else

/* Rest of the file is #else... */

#ifdef USE_AF_UNIX
#include <sys/un.h>
#endif

void af_unix_connection(void *);

/* Accepted socket */
struct sockaddr *s_unix_accept = NULL;

/* Listening socket */
struct sockaddr *s_unix = NULL;
int s_unix_l;
int s_unix_fd = -1;

#ifdef USE_AF_UNIX

int get_address()
{
	struct sockaddr_un *addr;
	unsigned char *path;
	
	if (!links_home) return -1;
	
	path = stracpy(links_home);
	
	addr = mem_alloc(sizeof(struct sockaddr_un) + strlen(path) + 1);
	if (!addr) {
		mem_free(path);
		return -1;
	}
	
	s_unix_accept = mem_alloc(sizeof(struct sockaddr_un) + strlen(path) + 1);
	if (!s_unix_accept) {
		mem_free(addr);
		mem_free(path);
		return -1;
	}
	
	addr->sun_family = AF_UNIX;
	
	add_to_strn(&path, LINKS_SOCK_NAME);
	strcpy(addr->sun_path, path);
	mem_free(path);
	
	s_unix = (struct sockaddr *) addr;
	s_unix_l = (char *) &addr->sun_path - (char *) addr + strlen(addr->sun_path);
	
	return AF_UNIX;
}

void unlink_unix()
{
	unlink(((struct sockaddr_un *) s_unix)->sun_path);
#if 0
	if (unlink(((struct sockaddr_un *) s_unix)->sun_path)) {
		perror("unlink");
		debug("unlink: %s", ((struct sockaddr_un *)s_unix)->sun_path);
	}
#endif
}

#else

int get_address()
{
	struct sockaddr_in *sin;
	
	sin = mem_alloc(sizeof(struct sockaddr_in));
	if (!sin) return -1;
	
	s_unix_accept = mem_alloc(sizeof(struct sockaddr_in));
	if (!s_unix_accept) {
		mem_free(sin);
		return -1;
	}
	
	sin->sin_family = AF_INET;
	sin->sin_port = LINKS_PORT;
	sin->sin_addr.s_addr = htonl(0x7f000001); /* localhost */
	
	s_unix = (struct sockaddr *) sin;
	s_unix_l = sizeof(struct sockaddr_in);
	
	return AF_INET;
}

void unlink_unix()
{
}

#endif

int bind_to_af_unix()
{
	int unlinked = 0;
	int reuse_addr = 1;
	int attempts = 0;
	int af;
	
	af = get_address();
	if (af == -1) return -1;
	
	s_unix_fd = socket(af, SOCK_STREAM, 0);
	if (s_unix_fd == -1) return -1;
	
again:
	
#if defined(SOL_SOCKET) && defined(SO_REUSEADDR)
	setsockopt(s_unix_fd, SOL_SOCKET, SO_REUSEADDR, (void *) &reuse_addr, sizeof(int));
#endif
	
	if (bind(s_unix_fd, s_unix, s_unix_l) < 0) {
#if 0
		perror("");
		debug("bind: %d", errno);
#endif
		close(s_unix_fd);
		
		/* Try to connect there then */
		
		s_unix_fd = socket(af, SOCK_STREAM, 0);
		if (s_unix_fd == -1) return -1;
		
#if defined(SOL_SOCKET) && defined(SO_REUSEADDR)
		setsockopt(s_unix_fd, SOL_SOCKET, SO_REUSEADDR, (void *)&reuse_addr, sizeof(int));
#endif
		
		if (connect(s_unix_fd, s_unix, s_unix_l) < 0) {
#if 0
			perror("");
			debug("connect: %d", errno);
#endif
			
			if (++attempts < MAX_BIND_TRIES) {
				struct timeval tv = { 0, 100000 };
				fd_set dummy;
				
				/* Sleep for a second */
				FD_ZERO(&dummy);
				select(0, &dummy, &dummy, &dummy, &tv);
				close(s_unix_fd);
				
				goto again;
			}
			
			close(s_unix_fd); s_unix_fd = -1;
			
			if (!unlinked) {
				unlink_unix();
				unlinked = 1;
				
				goto again;
			}
			
			mem_free(s_unix); s_unix = NULL;
			
			return -1;
		}
		
		mem_free(s_unix); s_unix = NULL;
		
		return s_unix_fd;
	}
	
	if (listen(s_unix_fd, 100)) {
		error("ERROR: listen failed: %d", errno);
		
		mem_free(s_unix); s_unix = NULL;
		close(s_unix_fd); s_unix_fd = -1;
		
		return -1;
	}
	
	set_handlers(s_unix_fd, af_unix_connection, NULL, NULL, NULL);
	
	return -1;
}

void af_unix_connection(void *dummy)
{
	int l = s_unix_l;
	int ns;
	
	ns = accept(s_unix_fd, (struct sockaddr *) s_unix_accept, &l);
	
	init_term(ns, ns, win_func);
}

void af_unix_close()
{
	if (s_unix_fd != -1)
		close(s_unix_fd);
	
	if (s_unix) {
		unlink_unix();
		mem_free(s_unix); s_unix = NULL;
	}
	
	if (s_unix_accept) {
		mem_free(s_unix_accept); s_unix_accept = NULL;
	}
}

#endif
