/* $Id: generic.h,v 1.24 2005/02/05 05:26:40 jonas Exp $ */

/* This is... er, the OS-independent part of osdep/ ;-). */

#ifndef EL__OSDEP_GENERIC_H
#define EL__OSDEP_GENERIC_H

#ifdef HAVE_LIMITS_H
#include <limits.h> /* may contain PIPE_BUF definition on some systems */
#endif

#ifdef HAVE_SYS_SIGNAL_H
#include <sys/signal.h> /* may contain SA_RESTART */
#endif

#ifdef HAVE_STDDEF_H
#include <stddef.h> /* may contain offsetof() */
#endif

#ifndef INT_MAX
#ifdef MAXINT
#define INT_MAX MAXINT
#else
/* XXX: We could use util/types.h to determine something useful? --pasky */
#define INT_MAX 0x7fffffff
#endif
#endif

#ifndef LONG_MAX
#ifdef MAXLONG
#define LONG_MAX MAXLONG
#else
/* XXX: We could use util/types.h to determine something useful? --pasky */
#define LONG_MAX 0x7fffffff
#endif
#endif

#ifndef SA_RESTART
#define SA_RESTART	0
#endif

#ifndef PIPE_BUF
#define PIPE_BUF	512 /* POSIX says that. -- Mikulas */
#endif

/* Attempt to workaround the EINTR mess. */
#if defined(EINTR) && !defined(CONFIG_WIN32)

#ifdef TEMP_FAILURE_RETRY	/* GNU libc */
#define safe_read(fd, buf, count) TEMP_FAILURE_RETRY(read(fd, buf, count))
#define safe_write(fd, buf, count) TEMP_FAILURE_RETRY(write(fd, buf, count))
#else /* TEMP_FAILURE_RETRY */

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

static inline ssize_t
safe_read(int fd, void *buf, size_t count) {
	do {
		int r = read(fd, buf, count);

		if (r == -1 && errno == EINTR) continue;
		return r;
	} while (1);
}

static inline ssize_t
safe_write(int fd, const void *buf, size_t count) {
	do {
		int w = write(fd, buf, count);

		if (w == -1 && errno == EINTR) continue;
		return w;
	} while (1);
}
#endif /* TEMP_FAILURE_RETRY */

#else /* EINTR && !CONFIG_WIN32 */

#define safe_read(fd, buf, count) read(fd, buf, count)
#define safe_write(fd, buf, count) write(fd, buf, count)

#endif /* EINTR && !CONFIG_WIN32 */


/* Compiler area: */

/* Some compilers, like SunOS4 cc, don't have offsetof in <stddef.h>.  */
#ifndef offsetof
#define offsetof(type, ident) ((size_t) &(((type *) 0)->ident))
#endif

/* Alignment of types.  */
#define alignof(TYPE) \
    ((int) &((struct { unsigned char dummy1; TYPE dummy2; } *) 0)->dummy2)

/* Using this macro to copy structs is both faster and safer than
 * memcpy(destination, source, sizeof(source)). Please, use this macro instead
 * of memcpy(). */
#define copy_struct(destination, source) \
	do { (*(destination) = *(source)); } while (0)

#endif
