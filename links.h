/* Global include with common functions and definitions for elinks */
/* $Id: links.h,v 1.59 2002/03/16 17:44:40 pasky Exp $ */

#ifndef EL__LINKS_H
#define EL__LINKS_H

#ifndef __EXTENSION__
#define __EXTENSION__ /* Helper for SunOS */
#endif

/* XXX: REMOVE-ME */
struct session;

/* Includes for internal functions */

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#include <stdlib.h>
#include <string.h>

/* Temporary global includes, which we unfortunately still need. */

/* sched.c, https.c */
#ifdef HAVE_SSL
#include <openssl/ssl.h>
#endif

/* lua.c */
#ifdef HAVE_LUA
#include <lua.h>
#include <lualib.h>
#endif

#include "os_dep.h"

#if 0

/* These are residuum from old links - dunno what they were meant for? */

#ifdef HAVE_SYS_RESOURCE_H
#  include <sys/resource.h>
#endif

#ifdef HAVE_NETINET_IN_SYSTM_H
#  include <netinet/in_systm.h>
#else
#  ifdef HAVE_NETINET_IN_SYSTEM_H
#    include <netinet/in_system.h>
#  endif
#endif

#endif

#include "os_depx.h"

#include "setup.h"

/* Includes for internal functions */

#include "error.h"


/* This is for our pointer stuff */

#define DUMMY ((void *)-1L)


/* Misc. types definition */

typedef unsigned tcount;


/* Memory managment */

#if !defined(LEAK_DEBUG) && defined(LEAK_DEBUG_LIST)
#error "You defined LEAK_DEBUG_LIST, but not LEAK_DEBUG!"
#endif

#ifdef LEAK_DEBUG

/* XXX: No, I don't like this too. Probably we should have the leak debugger
 * in separate file. --pasky */

#include "error.h"

#define mem_alloc(x) debug_mem_alloc(__FILE__, __LINE__, x)
#define mem_free(x) debug_mem_free(__FILE__, __LINE__, x)
#define mem_realloc(x, y) debug_mem_realloc(__FILE__, __LINE__, x, y)

#else

static inline void *mem_alloc(size_t size)
{
	void *p;
	if (!size) return DUMMY;
	if (!(p = malloc(size))) {
		error("ERROR: out of memory (malloc returned NULL)\n");
		return NULL;
	}
	return p;
}

static inline void mem_free(void *p)
{
	if (p == DUMMY) return;
	if (!p) {
		internal("mem_free(NULL)");
		return;
	}
	free(p);
}

static inline void *mem_realloc(void *p, size_t size)
{
	if (p == DUMMY) return mem_alloc(size);
	if (!p) {
		internal("mem_realloc(NULL, %d)", size);
		return NULL;
	}
	if (!size) {
		mem_free(p);
		return DUMMY;
	}
	if (!(p = realloc(p, size))) {
		error("ERROR: out of memory (realloc returned NULL)\n");
		return NULL;
	}
	return p;
}

#endif

#if !(defined(LEAK_DEBUG) && defined(LEAK_DEBUG_LIST))

static inline unsigned char *memacpy(const unsigned char *src, int len)
{
	unsigned char *m;
	if ((m = mem_alloc(len + 1))) {
		memcpy(m, src, len);
		m[len] = 0;
	}
	return m;
}

static inline unsigned char *stracpy(const unsigned char *src)
{
	return src ? memacpy(src, src != DUMMY ? strlen(src) : 0) : NULL;
}

#else

static inline unsigned char *debug_memacpy(unsigned char *f, int l, unsigned char *src, int len)
{
	unsigned char *m;
	if ((m = debug_mem_alloc(f, l, len + 1))) {
		memcpy(m, src, len);
		m[len] = 0;
	}
	return m;
}

#define memacpy(s, l) debug_memacpy(__FILE__, __LINE__, s, l)

static inline unsigned char *debug_stracpy(unsigned char *f, int l, unsigned char *src)
{
	return src ? debug_memacpy(f, l, src, src != DUMMY ? strlen(src) : 0) : NULL;
}

#define stracpy(s) debug_stracpy(__FILE__, __LINE__, s)

#endif


/* Inline utility functions */

static inline unsigned char upcase(unsigned char a)
{
	if (a>='a' && a<='z') a -= 0x20;
	return a;
}

static inline int xstrcmp(unsigned char *s1, unsigned char *s2)
{
        if (!s1 && !s2) return 0;
        if (!s1) return -1;
        if (!s2) return 1;
        return strcmp(s1, s2);
}

static inline int cmpbeg(unsigned char *str, unsigned char *b)
{
	while (*str && upcase(*str) == upcase(*b)) str++, b++;
	return !!*b;
}

static inline int snprint(unsigned char *s, int n, unsigned num)
{
	int q = 1;
	while (q <= num / 10) q *= 10;
	while (n-- > 1 && q) *(s++) = num / q + '0', num %= q, q /= 10;
	*s = 0;
	return !!q;
}

static inline int snzprint(unsigned char *s, int n, int num)
{
	if (n > 1 && num < 0) *(s++) = '-', num = -num, n--;
	return snprint(s, n, num);
}

static inline void add_to_strn(unsigned char **s, unsigned char *a)
{
	unsigned char *p;
	if (!(p = mem_realloc(*s, strlen(*s) + strlen(a) + 1))) return;
	strcat(p, a);
	*s = p;
}

#define ALLOC_GR	0x100		/* must be power of 2 */

#define init_str() init_str_x(__FILE__, __LINE__)

static inline unsigned char *init_str_x(unsigned char *file, int line)
{
	unsigned char *p;
	if ((p = debug_mem_alloc(file, line, ALLOC_GR))) *p = 0;
	return p;
}

static inline void add_to_str(unsigned char **s, int *l, unsigned char *a)
{
	unsigned char *p;
	int ll = strlen(a);
	if ((*l & ~(ALLOC_GR - 1)) != ((*l + ll) & ~(ALLOC_GR - 1)) &&
	   (!(p = mem_realloc(*s, (*l + ll + ALLOC_GR) & ~(ALLOC_GR - 1))) ||
	   !(*s = p))) return;
	strcpy(*s + *l, a); *l += ll;
}

static inline void add_bytes_to_str(unsigned char **s, int *l, unsigned char *a, int ll)
{
	unsigned char *p;
	if ((*l & ~(ALLOC_GR - 1)) != ((*l + ll) & ~(ALLOC_GR - 1)) &&
	   (!(p = mem_realloc(*s, (*l + ll + ALLOC_GR) & ~(ALLOC_GR - 1))) ||
	   !(*s = p))) return;
	memcpy(*s + *l, a, ll); (*s)[*l += ll] = 0;
}

static inline void add_chr_to_str(unsigned char **s, int *l, unsigned char a)
{
	unsigned char *p;
	if ((*l & (ALLOC_GR - 1)) == ALLOC_GR - 1 &&
	   (!(p = mem_realloc(*s, (*l + 1 + ALLOC_GR) & ~(ALLOC_GR - 1))) ||
	   !(*s = p))) return;
	*(*s + *l) = a; *(*s + ++(*l)) = 0;
}

static inline void add_num_to_str(unsigned char **s, int *l, int n)
{
	unsigned char a[64];
	/*sprintf(a, "%d", n);*/
	snzprint(a, 64, n);
	add_to_str(s, l, a);
}

static inline void add_knum_to_str(unsigned char **s, int *l, int n)
{
	unsigned char a[13];
	if (n && n / (1024 * 1024) * (1024 * 1024) == n) snzprint(a, 12, n / (1024 * 1024)), strcat(a, "M");
	else if (n && n / 1024 * 1024 == n) snzprint(a, 12, n / 1024), strcat(a, "k");
	else snzprint(a, 13, n);
	add_to_str(s, l, a);
}

static inline long strtolx(unsigned char *c, unsigned char **end)
{
	long l = strtol(c, (char **)end, 10);
	if (!*end) return l;
	if (upcase(**end) == 'K') {
		(*end)++;
		if (l < -MAXINT / 1024) return -MAXINT;
		if (l > MAXINT / 1024) return MAXINT;
		return l * 1024;
	}
	if (upcase(**end) == 'M') {
		(*end)++;
		if (l < -MAXINT / (1024 * 1024)) return -MAXINT;
		if (l > MAXINT / (1024 * 1024)) return MAXINT;
		return l * (1024 * 1024);
	}
	return l;
}

static inline unsigned char *copy_string(unsigned char **dst, unsigned char *src)
{
	if ((*dst = src) && (*dst = mem_alloc(strlen(src) + 1))) strcpy(*dst, src);
	return *dst;
}

/* Copies at most dst_size chars into dst. Ensures null termination of dst. */
static inline unsigned char *safe_strncpy(unsigned char *dst, const unsigned char *src, size_t dst_size) {
#if 0
	size_t to_copy;

	to_copy = strlen(src);

	/* Ensure that the url size is not greater than str_size */
	if (dst_size < to_copy)
		to_copy = dst_size - 1;

	strncpy(dst, src, to_copy);

	/* Ensure null termination */
	dst[to_copy] = '\0';
	
	return dst;
#endif

	strncpy(dst, src, dst_size);
	dst[dst_size - 1] = 0;
	
	return dst;
}


struct list_head {
	void *next;
	void *prev;
};

#ifndef HAVE_TYPEOF

struct xlist_head {
	struct xlist_head *next;
	struct xlist_head *prev;
};

#endif

#define init_list(x) {(x).next=&(x); (x).prev=&(x);}
#define list_empty(x) ((x).next == &(x))
#define del_from_list(x) {((struct list_head *)(x)->next)->prev=(x)->prev; ((struct list_head *)(x)->prev)->next=(x)->next;}
/*#define add_to_list(l,x) {(x)->next=(l).next; (x)->prev=(typeof(x))&(l); (l).next=(x); if ((l).prev==&(l)) (l).prev=(x);}*/
#define add_at_pos(p,x) do {(x)->next=(p)->next; (x)->prev=(p); (p)->next=(x); (x)->next->prev=(x);} while(0)

#ifdef HAVE_TYPEOF
#define add_to_list(l,x) add_at_pos((typeof(x))&(l),(x))
#define foreach(e,l) for ((e)=(l).next; (e)!=(typeof(e))&(l); (e)=(e)->next)
#define foreachback(e,l) for ((e)=(l).prev; (e)!=(typeof(e))&(l); (e)=(e)->prev)
#else
#define add_to_list(l,x) add_at_pos((struct xlist_head *)&(l),(struct xlist_head *)(x))
#define foreach(e,l) for ((e)=(l).next; (e)!=(void *)&(l); (e)=(e)->next)
#define foreachback(e,l) for ((e)=(l).prev; (e)!=(void *)&(l); (e)=(e)->prev)
#endif
#define free_list(l) {while ((l).next != &(l)) {struct list_head *a=(l).next; del_from_list(a); mem_free(a); }}

#define WHITECHAR(x) ((x) == 9 || (x) == 10 || (x) == 12 || (x) == 13 || (x) == ' ')
#define U(x) ((x) == '"' || (x) == '\'')

static inline int isA(unsigned char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
	        c == '_' || c == '-';
}

static inline int casecmp(unsigned char *c1, unsigned char *c2, int len)
{
	int i;
	for (i = 0; i < len; i++) if (upcase(c1[i]) != upcase(c2[i])) return 1;
	return 0;
}

#ifndef HAVE_STRCASESTR
/* Stub for strcasestr(), GNU extension */
static inline unsigned char *strcasestr(unsigned char *haystack, unsigned char *needle) {
	size_t haystack_length = strlen(haystack);
	size_t needle_length = strlen(needle);
	int i;

	if (haystack_length < needle_length)
		return NULL;

	for (i = haystack_length - needle_length + 1; i; i--) {
		if (!casecmp(haystack, needle, needle_length))
			return haystack;
		haystack++;
	}

	return NULL;
}
#endif

/* TODO: Move to.. somewhere :). */

#define CI_BYTES	1
#define CI_FILES	2
#define CI_LOCKED	3
#define CI_LOADING	4
#define CI_TIMERS	5
#define CI_TRANSFER	6
#define CI_CONNECTING	7
#define CI_KEEP		8
#define CI_LIST		9

/* XXX XXX REMOVE ME */

typedef long ttime;



/* sched.c */

#define PRI_MAIN	0
#define PRI_DOWNLOAD	0
#define PRI_FRAME	1
#define PRI_NEED_IMG	2
#define PRI_IMG		3
#define PRI_PRELOAD	4
#define PRI_CANCEL	5
#define N_PRI		6

struct remaining_info {
	int valid;
	int size, loaded, last_loaded, cur_loaded;
	int pos;
	ttime elapsed;
	ttime last_time;
	ttime dis_b;
	int data_in_secs[CURRENT_SPD_SEC];
	int timer;
};

struct connection {
	struct connection *next;
	struct connection *prev;
	tcount count;
	unsigned char *url;
	unsigned char *prev_url;
	int running;
	int state;
	int prev_error;
	int from;
	int pri[N_PRI];
	int no_cache;
	int sock1;
	int sock2;
	void *dnsquery;
	void *conn_info;
	int tries;
	struct list_head statuss;
	void *info;
	void *buffer;
	void (*conn_func)(void *);
	struct cache_entry *cache;
	int received;
	int est_length;
	int unrestartable;
	struct remaining_info prg;
	int timer;
	int detached;
#ifdef HAVE_SSL
	SSL *ssl;
	int no_tsl;
#endif
};

static inline int getpri(struct connection *c)
{
	int i;
	for (i = 0; i < N_PRI; i++) if (c->pri[i]) return i;
	internal("connection has no owner");
	return N_PRI;
}

#define NC_ALWAYS_CACHE	0
#define NC_CACHE	1
#define NC_IF_MOD	2
#define NC_RELOAD	3
#define NC_PR_NO_CACHE	4

#define S_WAIT		0
#define S_DNS		1
#define S_CONN		2
#define S_SSL_NEG	3
#define S_SENT		4
#define S_GETH		5
#define S_PROC		6
#define S_TRANS		7
#define S_QUESTIONS	8

#define S_WAIT_REDIR		-999
#define S_OK			-1000
#define S_INTERRUPTED		-1001
#define S_EXCEPT		-1002
#define S_INTERNAL		-1003
#define S_OUT_OF_MEM		-1004
#define S_NO_DNS		-1005
#define S_CANT_WRITE		-1006
#define S_CANT_READ		-1007
#define S_MODIFIED		-1008
#define S_BAD_URL		-1009
#define S_TIMEOUT		-1010
#define S_RESTART		-1011
#define S_STATE			-1012

#define S_HTTP_ERROR		-1100
#define S_HTTP_100		-1101
#define S_HTTP_204		-1102

#define S_FILE_TYPE		-1200
#define S_FILE_ERROR		-1201

#define S_FTP_ERROR		-1300
#define S_FTP_UNAVAIL		-1301
#define S_FTP_LOGIN		-1302
#define S_FTP_PORT		-1303
#define S_FTP_NO_FILE		-1304
#define S_FTP_FILE_ERROR	-1305

#define S_SSL_ERROR		-1400
#define S_NO_SSL		-1401

extern struct s_msg_dsc {
	int n;
	unsigned char *msg;
} msg_dsc[];

struct status {
	struct status *next;
	struct status *prev;
	struct connection *c;
	struct cache_entry *ce;
	int state;
	int prev_error;
	int pri;
	void (*end)(struct status *, void *);
	void *data;
	struct remaining_info *prg;
};

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

void check_queue();
long connect_info(int);
void send_connection_info(struct connection *c);
void setcstate(struct connection *c, int);
int get_keepalive_socket(struct connection *c);
void add_keepalive_socket(struct connection *c, ttime);
void run_connection(struct connection *c);
void retry_connection(struct connection *c);
void abort_connection(struct connection *c);
void end_connection(struct connection *c);
int load_url(unsigned char *, unsigned char *, struct status *, int, int);
void change_connection(struct status *, struct status *, int);
void detach_connection(struct status *, int);
void abort_all_connections();
void abort_background_connections();
int is_entry_used(struct cache_entry *);
void connection_timeout(struct connection *);
void set_timeout(struct connection *);
void add_blacklist_entry(unsigned char *, int);
void del_blacklist_entry(unsigned char *, int);
int get_blacklist_flags(unsigned char *);
void free_blacklist();
void update_noproxy();
void free_noproxy();
unsigned char *find_auth(unsigned char *);
int add_auth_entry(unsigned char *, unsigned char *);
void del_auth_entry(struct http_auth_basic *);
void free_auth();

#define BL_HTTP10	1
#define BL_NO_CHARSET	2

/* kbd.c */

#define BM_BUTT		3
#define B_LEFT		0
#define B_MIDDLE	1
#define B_RIGHT		2
#define BM_ACT		12
#define B_DOWN		0
#define B_UP		4
#define B_DRAG		8

#define KBD_ENTER	0x100
#define KBD_BS		0x101
#define KBD_TAB		0x102
#define KBD_ESC		0x103
#define KBD_LEFT	0x104
#define KBD_RIGHT	0x105
#define KBD_UP		0x106
#define KBD_DOWN	0x107
#define KBD_INS		0x108
#define KBD_DEL		0x109
#define KBD_HOME	0x10a
#define KBD_END		0x10b
#define KBD_PAGE_UP	0x10c
#define KBD_PAGE_DOWN	0x10d

#define KBD_F1		0x120
#define KBD_F2		0x121
#define KBD_F3		0x122
#define KBD_F4		0x123
#define KBD_F5		0x124
#define KBD_F6		0x125
#define KBD_F7		0x126
#define KBD_F8		0x127
#define KBD_F9		0x128
#define KBD_F10		0x129
#define KBD_F11		0x12a
#define KBD_F12		0x12b

#define KBD_CTRL_C	0x200

#define KBD_SHIFT	1
#define KBD_CTRL	2
#define KBD_ALT		4

void handle_trm(int, int, int, int, int, void *, int);
void free_all_itrms();
void resize_terminal();
void dispatch_special(unsigned char *);
void kbd_ctrl_c();
int is_blocked();

/* terminal.c */

typedef unsigned short chr;

struct event {
	long ev;
	long x;
	long y;
	long b;
};

#define EV_INIT		0
#define EV_KBD		1
#define EV_MOUSE	2
#define EV_REDRAW	3
#define EV_RESIZE	4
#define EV_ABORT	5

struct window {
	struct window *next;
	struct window *prev;
	void (*handler)(struct window *, struct event *, int fwd);
	void *data;
	int xp, yp;
	struct terminal *term;
};

#define MAX_TERM_LEN	16	/* this must be multiple of 8! (alignment problems) */

#define MAX_CWD_LEN	8192	/* this must be multiple of 8! (alignment problems) */	

#define ENV_XWIN	1
#define ENV_SCREEN	2
#define ENV_OS2VIO	4
#define ENV_BE		8
#define ENV_TWIN	16

struct terminal {
	struct terminal *next;
	struct terminal *prev;
	int master;
	int fdin;
	int fdout;
	int x;
	int y;
	int environment;
	unsigned char term[MAX_TERM_LEN];
	unsigned char cwd[MAX_CWD_LEN];
	unsigned *screen;
	unsigned *last_screen;
	struct term_spec *spec;
	int cx;
	int cy;
	int lcx;
	int lcy;
	int dirty;
	int redrawing;
	int blocked;
	unsigned char *input_queue;
	int qlen;
	struct list_head windows;
	unsigned char *title;
};

struct term_spec {
	struct term_spec *next;
	struct term_spec *prev;
	unsigned char term[MAX_TERM_LEN];
	int mode;
	int m11_hack;
	int restrict_852;
	int block_cursor;
	int col;
	int charset;
};

#define TERM_DUMB	0
#define TERM_VT100	1
#define TERM_LINUX	2
#define TERM_KOI8	3

#define ATTR_FRAME	0x8000

extern struct list_head term_specs;
extern struct list_head terminals;

int hard_write(int, unsigned char *, int);
int hard_read(int, unsigned char *, int);
unsigned char *get_cwd();
void set_cwd(unsigned char *);
struct terminal *init_term(int, int, void (*)(struct window *, struct event *, int));
void sync_term_specs();
struct term_spec *new_term_spec(unsigned char *);
void free_term_specs();
void destroy_terminal(struct terminal *);
void redraw_terminal(struct terminal *);
void redraw_terminal_all(struct terminal *);
void redraw_terminal_cls(struct terminal *);
void cls_redraw_all_terminals();
void redraw_from_window(struct window *);
void redraw_below_window(struct window *);
void add_window(struct terminal *, void (*)(struct window *, struct event *, int), void *);
void add_window_at_pos(struct terminal *, void (*)(struct window *, struct event *, int), void *, struct window *);
void delete_window(struct window *);
void delete_window_ev(struct window *, struct event *ev);
void set_window_ptr(struct window *, int, int);
void get_parent_ptr(struct window *, int *, int *);
struct window *get_root_window(struct terminal *);
void add_empty_window(struct terminal *, void (*)(void *), void *);
void redraw_screen(struct terminal *);
void redraw_all_terminals();
void set_char(struct terminal *, int, int, unsigned);
unsigned get_char(struct terminal *, int, int);
void set_color(struct terminal *, int, int, unsigned);
void set_only_char(struct terminal *, int, int, unsigned);
void set_line(struct terminal *, int, int, int, chr *);
void set_line_color(struct terminal *, int, int, int, unsigned);
void fill_area(struct terminal *, int, int, int, int, unsigned);
void draw_frame(struct terminal *, int, int, int, int, unsigned, int);
void print_text(struct terminal *, int, int, int, unsigned char *, unsigned);
void set_cursor(struct terminal *, int, int, int, int);
void destroy_all_terminals();
void block_itrm(int);
int unblock_itrm(int);
void exec_thread(unsigned char *, int);
void close_handle(void *);

#define TERM_FN_TITLE	1
#define TERM_FN_RESIZE	2

void exec_on_terminal(struct terminal *, unsigned char *, unsigned char *, int);
void set_terminal_title(struct terminal *, unsigned char *);
void do_terminal_function(struct terminal *, unsigned char, unsigned char *);

/* language.c */

#include "lang_defs.h"

extern unsigned char dummyarray[];

extern int current_language;

void init_trans();
void shutdown_trans();
unsigned char *get_text_translation(unsigned char *, struct terminal *term);
unsigned char *get_english_translation(unsigned char *);
void set_language(int);
int n_languages();
unsigned char *language_name(int);

#define _(_x_, _y_) get_text_translation(_x_, _y_)
#define TEXT(x) (dummyarray + x)

/* types.c */

struct assoc {
	struct assoc *next;
	struct assoc *prev;
	tcount cnt;
	unsigned char *label;
	unsigned char *ct;
	unsigned char *prog;
	int cons;
	int xwin;
	int block;
	int ask;
	int system;
};

struct extension {
	struct extension *next;
	struct extension *prev;
	tcount cnt;
	unsigned char *ext;
	unsigned char *ct;
};

struct protocol_program {
	struct protocol_program *next;
	struct protocol_program *prev;
	unsigned char *prog;
	int system;
};

extern struct list_head assoc;
extern struct list_head extensions;

extern struct list_head mailto_prog;
extern struct list_head telnet_prog;
extern struct list_head tn3270_prog;

unsigned char *get_content_type(unsigned char *, unsigned char *);
struct assoc *get_type_assoc(struct terminal *term, unsigned char *);
void update_assoc(struct assoc *);
void update_ext(struct extension *);
void update_prog(struct list_head *, unsigned char *, int);
unsigned char *get_prog(struct list_head *);
void free_types();

void menu_add_ct(struct terminal *, void *, void *);
void menu_del_ct(struct terminal *, void *, void *);
void menu_list_assoc(struct terminal *, void *, void *);
void menu_add_ext(struct terminal *, void *, void *);
void menu_del_ext(struct terminal *, void *, void *);
void menu_list_ext(struct terminal *, void *, void *);

/* bfu.c */

struct memory_list {
	int n;
	void *p[1];
};

struct memory_list *getml(void *, ...);
void add_to_ml(struct memory_list **, ...);
void freeml(struct memory_list *);

#define MENU_FUNC (void (*)(struct terminal *, void *, void *))

extern unsigned char m_bar;

#define M_BAR	(&m_bar)

struct menu_item {
	unsigned char *text;
	unsigned char *rtext;
	unsigned char *hotkey;
	void (*func)(struct terminal *, void *, void *);
	void *data;
	int in_m;
	int free_i;
};

struct menu {
	int selected;
	int view;
	int xp, yp;
	int x, y, xw, yw;
	int ni;
	void *data;
	struct window *win;
	struct menu_item *items;
};

struct mainmenu {
	int selected;
	int sp;
	int ni;
	void *data;
	struct window *win;
	struct menu_item *items;
};

struct history_item {
	struct history_item *next;
	struct history_item *prev;
	unsigned char d[1];
};

struct history {
	int n;
	struct list_head items;
};

#define D_END		0
#define D_CHECKBOX	1
#define D_FIELD		2
#define D_FIELD_PASS	3
#define D_BUTTON	4
#define D_BOX		5

#define B_ENTER		1
#define B_ESC		2

struct dialog_item_data;
struct dialog_data;

struct dialog_item {
	int type;
	int gid, gnum; /* for buttons: gid - flags B_XXX */	/* for fields: min/max */ /* for box: gid is box height */
	int (*fn)(struct dialog_data *, struct dialog_item_data *);
	struct history *history;
	int dlen;
	unsigned char *data;
	void *udata; /* for box: holds list */
	unsigned char *text;
};

struct dialog_item_data {
	int x, y, l;
	int vpos, cpos;
	int checked;
	struct dialog_item *item;
	struct list_head history;
	struct history_item *cur_hist;
	unsigned char *cdata;
};

#define	EVENT_PROCESSED		0
#define EVENT_NOT_PROCESSED	1

struct dialog {
	unsigned char *title;
	void (*fn)(struct dialog_data *);
	int (*handle_event)(struct dialog_data *, struct event *);
	void (*abort)(struct dialog_data *);
	void *udata;
	void *udata2;
	int align;
	void (*refresh)(void *);
	void *refresh_data;
	struct dialog_item items[1];
};

struct dialog_data {
	struct window *win;
	struct dialog *dlg;
	int x, y, xw, yw;
	int n;
	int selected;
	struct memory_list *ml;
	struct dialog_item_data items[1];
};


/* Stores display information about a box. Kept in cdata. */
struct dlg_data_item_data_box {
	int sel;	/* Item currently selected */	
	int box_top;	/* Index into items of the item that is on the top line of the box */
	struct list_head items;	/* The list being displayed */
	int list_len;	/* Number of items in the list */
};

/* Which fields to free when zapping a box_item. Bitwise or these. */
enum box_item_free {NOTHING = 0, TEXT = 1 , DATA = 2};
/* An item in a box */
struct box_item {
	struct box_item *next;
	struct box_item *prev;
	unsigned char *text;	/* Text to display */
	void (*on_hilight)(struct terminal *, struct dlg_data_item_data_box *, struct box_item *);	/* Run when this item is hilighted */
	int (*on_selected)(struct terminal *, struct dlg_data_item_data_box *, struct box_item *);	/* Run when the user selects on this item. Returns pointer to the box_item that should be selected after execution*/
	void *data;	/* data */
	enum box_item_free free_i;
};

void show_dlg_item_box(struct dialog_data *, struct dialog_item_data *); 

#define BOX_HILIGHT_FUNC (void (*)(struct terminal *, struct dlg_data_item_data_box *, struct box_item *))
#define BOX_ON_SELECTED_FUNC (int (*)(struct terminal *, struct dlg_data_item_data_box *, struct box_item *))

/* Ops dealing with box data */
#define get_box_from_dlg_item_data(x) ((struct dlg_data_item_data_box *)(x->cdata))
#define get_box_list_height(x) (x->data_len)


struct menu_item *new_menu(int);
void add_to_menu(struct menu_item **, unsigned char *, unsigned char *, unsigned char *, void (*)(struct terminal *, void *, void *), void *, int);
void do_menu(struct terminal *, struct menu_item *, void *);
void do_menu_selected(struct terminal *, struct menu_item *, void *, int);
void do_mainmenu(struct terminal *, struct menu_item *, void *, int);
void do_dialog(struct terminal *, struct dialog *, struct memory_list *);
int check_number(struct dialog_data *, struct dialog_item_data *);
int check_nonempty(struct dialog_data *, struct dialog_item_data *);
void max_text_width(struct terminal *, unsigned char *, int *);
void min_text_width(struct terminal *, unsigned char *, int *);
void dlg_format_text(struct terminal *, struct terminal *, unsigned char *, int, int *, int, int *, int, int);
void max_buttons_width(struct terminal *, struct dialog_item_data *, int, int *);
void min_buttons_width(struct terminal *, struct dialog_item_data *, int, int *);
void dlg_format_buttons(struct terminal *, struct terminal *, struct dialog_item_data *, int, int, int *, int, int *, int);
void checkboxes_width(struct terminal *, unsigned char **, int *, void (*)(struct terminal *, unsigned char *, int *));
void dlg_format_checkbox(struct terminal *, struct terminal *, struct dialog_item_data *, int, int *, int, int *, unsigned char *);
void dlg_format_checkboxes(struct terminal *, struct terminal *, struct dialog_item_data *, int, int, int *, int, int *, unsigned char **);
void dlg_format_field(struct terminal *, struct terminal *, struct dialog_item_data *, int, int *, int, int *, int);
void max_group_width(struct terminal *, unsigned char **, struct dialog_item_data *, int, int *);
void min_group_width(struct terminal *, unsigned char **, struct dialog_item_data *, int, int *);
void dlg_format_group(struct terminal *, struct terminal *, unsigned char **, struct dialog_item_data *, int, int, int *, int, int *);
void dlg_format_box(struct terminal *, struct terminal *, struct dialog_item_data *, int, int *, int, int *, int);
void checkbox_list_fn(struct dialog_data *);
void group_fn(struct dialog_data *);
void center_dlg(struct dialog_data *);
void draw_dlg(struct dialog_data *);
void display_dlg_item(struct dialog_data *, struct dialog_item_data *, int);
int ok_dialog(struct dialog_data *, struct dialog_item_data *);
int cancel_dialog(struct dialog_data *, struct dialog_item_data *);
int clear_dialog(struct dialog_data *, struct dialog_item_data *);
void msg_box(struct terminal *, struct memory_list *, unsigned char *, int, /*unsigned char *, void *, int,*/ ...);
void input_field_fn(struct dialog_data *);
void input_field(struct terminal *, struct memory_list *, unsigned char *, unsigned char *, unsigned char *, unsigned char *, void *, struct history *, int, unsigned char *, int, int, int (*)(struct dialog_data *, struct dialog_item_data *), void (*)(void *, unsigned char *), void (*)(void *));

void add_to_history(struct history *, unsigned char *, int);

void box_sel_move(struct dialog_item_data *, int ); 
void show_dlg_item_box(struct dialog_data *, struct dialog_item_data *);
void box_sel_set_visible(struct dialog_item_data *, int ); 

/* menu.c */

#include "html.h"
#include "html_r.h"
#include "session.h"

void activate_bfu_technology(struct session *, int);
void dialog_goto_url(struct session *ses, char *url);
void dialog_save_url(struct session *ses);
void dialog_lua_console(struct session *ses);
void free_history_lists();
void query_file(struct session *, unsigned char *, void (*)(struct session *, unsigned char *), void (*)(struct session *));
void search_dlg(struct session *, struct f_data_c *, int);
void search_back_dlg(struct session *, struct f_data_c *, int);
void exit_prog(struct terminal *, void *, struct session *);
void do_auth_dialog(struct session *);

/* view.c */

extern int textarea_editor;
void textarea_edit(int, struct terminal *, struct form_control *, struct form_state *, struct f_data_c *, struct link *);

int can_open_in_new(struct terminal *);
void open_in_new_window(struct terminal *, void (*)(struct terminal *, void (*)(struct terminal *, unsigned char *, unsigned char *), struct session *ses), struct session *);
void send_open_in_new_xterm(struct terminal *term, void (*open_window)(struct terminal *term, unsigned char *, unsigned char *), struct session *ses);
void send_open_new_xterm(struct terminal *, void (*)(struct terminal *, unsigned char *, unsigned char *), struct session *);
void destroy_fc(struct form_control *);
void sort_links(struct f_data *);
void destroy_formatted(struct f_data *);
void clear_formatted(struct f_data *);
void init_formatted(struct f_data *);
void detach_formatted(struct f_data_c *);
void init_vs(struct view_state *, unsigned char *);
void destroy_vs(struct view_state *);
void copy_location(struct location *, struct location *);
void draw_doc(struct terminal *, struct f_data_c *, int);
int dump_to_file(struct f_data *, int);
void draw_formatted(struct session *);
void send_event(struct session *, struct event *);
void link_menu(struct terminal *, void *, struct session *);
void save_as(struct terminal *, void *, struct session *);
void save_url(struct session *, unsigned char *);
void menu_save_formatted(struct terminal *, void *, struct session *);
void selected_item(struct terminal *, void *, struct session *);
void toggle(struct session *, struct f_data_c *, int);
void do_for_frame(struct session *, void (*)(struct session *, struct f_data_c *, int), int);
int get_current_state(struct session *);
unsigned char *print_current_link(struct session *);
unsigned char *print_current_title(struct session *);
void loc_msg(struct terminal *, struct location *, struct f_data_c *);
void state_msg(struct session *);
void search_for(struct session *, unsigned char *);
void search_for_back(struct session *, unsigned char *);
void find_next(struct session *, struct f_data_c *, int);
void find_next_back(struct session *, struct f_data_c *, int);
void set_frame(struct session *, struct f_data_c *, int);
struct f_data_c *current_frame(struct session *);

/* default.c */

#define MAX_STR_LEN	1024

#define option option_dirty_workaround_for_name_clash_with_include_on_cygwin

struct option {
	unsigned char *cmd_name;
	unsigned char *cfg_name;
	unsigned char *(*rd_cmd)(struct option *, unsigned char ***, int *);
	unsigned char *(*rd_cfg)(struct option *, unsigned char *);
	void (*wr_cfg)(struct option *, unsigned char **, int *);
	int min, max;
	void *ptr;
	unsigned char *desc;
};

unsigned char *parse_options(int, unsigned char *[]);
void init_home();
unsigned char *get_token(unsigned char **line);
void load_config();
void write_config(struct terminal *);
void write_html_config(struct terminal *);
void end_config();

int load_url_history();
int save_url_history();

extern int anonymous;
extern unsigned char user_agent[];

extern unsigned char system_name[];

extern unsigned char *links_home;
extern int first_use;

extern int created_home;

extern int no_connect;
extern int base_session;

#define D_DUMP		1
#define D_SOURCE	2
extern int dmp;
extern int dump_width;

typedef enum {
	COOKIES_ACCEPT_NONE,
	COOKIES_ACCEPT_ASK,
	COOKIES_ACCEPT_ALL
} cookies_accept_t;

extern cookies_accept_t cookies_accept;
extern int cookies_save;
extern int cookies_resave;
extern int cookies_paranoid_security;

extern int async_lookup;
extern int download_utime;
extern int max_connections;
extern int max_connections_to_host;
extern int max_tries;
extern int receive_timeout;
extern int unrestartable_receive_timeout;

extern int keep_unhistory;

extern struct document_setup dds;

extern int max_format_cache_entries;
extern long memory_cache_size;

extern struct rgb default_fg;
extern struct rgb default_bg;
extern struct rgb default_link;
extern struct rgb default_vlink;

extern int color_dirs;

extern int show_status_bar;
extern int show_title_bar;

extern int form_submit_auto;
extern int form_submit_confirm;
extern int accesskey_enter;
extern int accesskey_priority;
extern int links_wraparound;

extern int allow_special_files;

typedef enum {
	REFERER_NONE,
	REFERER_SAME_URL,
	REFERER_FAKE,
	REFERER_TRUE,
} referer_t;

extern referer_t referer;
extern unsigned char fake_referer[];
extern unsigned char http_proxy[];
extern unsigned char ftp_proxy[];
extern unsigned char no_proxy_for[];
extern unsigned char download_dir[];

extern int startup_goto_dialog;

struct http_bugs {
	int http10;
	int allow_blacklist;
	int bug_302_redirect;
	int bug_post_no_keepalive;
};

extern struct http_bugs http_bugs;

extern unsigned char default_anon_pass[];

/* bookmarks.c */

/* Where all bookmarks are kept */
extern struct list_head bookmarks;

/* A pointer independent id that bookmarks can be identified by. Guarenteed to 
	be unique between all bookmarks */
typedef int bookmark_id;
extern bookmark_id next_bookmark_id;
#define		BAD_BOOKMARK_ID		(bookmark_id)(-1)

/* Bookmark record structure */
struct bookmark {
	struct bookmark *next;
	struct bookmark *prev;
	bookmark_id id;	/* Bookmark id */
	unsigned char *title;	/* title of bookmark */
	unsigned char *url;	/* Location of bookmarked item */
	int selected; /* Whether to display this bookmark or not. */
};

/* Cleanups and saves bookmarks */
void finalize_bookmarks();

/* Read/write bookmarks functions */
void read_bookmarks();
void write_bookmarks();

void bookmark_menu(struct terminal *, void *, struct session *);

/* Launches bookmark manager */
void menu_bookmark_manager(struct terminal *, void *, struct session *);

void add_bookmark(const unsigned char *, const unsigned char *);

struct bookmark *create_bookmark(const unsigned char *, const unsigned char *);

/* Launches add dialogs */
void launch_bm_add_link_dialog(struct terminal *,struct dialog_data *,struct session *);
void launch_bm_add_doc_dialog(struct terminal *,struct dialog_data *,struct session *);


/* kbdbind.c */

#define KM_MAIN		0
#define KM_EDIT		1
#define KM_MENU		2
#define KM_MAX		3

/* Note: if you add anything here, please keep it in alphabetical order,
 * and also update the table in parse_act() in kbdbind.c.  */
enum {
	ACT_ADD_BOOKMARK,
	ACT_AUTO_COMPLETE,
	ACT_BACK,
	ACT_BACKSPACE,
	ACT_BOOKMARK_MANAGER,
	ACT_COOKIES_LOAD,
	ACT_COPY_CLIPBOARD,
	ACT_CUT_CLIPBOARD,
	ACT_DELETE,
	ACT_DOCUMENT_INFO,
	ACT_DOWN,
	ACT_DOWNLOAD,
	ACT_EDIT,
	ACT_END,
	ACT_ENTER,
	ACT_FILE_MENU,
	ACT_FIND_NEXT,
	ACT_FIND_NEXT_BACK,
	ACT_GOTO_URL,
	ACT_GOTO_URL_CURRENT,
	ACT_GOTO_URL_CURRENT_LINK,
	ACT_HEADER_INFO,
	ACT_HOME,
	ACT_KILL_TO_BOL,
	ACT_KILL_TO_EOL,
	ACT_LEFT,
	ACT_LUA_CONSOLE,
	ACT_LUA_FUNCTION,
	ACT_MENU,
	ACT_NEXT_FRAME,
	ACT_OPEN_NEW_WINDOW,
	ACT_OPEN_LINK_IN_NEW_WINDOW,
	ACT_PAGE_DOWN,
	ACT_PAGE_UP,
	ACT_PASTE_CLIPBOARD,
	ACT_PREVIOUS_FRAME,
	ACT_QUIT,
	ACT_REALLYQUIT,
	ACT_RELOAD,
	ACT_RIGHT,
	ACT_SCROLL_DOWN,
	ACT_SCROLL_LEFT,
	ACT_SCROLL_RIGHT,
	ACT_SCROLL_UP,
	ACT_SEARCH,
	ACT_SEARCH_BACK,
	ACT_TOGGLE_DISPLAY_IMAGES,
	ACT_TOGGLE_DISPLAY_TABLES,
	ACT_TOGGLE_HTML_PLAIN,
	ACT_UNBACK,
	ACT_UP,
	ACT_VIEW_IMAGE,
	ACT_ZOOM_FRAME
};

void init_keymaps();
void free_keymaps();
long parse_key(unsigned char *);
int kbd_action(int, struct event *, int *);
unsigned char *bind_rd(struct option *, unsigned char *);
unsigned char *unbind_rd(struct option *, unsigned char *);
#ifdef HAVE_LUA
unsigned char *bind_lua_func(unsigned char *, unsigned char *, int);
#endif

/* lua.c */

#ifdef HAVE_LUA

extern lua_State *lua_state;

void init_lua();
void alert_lua_error(unsigned char *);
void alert_lua_error2(unsigned char *, unsigned char *);
int prepare_lua(struct session *);
void finish_lua();
void lua_console(struct session *, unsigned char *);
void handle_standard_lua_returns(unsigned char *);

#endif

#endif
