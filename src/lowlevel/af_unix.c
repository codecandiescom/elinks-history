/* AF_UNIX inter-instances socket interface */
/* $Id: af_unix.c,v 1.59 2003/07/06 23:17:34 pasky Exp $ */

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

#include "elinks.h"

#include "intl/gettext/libintl.h"
#include "lowlevel/af_unix.h"
#include "lowlevel/home.h"
#include "lowlevel/select.h"
#include "terminal/terminal.h"
#include "sched/session.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"

/* Testing purpose. Do not remove. */
#if 0
#undef DONT_USE_AF_UNIX
#undef USE_AF_UNIX
#endif

#ifdef DONT_USE_AF_UNIX

/*** No internal communication. ***/

int
af_unix_open(void)
{
	return -1;
}

void
af_unix_close(void)
{
}


#else /* DONT_USE_AF_UNIX */

/* Common to both AF_UNIX and AF_INET stuff. */
struct socket_info {
	struct sockaddr *addr;
	int size;
	int fd;
};

/* Accepted socket info */
static struct socket_info s_info_accept;

/* Listening socket info */
static struct socket_info s_info_listen;

/* Connect socket info */
static struct socket_info s_info_connect;

/* Type of address requested (for get_address()) */
enum addr_type {
	ADDR_IP_CLIENT,
	ADDR_IP_SERVER,
	ADDR_ANY_SERVER,
};


#ifdef USE_AF_UNIX

/*** Unix file socket for internal communication. ***/

#include <sys/un.h>

/* Compute socket file path and allocate space for it.
 * It returns 0 on error (in this case, there's no need
 * to free anything).
 * It returns 1 on success. */
static int
get_sun_path(unsigned char **sun_path, int *sun_path_len)
{
	assert(sun_path && sun_path_len);
	if_assert_failed return 0;

	if (!elinks_home) return 0;

	*sun_path = init_str();
	if (!*sun_path) return 0;

	*sun_path_len = 0;

	add_to_str(sun_path, sun_path_len, elinks_home);
	add_to_str(sun_path, sun_path_len, ELINKS_SOCK_NAME);
	add_num_to_str(sun_path, sun_path_len,
		       get_opt_int_tree(&cmdline_options, "session-ring"));

	return 1;
}

/* @type is ignored here => always local. */
static int
get_address(struct socket_info *info, enum addr_type type)
{
	struct sockaddr_un *addr = NULL;
	int sun_path_freespace;
	unsigned char *path;
	int pathl;

	assert(info);
	if_assert_failed return -1;

	if (!get_sun_path(&path, &pathl)) return -1;

	/* Linux defines that as:
	 * #define UNIX_PATH_MAX   108
	 * struct sockaddr_un {
	 *	sa_family_t sun_family;
	 *	char sun_path[UNIX_PATH_MAX];
	 * };
	 *
	 * UNIX_PATH_MAX is not defined on all systems, so we'll use sizeof().
	 */

	/* Following code may need to be changed if at some time the
	 * struct sockaddr_un mess is fixed. For now, i tried to make it
	 * sure and portable.
	 *
	 * Extract from glibc documentation:
	 * char sun_path[108]
	 * This is the file name to use.
	 * Incomplete: Why is 108 a magic number?
	 * RMS suggests making this a zero-length array and tweaking the example
	 * following to use alloca to allocate an appropriate amount of storage
	 * based on the length of the filename.
	 *
	 * But at this day (2003/06/18) it seems there's no implementation of such
	 * thing.
	 * If it was the case, then following code will always generate an error.
	 * --Zas
	 */

	sun_path_freespace = sizeof(addr->sun_path) - (pathl + 1);
	if (sun_path_freespace < 0) {
		internal("Socket path name '%s' is too long: %d >= %d",
			 path, pathl, sizeof(addr->sun_path));
		goto free_and_error;
	}

	addr = mem_calloc(1, sizeof(struct sockaddr_un));
	if (!addr) goto free_and_error;

	memcpy(addr->sun_path, path, pathl); /* ending '\0' is done by calloc() */
	mem_free(path);

	addr->sun_family = AF_UNIX;

	info->addr = (struct sockaddr *) addr;
	/* The size of the address is the offset of the start of the filename,
	 * plus its length, plus one for the terminating null byte (well, this
	 * last byte may or not be needed it depends of....).
	 * Alternatively we can use SUN_LEN() macro but this one is not always
	 * defined nor always defined in the same way. --Zas */
	info->size = sizeof(struct sockaddr_un) - sun_path_freespace;

	return AF_UNIX;

free_and_error:
	mem_free(path);
	if (addr) mem_free(addr);

	return -1;
}

static int
alloc_address(struct socket_info *info)
{
	struct sockaddr_un *sa;

	assert(info);
	if_assert_failed return 0;

	/* calloc() is safer there. */
	sa = mem_calloc(1, sizeof(struct sockaddr_un));
	if (!sa) return 0;

	info->addr = (struct sockaddr *) sa;
	info->size = sizeof(struct sockaddr_un);

	return 1;
}

static void
unlink_unix(struct sockaddr *addr)
{
	assert(addr);
	if_assert_failed return;

	unlink(((struct sockaddr_un *) addr)->sun_path);
}

#define setsock_reuse_addr(fd)


#else /* USE_AF_UNIX */

/*** TCP socket for internal communication. ***/
/* FIXME: IPv6 support. */

#include <arpa/inet.h>

/* These may not be defined in netinet/in.h on some systems. */
#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK         ((struct in_addr) 0x7f000001)
#endif

#ifndef IPPORT_USERRESERVED
#define IPPORT_USERRESERVED	5000
#endif

/* @type is not used for now, and is ignored, it will
 * be used in remote mode feature. */
static int
get_address(struct socket_info *info, enum addr_type type)
{
	struct sockaddr_in *sin;
	unsigned short port;

	assert(info);
	if_assert_failed return -1;

	/* Each ring is bind to ELINKS_PORT + ring number. */
	port = ELINKS_PORT + get_opt_int_tree(&cmdline_options,
					      "session-ring");
	if (port < IPPORT_USERRESERVED || port > 65535)
		return -1; /* Just in case of... */

	sin = mem_calloc(1, sizeof(struct sockaddr_in));
	if (!sin) return -1;

	sin->sin_family = AF_INET;
	sin->sin_port = htons(port);
	sin->sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	info->addr = (struct sockaddr *) sin;
	info->size = sizeof(struct sockaddr_in);

	return AF_INET;
}

static int
alloc_address(struct socket_info *info)
{
	struct sockaddr_in *sa;

	assert(info);
	if_assert_failed return 0;

	/* calloc() is safer there. */
	sa = mem_calloc(1, sizeof(struct sockaddr_in));
	if (!sa) return 0;

	info->addr = (struct sockaddr *) sa;
	info->size = sizeof(struct sockaddr_in);

	return 1;
}

#if defined(SOL_SOCKET) && defined(SO_REUSEADDR)
static void
setsock_reuse_addr(int fd)
{
	int reuse_addr = 1;

	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
		   (void *)&reuse_addr, sizeof(int));
}
#else
#define setsock_reuse_addr(fd)
#endif

#define unlink_unix(s)

#endif /* USE_AF_UNIX */

/* Max. number of bind attempts. */
#define MAX_BIND_TRIES			3
/* Base delay in useconds between bind attempts.
 * We will sleep that time on first attempt, then
 * 2 * delay, then 3 * delay .... */
#define BIND_TRIES_DELAY		100000
/* Number of connections in listen backlog. */
#define LISTEN_BACKLOG			100

/* Max. number of connect attempts. */
#define MAX_CONNECT_TRIES		3
/* Base delay in useconds between connect attempts. */
#define CONNECT_TRIES_DELAY		50000


/* Called when we receive a connection on listening socket. */
static void
af_unix_connection(struct socket_info *info)
{
	int ns;
	int l = info->size;

	assert(info);
	if_assert_failed return;

	memset(info->addr, 0, l);
	ns = accept(info->fd, info->addr, &l);
	if (ns < 0) {
		error(gettext("accept() failed: %d (%s)"),
		      errno, (unsigned char *) strerror(errno));
		return;
	}

	init_term(ns, ns, tabwin_func);

	set_highpri();
}

/* usleep() is not portable, so we use this replacement.
 * TODO: move it to somewhere. */
void
elinks_usleep(unsigned long useconds)
{
	struct timeval delay;
	fd_set dummy;

	FD_ZERO(&dummy);

	delay.tv_sec = 0;
	delay.tv_usec = useconds;
	select(0, &dummy, &dummy, &dummy, &delay);
}

/* Listen on socket for internal ELinks communication.
 * Returns -1 on error
 * or listened file descriptor on success. */
static int
bind_to_af_unix(void)
{
	int attempts = 0;
	int af = get_address(&s_info_listen, ADDR_IP_SERVER);

	if (af == -1) goto free_and_error;

again:
	s_info_listen.fd = socket(af, SOCK_STREAM, 0);
	if (s_info_listen.fd == -1) {
		error(gettext("socket() failed: %d (%s)"),
		      errno, (unsigned char *) strerror(errno));
		goto free_and_error;
	}

	setsock_reuse_addr(s_info_listen.fd);

	if (bind(s_info_listen.fd, s_info_listen.addr, s_info_listen.size) < 0) {
		if (errno != EADDRINUSE)
			error(gettext("bind() failed: %d (%s)"),
			      errno, (unsigned char *) strerror(errno));

		if (++attempts < MAX_BIND_TRIES) {
			elinks_usleep(BIND_TRIES_DELAY * attempts);
			close(s_info_listen.fd);

			goto again;
		}

		goto free_and_error;
	}

	/* Listen and accept. */
	if (!alloc_address(&s_info_accept))
		goto free_and_error;

	s_info_accept.fd = s_info_listen.fd;

	if (listen(s_info_listen.fd, LISTEN_BACKLOG)) {
		error(gettext("listen() failed: %d (%s)"),
		      errno, (unsigned char *) strerror(errno));
		goto free_and_error;
	}

	set_handlers(s_info_listen.fd, (void (*)(void *)) af_unix_connection,
		     NULL, NULL, &s_info_accept);

	return s_info_listen.fd;

free_and_error:
	af_unix_close();

	return -1;
}

/* Connect to an listening socket for internal ELinks communication.
 * Returns -1 on error
 * or file descriptor on success. */
static int
connect_to_af_unix(void)
{
	int attempts = 0;
	int af = get_address(&s_info_connect, ADDR_IP_CLIENT);

	if (af == -1) goto free_and_error;

again:
	s_info_connect.fd = socket(af, SOCK_STREAM, 0);
	if (s_info_connect.fd == -1) {
		error(gettext("socket() failed: %d (%s)"),
		      errno, (unsigned char *) strerror(errno));
		goto free_and_error;
	}

#if 0
	/* Is this of any use on connect socket ?? */
	setsock_reuse_addr(s_info_connect.fd);
#endif
	if (connect(s_info_connect.fd, s_info_connect.addr,
		    s_info_connect.size) < 0) {
		if (errno != ECONNREFUSED && errno != ENOENT)
			error(gettext("connect() failed: %d (%s)"),
			      errno, (unsigned char *) strerror(errno));

		if (++attempts < MAX_CONNECT_TRIES) {
			elinks_usleep(CONNECT_TRIES_DELAY * attempts);
			close(s_info_connect.fd);

			goto again;
		}

		close(s_info_connect.fd); s_info_connect.fd = -1;

		goto free_and_error;
	}

	return s_info_connect.fd;

free_and_error:
	if (s_info_connect.addr) {
		mem_free(s_info_connect.addr);
		s_info_connect.addr = NULL;
	}

	return -1;
}


/* Free all allocated memory and close all descriptors if
 * needed. */
void
af_unix_close(void)
{
	/* We test for addr != NULL since
	 * if it was not allocated then fd is not
	 * initialized and we don't want to close
	 * fd 0 ;). --Zas */
	if (s_info_listen.addr) {
		if (s_info_listen.fd != -1) {
			close(s_info_listen.fd);
			s_info_listen.fd = -1;
		}

		unlink_unix(s_info_listen.addr);
		mem_free(s_info_listen.addr);
		s_info_listen.addr = NULL;
	}

	if (s_info_connect.addr) {
		if (s_info_connect.fd != -1) {
			close(s_info_connect.fd);
			s_info_connect.fd = -1;
		}

		mem_free(s_info_connect.addr);
		s_info_connect.addr = NULL;
	}

	if (s_info_accept.addr) {
		if (s_info_accept.fd != -1) {
			close(s_info_accept.fd);
			s_info_accept.fd = -1;
		}

		mem_free(s_info_accept.addr);
		s_info_accept.addr = NULL;
	}
}

/* Initialize sockets for internal ELinks communication.
 * If connect succeeds it returns file descriptor,
 * else it tries to bind and listen on a socket, and
 * return -1
 */
int
af_unix_open(void)
{
	int fd = connect_to_af_unix();

	if (fd != -1) return fd;

	bind_to_af_unix();
	return -1;
}


#undef MAX_BIND_TRIES
#undef BIND_TRIES_DELAY
#undef LISTEN_BACKLOG
#undef MAX_CONNECT_TRIES
#undef CONNECT_TRIES_DELAY

#endif /* DONT_USE_AF_UNIX */
