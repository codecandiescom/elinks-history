/* $Id: auth.h,v 1.2 2002/05/12 20:15:25 pasky Exp $ */

#ifndef EL__PROTOCOL_HTTP_AUTH_H
#define EL__PROTOCOL_HTTP_AUTH_H

struct http_auth_basic {
        struct http_auth_basic *next;
        struct http_auth_basic *prev;
        int blocked;
        int valid;
        unsigned char *url;
        int url_len;
        unsigned char *realm;
        unsigned char *uid;
        unsigned char *passwd;
};

unsigned char *find_auth(unsigned char *);
int add_auth_entry(unsigned char *, unsigned char *);
void del_auth_entry(struct http_auth_basic *);
void free_auth();

#endif
