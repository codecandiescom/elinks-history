/* $Id: socket.h,v 1.4 2002/03/18 15:14:54 pasky Exp $ */

#ifndef EL__CONNECT_H
#define EL__CONNECT_H

#include <lowlevel/sched.h>

struct read_buffer {
	int sock;
	int len;
	int close;
	void (*done)(struct connection *, struct read_buffer *);
	unsigned char data[1];
};

void close_socket(int *);
void make_connection(struct connection *, int, int *, void (*)(struct connection *));
void dns_found(/* struct connection */ void *, int);
int get_pasv_socket(struct connection *, int, unsigned char *);
void write_to_socket(struct connection *, int, unsigned char *, int, void (*)(struct connection *));
struct read_buffer *alloc_read_buffer(struct connection *c);
void read_from_socket(struct connection *, int, struct read_buffer *, void (*)(struct connection *, struct read_buffer *));
void kill_buffer_data(struct read_buffer *, int);

#endif
