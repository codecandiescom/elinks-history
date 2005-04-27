/* $Id: hardio.h,v 1.2 2005/04/27 15:15:01 jonas Exp $ */

#ifndef EL__TERMINAL_HARDIO_H
#define EL__TERMINAL_HARDIO_H

ssize_t hard_write(int, unsigned char *, int);
ssize_t hard_read(int, unsigned char *, int);

#endif
