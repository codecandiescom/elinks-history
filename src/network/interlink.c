/* AF_UNIX inter-instances socket interface */
/* $Id: interlink.c,v 1.19 2002/12/02 14:25:58 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* struct timeval */
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif

/* Blame BSD for position of this includes. */
#include <netinet/in.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <sys/socket.h> /* OS/2 needs this after sys/types.h */

#include "links.h"

#include "document/session.h"
#include "lowlevel/af_unix.h"
#include "lowlevel/home.h"
#include "lowlevel/select.h"
#include "lowlevel/terminal.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"

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
	unsigned char *path = init_str();
	int pathl = 0;

	if (!elinks_home || !path) return -1;

	add_to_str(&path, &pathl, elinks_home);

	addr = mem_calloc(1, sizeof(struct sockaddr_un) + pathl + 1);
	if (!addr) {
		mem_free(path);
		return -1;
	}

	s_unix_accept = mem_alloc(sizeof(struct sockaddr_un) + pathl + 1);
	if (!s_unix_accept) {
		mem_free(addr);
		mem_free(path);
		return -1;
	}

	addr->sun_family = AF_UNIX;

	add_to_str(&path, &pathl, LINKS_SOCK_NAME);
	add_num_to_str(&path, &pathl, get_opt_int_tree(cmdline_options, "session-ring"));
	strcpy(addr->sun_path, path);
	mem_free(path);

	s_unix = (struct sockaddr *) addr;
	s_unix_l = (char *) &addr->sun_path - (char *) addr + strlen(addr->sun_path) + 1;

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

	sin = mem_calloc(1, sizeof(struct sockaddr_in));
	if (!sin) return -1;

	s_unix_accept = mem_alloc(sizeof(struct sockaddr_in));
	if (!s_unix_accept) {
		mem_free(sin);
		return -1;
	}

	sin->sin_family = AF_INET;
	sin->sin_port = htons(LINKS_PORT);
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

	if (!s_unix) return -1;
	
	af = get_address();
	if (af == -1) return -1;

again:

	s_unix_fd = socket(af, SOCK_STREAM, 0);
	if (s_unix_fd == -1) return -1;

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

	memset(s_unix_accept, 0, l);
	ns = accept(s_unix_fd, (struct sockaddr *) s_unix_accept, &l);

	init_term(ns, ns, win_func);

	set_highpri();
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
