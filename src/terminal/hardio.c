/* Low-level terminal-suitable I/O routines */
/* $Id: hardio.c,v 1.2 2003/05/05 14:19:08 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "terminal/hardio.h"

int
hard_write(int fd, unsigned char *p, int l)
{
	int w = 1;
	int t = 0;

	while (l > 0 && w) {
		w = write(fd, &p[t], l);
		if (w < 0) {
			if (errno == EINTR) continue;
			return -1;
		}
		t += w;
		l -= w;
	}

	return t;
}

int
hard_read(int fd, unsigned char *p, int l)
{
	int r = 1;
	int t = 0;

	while (l > 0 && r) {
		r = read(fd, &p[t], l);
		if (r < 0) {
			if (errno == EINTR) continue;
			return -1;
		}
#if 0 /* for debugging purpose */
		{
		 int ww;

		 for (ww = 0; ww < r; ww++)
			 fprintf(stderr, " %02x", (int) p[ww]);
		 fflush(stderr);
		}
#endif
	 	t += r;
		l -= r;
	}

	return t;
}
