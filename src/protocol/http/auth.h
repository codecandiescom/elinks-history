/* $Id: auth.h,v 1.3 2003/04/24 08:23:40 zas Exp $ */

#ifndef EL__PROTOCOL_HTTP_AUTH_H
#define EL__PROTOCOL_HTTP_AUTH_H

struct http_auth_basic {
        LIST_HEAD(struct http_auth_basic);

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
