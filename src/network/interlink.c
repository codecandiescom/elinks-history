/* AF_UNIX inter-instances socket interface */
/* $Id: interlink.c,v 1.40 2003/06/18 18:58:43 zas Exp $ */

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

#if 0	/* Testing purpose. */
#undef DONT_USE_AF_UNIX
#undef USE_AF_UNIX
#endif

#ifdef DONT_USE_AF_UNIX

int
bind_to_af_unix(void)
{
	return -1;
}

void
af_unix_close(void)
{
}

#else

/* Rest of the file is #else... */

#ifdef USE_AF_UNIX
#include <sys/un.h>
#endif

/* FIXME: Separate client and server code. --Zas */

/* Accepted socket */
static struct sockaddr *s_unix_accept;

/* Listening socket */
static struct sockaddr *s_unix;
static int s_unix_l;
static int s_unix_fd = -1;

#ifdef USE_AF_UNIX

static int
get_address(void)
{
	struct sockaddr_un *addr = NULL;
	unsigned char *path;
	int pathl = 0;
	int sun_path_freespace;

	if (!elinks_home) return -1;

	path = init_str();
	if (!path) return -1;

	add_to_str(&path, &pathl, elinks_home);
	add_to_str(&path, &pathl, ELINKS_SOCK_NAME);
	add_num_to_str(&path, &pathl, get_opt_int_tree(&cmdline_options, "session-ring"));

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

	s_unix_accept = mem_alloc(sizeof(struct sockaddr_un));
	if (!s_unix_accept) goto free_and_error;

	memcpy(addr->sun_path, path, pathl); /* ending '\0' is done by calloc() */
	mem_free(path);

	addr->sun_family = AF_UNIX;

	s_unix = (struct sockaddr *) addr;
	/* The size of the address is the offset of the start of the filename,
	 * plus its length, plus one for the terminating null byte (well, this
	 * last byte may or not be needed it depends of....).
	 * Alternatively we can use SUN_LEN() macro but this one is not always
	 * defined nor always defined in the same way. --Zas */
	s_unix_l = sizeof(struct sockaddr_un) - sun_path_freespace;

	return AF_UNIX;

free_and_error:
	mem_free(path);
	if (addr) mem_free(addr);

	return -1;
}

static void
unlink_unix(void)
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

/* It may not be defined in netinet/in.h on some systems. */
#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK         ((unsigned long int) 0x7f000001)
#endif

/* FIXME: IPv6 support. */

static int
get_address(void)
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
	sin->sin_port = htons(ELINKS_PORT);
	sin->sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	s_unix = (struct sockaddr *) sin;
	s_unix_l = sizeof(struct sockaddr_in);

	return AF_INET;
}

#define unlink_unix()

#endif

static void
af_unix_connection(void *dummy)
{
	int ns;
	int l = s_unix_l;

	memset(s_unix_accept, 0, l);
	ns = accept(s_unix_fd, (struct sockaddr *) s_unix_accept, &l);
	if (ns < 0) {
		error(gettext("accept() failed: %d (%s)"),
		      errno, (unsigned char *) strerror(errno));
		return;
	}

	init_term(ns, ns, tabwin_func);

	set_highpri();
}


/* TODO: separate to a client function and a server function. */
int
bind_to_af_unix(void)
{
	int unlinked = 0;
	int reuse_addr = 1;
	int attempts = 0;
	int af;

	af = get_address();
	if (af == -1) return -1;

again:

	s_unix_fd = socket(af, SOCK_STREAM, 0);
	if (s_unix_fd == -1) {
		error(gettext("socket() failed: %d (%s)"),
		      errno, (unsigned char *) strerror(errno));
		return -1;
	}

#if defined(SOL_SOCKET) && defined(SO_REUSEADDR)
	setsockopt(s_unix_fd, SOL_SOCKET, SO_REUSEADDR, (void *) &reuse_addr, sizeof(int));
#endif

	if (bind(s_unix_fd, s_unix, s_unix_l) < 0) {
		if (errno != EADDRINUSE) /* This will change soon --Zas */
			error(gettext("bind() failed: %d (%s)"),
			      errno, (unsigned char *) strerror(errno));

		close(s_unix_fd);

		/* Try to connect there then */

		s_unix_fd = socket(af, SOCK_STREAM, 0);
		if (s_unix_fd == -1) {
			error(gettext("socket() failed: %d (%s)"),
			      errno, (unsigned char *) strerror(errno));
			return -1;
		}

#if defined(SOL_SOCKET) && defined(SO_REUSEADDR)
		setsockopt(s_unix_fd, SOL_SOCKET, SO_REUSEADDR, (void *)&reuse_addr, sizeof(int));
#endif

		if (connect(s_unix_fd, s_unix, s_unix_l) < 0) {
			if (errno != ECONNREFUSED)
				error(gettext("connect() failed: %d (%s)"),
				      errno, (unsigned char *) strerror(errno));

			if (++attempts < MAX_BIND_TRIES) {
				/* XXX: What's wrong with sleep(1) ?? --Zas */
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
		error(gettext("listen() failed: %d (%s)"),
		      errno, (unsigned char *) strerror(errno));

		mem_free(s_unix); s_unix = NULL;
		close(s_unix_fd); s_unix_fd = -1;

		return -1;
	}

	set_handlers(s_unix_fd, af_unix_connection, NULL, NULL, NULL);

	return -1;
}

void
af_unix_close(void)
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
