/* Terminal interface - low-level displaying implementation */
/* $Id: terminal.c,v 1.13 2002/06/16 21:22:12 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "links.h"

#include "main.h"
/* We don't require this ourselves, but view.h does, and directly including
 * view.h w/o session.h already loaded doesn't work :/. --pasky */
#include "document/session.h"
#include "document/view.h"
#include "lowlevel/kbd.h"
#include "lowlevel/select.h"
#include "lowlevel/terminal.h"
#include "util/conv.h"
#include "util/error.h"


/* TODO: We must use termcap/terminfo if available! --pasky */

/* hard_write() */
int
hard_write(int fd, unsigned char *p, int l)
{
	int w = 1;
	int t = 0;

	while (l > 0 && w) {
		w = write(fd, p, l);
		if (w < 0) {
			if (errno == EINTR) continue;
			return -1;
		}
		t += w;
		p += w;
		l -= w;
	}

	return t;
}

/* hard_read() */
int hard_read(int fd, unsigned char *p, int l)
{
	int r = 1;
	int t = 0;

	while (l > 0 && r) {
		r = read(fd, p, l);
		if (r < 0) {
			if (errno == EINTR) continue;
			return -1;
		}
#if 0
		{
		 int ww;

		 for (ww = 0; ww < r; ww++)
			 fprintf(stderr, " %02x", (int) p[ww]);
		 fflush(stderr);
		}
#endif
	 	t += r;
		p += r;
		l -= r;
	}

	return t;
}


/* get_cwd() */
unsigned char *get_cwd()
{
	int bufsize = 128;
	unsigned char *buf;

	while (1) {
		buf = mem_alloc(bufsize);
		if (!buf) return NULL;
		if (getcwd(buf, bufsize)) return buf;
		mem_free(buf);

		if (errno == EINTR) continue;
		if (errno != ERANGE) return NULL;
		bufsize += 128;
	}

	return NULL;
}

/* set_cwd() */
void set_cwd(unsigned char *path)
{
	if (path) while (chdir(path) && errno == EINTR);
}


struct list_head terminals = {&terminals, &terminals};


/* alloc_term_screen() */
void alloc_term_screen(struct terminal *term, int x, int y)
{
	unsigned *s;
	unsigned *t;
	int space = x * y * sizeof(unsigned);

	s = mem_realloc(term->screen, space);
	if (!s) return;
	t = mem_realloc(term->last_screen, space);
	if (!t) return;

	memset(t, -1, space);
	term->x = x;
	term->y = y;
	term->last_screen = t;
	memset(s, 0, space);
	term->screen = s;
	term->dirty = 1;
}


void in_term(struct terminal *);
void destroy_terminal(struct terminal *);
void check_if_no_terminal();


/* clear_terminal() */
void clear_terminal(struct terminal *term)
{
	fill_area(term, 0, 0, term->x, term->y, ' ');
	set_cursor(term, 0, 0, 0, 0);
}

/* redraw_terminal_ev() */
void redraw_terminal_ev(struct terminal *term, int e)
{
	struct window *win;
	struct event ev = {0, 0, 0, 0};

	ev.ev = e;
	ev.x = term->x;
	ev.y = term->y;
	clear_terminal(term);
	term->redrawing = 2;

	foreachback(win, term->windows)
		win->handler(win, &ev, 0);

	term->redrawing = 0;
}

/* redraw_terminal() */
void redraw_terminal(struct terminal *term)
{
	redraw_terminal_ev(term, EV_REDRAW);
}

/* redraw_terminal_all() */
void redraw_terminal_all(struct terminal *term)
{
	redraw_terminal_ev(term, EV_RESIZE);
}

/* erase_screen() */
void erase_screen(struct terminal *term)
{
	if (!term->master || !is_blocked()) {
		if (term->master) want_draw();
		hard_write(term->fdout, "\033[2J\033[1;1H", 10);
		if (term->master) done_draw();
	}
}

/* redraw_terminal_cls() */
void redraw_terminal_cls(struct terminal *term)
{
	erase_screen(term);
	alloc_term_screen(term, term->x, term->y);
	redraw_terminal_all(term);
}

/* cls_redraw_all_terminals() */
void cls_redraw_all_terminals()
{
	struct terminal *term;

	foreach(term, terminals)
		redraw_terminal_cls(term);
}

/* redraw_from_window() */
void redraw_from_window(struct window *win)
{
	struct terminal *term = win->term;
	struct window *end = (void *)&term->windows;
	struct event ev = {EV_REDRAW, 0, 0, 0};

	ev.x = term->x;
	ev.y = term->y;
	if (term->redrawing) return;

	term->redrawing = 1;
	for (win = win->prev; win != end; win = win->prev) {
		win->handler(win, &ev, 0);
	}
	term->redrawing = 0;
}

/* redraw_below_window() */
void redraw_below_window(struct window *win)
{
	int tr;
	struct terminal *term = win->term;
	struct window *end = win;
	struct event ev = {EV_REDRAW, 0, 0, 0};

	ev.x = term->x;
	ev.y = term->y;
	if (term->redrawing >= 2) return;
	tr = term->redrawing;
	win->term->redrawing = 2;
	for (win = term->windows.prev; win != end; win = win->prev) {
		win->handler(win, &ev, 0);
	}
	term->redrawing = tr;
}

/* add_window_at_pos() */
void add_window_at_pos(struct terminal *term,
		       void (*handler)(struct window *, struct event *, int),
		       void *data, struct window *at)
{
	struct event ev = {EV_INIT, 0, 0, 0};
	struct window *win;

	ev.x = term->x;
	ev.y = term->y;

	win = mem_alloc(sizeof (struct window));
	if (!win) {
		mem_free(data);
		return;
	}

	win->handler = handler;
	win->data = data;
	win->term = term;
	win->xp = win->yp = 0;
	add_at_pos(at, win);
	win->handler(win, &ev, 0);
}

/* add_window() */
void add_window(struct terminal *term,
		void (*handler)(struct window *, struct event *, int),
		void *data)
{
	add_window_at_pos(term, handler, data,
			  (struct window *) &term->windows);
}

/* delete_window() */
void delete_window(struct window *win)
{
	struct event ev = {EV_ABORT, 0, 0, 0};

	win->handler(win, &ev, 1);
	del_from_list(win);
	if (win->data) mem_free(win->data);
	redraw_terminal(win->term);
	mem_free(win);
}

/* delete_window_ev() */
void delete_window_ev(struct window *win, struct event *ev)
{
	struct window *w = win->next;

	if ((void *)w == &win->term->windows) w = NULL;
	delete_window(win);
	if (ev && w && w->next != w) w->handler(w, ev, 1);
}

/* set_window_ptr() */
void set_window_ptr(struct window *win, int x, int y)
{
	win->xp = x;
	win->yp = y;
}

/* get_parent_ptr() */
void get_parent_ptr(struct window *win, int *x, int *y)
{
	if ((void *)win->next != &win->term->windows) {
		*x = win->next->xp;
		*y = win->next->yp;
	} else {
		*x = 0;
		*y = 0;
	}
}

/* get_root_window() */
struct window *get_root_window(struct terminal *term)
{
	if (list_empty(term->windows)) {
		internal("terminal has no windows");
		return NULL;
	}

	return (struct window *)term->windows.prev;
}


struct ewd {
	void (*fn)(void *);
	void *data;
	int b;
};


/* empty_window_handler() */
void empty_window_handler(struct window *win, struct event *ev, int fwd)
{
	struct window *n;
	struct ewd *ewd = win->data;
	int x, y;
	void (*fn)(void *) = ewd->fn;
	void *data = ewd->data;

	if (ewd->b) return;

	switch (ev->ev) {
		case EV_INIT:
		case EV_RESIZE:
		case EV_REDRAW:
			get_parent_ptr(win, &x, &y);
			set_window_ptr(win, x, y);
			return;
		case EV_ABORT:
			fn(data);
			return;
		case EV_KBD:
		case EV_MOUSE:
			/* Silence compiler warnings */
			break;
	}

	ewd->b = 1;
	n = win->next;
	delete_window(win);
	fn(data);
	if (n->next != n) n->handler(n, ev, fwd);
}

/* add_empty_window() */
void add_empty_window(struct terminal *term, void (*fn)(void *), void *data)
{
	struct ewd *ewd = mem_alloc(sizeof(struct ewd));

	if (!ewd) return;
	ewd->fn = fn;
	ewd->data = data;
	ewd->b = 0;
	add_window(term, empty_window_handler, ewd);
}



/* init_term() */
struct terminal *init_term(int fdin, int fdout,
			   void (*root_window)(struct window *, struct event *,
					       int))
{
	struct window *win;
	struct terminal *term = mem_alloc(sizeof (struct terminal));

	if (!term) {
		check_if_no_terminal();
		return NULL;
	}
	memset(term, 0, sizeof(struct terminal));

	term->fdin = fdin;
	term->fdout = fdout;
	term->master = (term->fdout == get_output_handle());
#if 0
	term->x = 0;
	term->y = 0;
	term->cx = 0;
	term->cy = 0;
#endif
	term->lcx = -1;
	term->lcy = -1;
	term->dirty = 1;
	term->redrawing = 0;
	term->blocked = -1;
	term->screen = DUMMY;
	term->last_screen = DUMMY;
	term->spec = get_opt_rec(root_options, "terminal.default");
	term->term[0] = 0;
	term->cwd[0] = 0;
	term->input_queue = DUMMY;
	term->qlen = 0;

	init_list(term->windows);

	win = mem_alloc(sizeof (struct window));
	if (!win) {
		mem_free(term);
		check_if_no_terminal();
		return NULL;
	}

	win->handler = root_window;
	win->data = NULL;
	win->term = term;

	add_to_list(term->windows, win);
	/*alloc_term_screen(term, 80, 25);*/
	add_to_list(terminals, term);

	set_handlers(fdin, (void (*)(void *)) in_term, NULL,
		     (void (*)(void *)) destroy_terminal, term);
	return term;
}


static inline void term_send_event(struct terminal *term, struct event *ev)
{
	((struct window *)&term->windows)->next->handler(term->windows.next, ev, 0);
}

static inline void term_send_ucs(struct terminal *term, struct event *ev, int u)
{
	struct list_head *opt_tree = (struct list_head *) term->spec->ptr;
	unsigned char *recoded;

	if (u == 0xA0) u = ' ';
	recoded = u2cp(u, get_opt_int_tree(opt_tree, "charset"));
	if (! recoded) recoded = "*";
	while (*recoded) {
		ev->x = *recoded;
		term_send_event(term, ev);
		recoded ++;
	}
}

/* in_term() */
void in_term(struct terminal *term)
{
	struct list_head *opt_tree = (struct list_head *) term->spec->ptr;
	struct event *ev;
	int r;
	unsigned char *iq;

	iq = mem_realloc(term->input_queue, term->qlen + ALLOC_GR);
	if (!iq) {
		destroy_terminal(term);
		return;
	}
	term->input_queue = iq;

	r = read(term->fdin, iq + term->qlen, ALLOC_GR);
	if (r <= 0) {
		if (r == -1 && errno != ECONNRESET)
			error("ERROR: error %d on terminal: could not read event", errno);
		destroy_terminal(term);
		return;
	}
	term->qlen += r;

test_queue:
	if (term->qlen < sizeof(struct event)) return;
	ev = (struct event *)iq;
	r = sizeof(struct event);

	if (ev->ev != EV_INIT
	    && ev->ev != EV_RESIZE
	    && ev->ev != EV_REDRAW
	    && ev->ev != EV_KBD
	    && ev->ev != EV_MOUSE
	    && ev->ev != EV_ABORT) {
		error("ERROR: error on terminal: bad event %d", ev->ev);
		goto mm;
	}

	if (ev->ev == EV_INIT) {
		int init_len;
		int evterm_len = sizeof(struct event) + MAX_TERM_LEN;
		int evtermcwd_len = evterm_len + MAX_CWD_LEN;
		int evtermcwd1int_len = evtermcwd_len + sizeof(int);
		int evtermcwd2int_len = evtermcwd1int_len + sizeof(int);

		if (term->qlen < evtermcwd2int_len) return;
		init_len = *(int *)(iq + evtermcwd1int_len);

		if (term->qlen < evtermcwd2int_len + init_len) return;

		memcpy(term->term, iq + sizeof(struct event), MAX_TERM_LEN);
		term->term[MAX_TERM_LEN - 1] = 0;

		{
			unsigned char name[MAX_TERM_LEN + 10];

			sprintf(name, "terminal.%s", term->term);
			term->spec = get_opt_rec(root_options, name);
		}

		memcpy(term->cwd, iq + evterm_len, MAX_CWD_LEN);
		term->cwd[MAX_CWD_LEN - 1] = 0;

		term->environment = *(int *)(iq + evtermcwd_len);
		ev->b = (long)(iq + evtermcwd1int_len);
		r = evtermcwd2int_len + init_len;
	}

	if (ev->ev == EV_REDRAW || ev->ev == EV_RESIZE || ev->ev == EV_INIT) {
		struct window *win;

send_redraw:
		if (ev->x < 0 || ev->y < 0) {
			error("ERROR: bad terminal size: %d, %d",
			      (int) ev->x, (int) ev->y);
			goto mm;
		}

		alloc_term_screen(term, ev->x, ev->y);
		clear_terminal(term);
		erase_screen(term);
		term->redrawing = 1;
		foreachback(win, term->windows)
			win->handler(win, ev, 0);
		term->redrawing = 0;
	}

	if (ev->ev == EV_KBD || ev->ev == EV_MOUSE) {
		if (ev->ev == EV_KBD && upcase(ev->x) == 'L'
		    && ev->y == KBD_CTRL) {
			ev->ev = EV_REDRAW;
			ev->x = term->x;
			ev->y = term->y;
			goto send_redraw;
		}
		else if (ev->ev == EV_KBD && ev->x == KBD_CTRL_C)
			((struct window *) &term->windows)->prev->handler(term->windows.prev, ev, 0);
		else if (ev->ev == EV_KBD) {
			if (term->utf_8.len) {
				if ((ev->x & 0xC0) == 0x80
				    && get_opt_bool_tree(opt_tree, "utf_8_io")) {
					term->utf_8.ucs <<= 6;
					term->utf_8.ucs |= ev->x & 0x3F;
					if (! --term->utf_8.len) {
						int u = term->utf_8.ucs;

						if (u < term->utf_8.min) u = UCS_NO_CHAR;
						term_send_ucs(term, ev, u);
					}
					goto mm;
				} else {
					term->utf_8.len = 0;
					term_send_ucs(term, ev, UCS_NO_CHAR);
				}
			}
			if (ev->x < 0x80 || ev->x > 0xFF
			    || !get_opt_bool_tree(opt_tree, "utf_8_io")) {
				term_send_event(term, ev);
				goto mm;
			} else if ((ev->x & 0xC0) == 0xC0 && (ev->x & 0xFE) != 0xFE) {
				int mask, len = 0, cov = 0x80;

				for (mask = 0x80; ev->x & mask; mask >>= 1) {
					len++;
					term->utf_8.min = cov;
					cov = 1 << (1 + 5 * len);
				}
				term->utf_8.len = len - 1;
				term->utf_8.ucs = ev->x & (mask - 1);
				goto mm;
			}
			term_send_ucs(term, ev, UCS_NO_CHAR);
		} else term_send_event(term, ev);
	}

	if (ev->ev == EV_ABORT) destroy_terminal(term);
	/* redraw_screen(term); */
mm:
	if (term->qlen == r) term->qlen = 0;
	else memmove(iq, iq + r, term->qlen -= r);

	goto test_queue;
}


/* getcompcode() */
inline int getcompcode(int c)
{
	return (c<<1 | (c&4)>>2) & 7;
}


unsigned char frame_dumb[48] =	"   ||||++||++++++--|-+||++--|-+----++++++++     ";
unsigned char frame_vt100[48] =	"aaaxuuukkuxkjjjkmvwtqnttmlvwtqnvvwwmmllnnjla    ";

/* For UTF8 I/O */ 
unsigned char frame_vt100_u[48] = {
	177, 177, 177, 179, 180, 180, 180, 191,
	191, 180, 179, 191, 217, 217, 217, 191,
	192, 193, 194, 195, 196, 197, 195, 195,
	192, 218, 193, 194, 195, 196, 197, 193,
	193, 194, 194, 192, 192, 218, 218, 197,
	197, 217, 218, 177,  32, 32,  32,  32
};

unsigned char frame_koi[48] = {
	144,145,146,129,135,178,180,167,
	166,181,161,168,174,173,172,131,
	132,137,136,134,128,138,175,176,
	171,165,187,184,177,160,190,185,
	186,182,183,170,169,162,164,189,
	188,133,130,141,140,142,143,139,
};

unsigned char frame_restrict[48] = {
	0, 0, 0, 0, 0, 179, 186, 186,
	205, 0, 0, 0, 0, 186, 205, 0,
	0, 0, 0, 0, 0, 0, 179, 186,
	0, 0, 0, 0, 0, 0, 0, 205,
	196, 205, 196, 186, 205, 205, 186, 186,
	179, 0, 0, 0, 0, 0, 0, 0,
};


#define PRINT_CHAR(p)								\
{										\
	unsigned ch = term->screen[p];						\
	unsigned char c = ch & 0xff;						\
	unsigned char A = ch >> 8 & 0x7f;					\
										\
	if (get_opt_int_tree(opt_tree, "type") == TERM_LINUX) {			\
		if (get_opt_bool_tree(opt_tree, "m11_hack") &&			\
		    !get_opt_bool_tree(opt_tree, "utf_8_io")) {			\
			if (ch >> 15 != mode) {					\
				mode = ch >> 15;				\
				if (!mode) add_to_str(&a, &l, "\033[10m");	\
				else add_to_str(&a, &l, "\033[11m");		\
			}							\
		}								\
		if (get_opt_bool_tree(opt_tree, "restrict_852")) {		\
			if ((ch >> 15) && c >= 176 && c < 224) {		\
				if (frame_restrict[c - 176])			\
					c = frame_restrict[c - 176];		\
			}							\
		}								\
	} else if (get_opt_int_tree(opt_tree, "type") == TERM_VT100		\
		   && !get_opt_bool_tree(opt_tree, "utf_8_io")) {		\
		if (ch >> 15 != mode) {						\
			mode = ch >> 15;					\
			if (!mode) add_to_str(&a, &l, "\x0f");			\
			else add_to_str(&a, &l, "\x0e");			\
		}								\
		if (mode && c >= 176 && c < 224) c = frame_vt100[c - 176];	\
	} else if (get_opt_int_tree(opt_tree, "type") == TERM_VT100		\
		   && (ch >> 15) && c >= 176 && c < 224) { 			\
		c = frame_vt100_u[c - 176];					\
	} else if (get_opt_int_tree(opt_tree, "type") == TERM_KOI8		\
		   && (ch >> 15) && c >= 176 && c < 224) { 			\
		c = frame_koi[c - 176];						\
	} else if (get_opt_int_tree(opt_tree, "type") == TERM_DUMB		\
		   && (ch >> 15) && c >= 176 && c < 224)			\
		c = frame_dumb[c - 176];					\
										\
	if (!(A & 0100) && (A >> 3) == (A & 7)) A = (A & 070) | 7 * !(A & 020);	\
	if (A != attrib) {							\
		attrib = A;							\
		add_to_str(&a, &l, "\033[0");					\
		if (get_opt_bool_tree(opt_tree, "colors")) {			\
			unsigned char m[4];					\
										\
			m[0] = ';';						\
		       	m[1] = '3';						\
			m[2] = (attrib & 7) + '0';				\
		       	m[3] = 0;						\
			add_to_str(&a, &l, m);					\
			m[1] = '4';						\
			m[2] = (attrib >> 3 & 7) + '0';				\
			add_to_str(&a, &l, m);					\
		} else if (getcompcode(attrib & 7) < getcompcode(attrib >> 3 & 7))	\
			add_to_str(&a, &l, ";7");				\
		if (attrib & 0100) add_to_str(&a, &l, ";1");			\
		add_to_str(&a, &l, "m");					\
	}									\
	if (c >= ' ' && c != 127/* && c != 155*/) {				\
		int charset = get_opt_int_tree(opt_tree, "charset");		\
		int type = get_opt_int_tree(opt_tree, "type");			\
										\
		if (ch >> 15) {							\
			int frames_charset = (type == TERM_LINUX ||		\
					      type == TERM_VT100)		\
						? get_cp_index("cp437")		\
						: type == TERM_KOI8		\
							? get_cp_index("koi8-r")\
							: -1;			\
			if (frames_charset != -1) charset = frames_charset;	\
		}								\
		if (get_opt_bool_tree(opt_tree, "utf_8_io"))			\
			add_to_str(&a, &l, cp2utf_8(charset, c));		\
		else 								\
			add_chr_to_str(&a, &l, c);				\
	}									\
	else if (!c || c == 1) add_chr_to_str(&a, &l, ' ');			\
	else add_chr_to_str(&a, &l, '.');					\
	cx++;									\
}										\


/* redraw_all_terminals() */
void redraw_all_terminals()
{
	struct terminal *term;

	foreach(term, terminals)
		redraw_screen(term);
}


/* redraw_screen() */
void redraw_screen(struct terminal *term)
{
	struct list_head *opt_tree = (struct list_head *) term->spec->ptr;
	int x, y, p = 0;
	int cx = -1, cy = -1;
	unsigned char *a;
	int attrib = -1;
	int mode = -1;
	int l = 0;

	if (!term->dirty || (term->master && is_blocked())) return;

	a = init_str();
	if (!a) return;

	for (y = 0; y < term->y; y++)
		for (x = 0; x < term->x; x++, p++) {
			if (y == term->y - 1 && x == term->x - 1) break;
#define TSP term->screen[p]
#define TLSP term->last_screen[p]
			if (TSP == TLSP) continue;
			if ((TSP & 0x3800) == (TLSP & 0x3800)
			    && ((TSP & 0xff) == 0 || (TSP & 0xff) == 1 ||
				(TSP & 0xff) == ' ')
			    && ((TLSP & 0xff) == 0 || (TLSP & 0xff) == 1 ||
				(TLSP & 0xff) == ' '))
				continue;
#undef TSP
#undef TLSP
			if (cx == x && cy == y) {
				PRINT_CHAR(p);
			} else if (cy == y && x - cx < 10) {
				int i;

				for (i = x - cx; i >= 0; i--)
					PRINT_CHAR(p - i);
			} else {
				add_to_str(&a, &l, "\033[");
				add_num_to_str(&a, &l, y + 1);
				add_to_str(&a, &l, ";");
				add_num_to_str(&a, &l, x + 1);
				add_to_str(&a, &l, "H");
				cx = x; cy = y;
				PRINT_CHAR(p);
			}
		}

	if (l) {
		if (get_opt_int_tree(opt_tree, "colors"))
				add_to_str(&a, &l, "\033[37;40m");

		add_to_str(&a, &l, "\033[0m");

		if (get_opt_int_tree(opt_tree, "type") == TERM_LINUX
		    && get_opt_bool_tree(opt_tree, "m11_hack"))
			add_to_str(&a, &l, "\033[10m");

		if (get_opt_int_tree(opt_tree, "type") == TERM_VT100)
			add_to_str(&a, &l, "\x0f");
	}

	if (l || term->cx != term->lcx || term->cy != term->lcy) {
		term->lcx = term->cx;
		term->lcy = term->cy;
		add_to_str(&a, &l, "\033[");
		add_num_to_str(&a, &l, term->cy + 1);
		add_to_str(&a, &l, ";");
		add_num_to_str(&a, &l, term->cx + 1);
		add_to_str(&a, &l, "H");
	}

	if (l && term->master) want_draw();
	hard_write(term->fdout, a, l);
	if (l && term->master) done_draw();

	mem_free(a);
	memcpy(term->last_screen, term->screen, term->x * term->y * sizeof(int));
	term->dirty = 0;
}


/* destroy_terminal() */
void destroy_terminal(struct terminal *term)
{
	while ((term->windows.next) != &term->windows)
		delete_window(term->windows.next);

	/* if (term->cwd) mem_free(term->cwd); */
	if (term->title) mem_free(term->title);
	mem_free(term->screen);
	mem_free(term->last_screen);

	set_handlers(term->fdin, NULL, NULL, NULL, NULL);
	mem_free(term->input_queue);

	if (term->blocked != -1) {
		close(term->blocked);
		set_handlers(term->blocked, NULL, NULL, NULL, NULL);
	}

	del_from_list(term);
	close(term->fdin);

	if (term->fdout != 1) {
		if (term->fdout != term->fdin) close(term->fdout);
	} else {
		unhandle_terminal_signals(term);
		free_all_itrms();
#ifndef NO_FORK_ON_EXIT
		if (!list_empty(terminals)) {
			if (fork()) exit(0);
		}
#endif
	}

	mem_free(term);
	check_if_no_terminal();
}


/* destroy_all_terminals() */
void destroy_all_terminals()
{
	struct terminal *term;

	while ((void *) (term = terminals.next) != &terminals)
		destroy_terminal(term);
}


/* check_if_no_terminal() */
void check_if_no_terminal()
{
	if (list_empty(terminals)) {
		terminate = 1;
	}
}


/* set_char() */
void set_char(struct terminal *t, int x, int y, unsigned c)
{
	t->dirty = 1;
	if (x >= 0 && x < t->x && y >= 0 && y < t->y)
		t->screen[x + t->x * y] = c;
}


/* get_char() */
unsigned get_char(struct terminal *t, int x, int y)
{
	if (x >= t->x) x = t->x - 1;
	if (x < 0) x = 0;
	if (y >= t->y) y = t->y - 1;
	if (y < 0) y = 0;

	return t->screen[x + t->x * y];
}


/* set_color() */
void set_color(struct terminal *t, int x, int y, unsigned c)
{
	t->dirty = 1;
	if (x >= 0 && x < t->x && y >= 0 && y < t->y) {
		int p = x + t->x * y;

		t->screen[p] = (t->screen[p] & 0x80ff) | (c & ~0x80ff);
	}
}


/* set_only_char() */
void set_only_char(struct terminal *t, int x, int y, unsigned c)
{
	t->dirty = 1;
	if (x >= 0 && x < t->x && y >= 0 && y < t->y) {
		int p = x + t->x * y;

		t->screen[p] = (t->screen[p] & ~0x80ff) | (c & 0x80ff);
	}
}


/* set_line() */
void set_line(struct terminal *t, int x, int y, int l, chr *line)
{
	int i = (x >= 0) ? 0 : -x;
	int end = (x + l <= t->x) ? l : t->x - x;

	t->dirty = 1;

	for (; i < end; i++)
		t->screen[x + i + t->x * y] = line[i];
}


/* set_line_color() */
void set_line_color(struct terminal *t, int x, int y, int l, unsigned c)
{
	int i = (x >= 0) ? 0 : -x;
	int end = (x + l <= t->x) ? l : t->x - x;

	t->dirty = 1;

	for (; i < end; i++) {
		int p = x + i + t->x * y;

		t->screen[p] = (t->screen[p] & 0x80ff) | (c & ~0x80ff);
	}
}


/* fill_area() */
void fill_area(struct terminal *t, int x, int y, int xw, int yw, unsigned c)
{
	int j = (y >= 0) ? 0 : -y;

	t->dirty = 1;
	for (; j < yw && y+j < t->y; j++) {
		int i = (x >= 0) ? 0 : -x;

		for (; i < xw && x + i < t->x; i++)
			t->screen[x + i + t->x * (y + j)] = c;
	}
}


int p1[] = { 218, 191, 192, 217, 179, 196 };
int p2[] = { 201, 187, 200, 188, 186, 205 };


/* draw_frame() */
void draw_frame(struct terminal *t, int x, int y, int xw, int yw,
		unsigned c, int w)
{
	int *p = w > 1 ? p2 : p1;

	c |= ATTR_FRAME;
	set_char(t, x, y, c+p[0]);
	set_char(t, x+xw-1, y, c+p[1]);
	set_char(t, x, y+yw-1, c+p[2]);
	set_char(t, x+xw-1, y+yw-1, c+p[3]);
	fill_area(t, x, y+1, 1, yw-2, c+p[4]);
	fill_area(t, x+xw-1, y+1, 1, yw-2, c+p[4]);
	fill_area(t, x+1, y, xw-2, 1, c+p[5]);
	fill_area(t, x+1, y+yw-1, xw-2, 1, c+p[5]);
}


/* print_text() */
void print_text(struct terminal *t, int x, int y, int l,
		unsigned char *text, unsigned c)
{
	for (; l-- && *text; text++, x++) set_char(t, x, y, *text + c);
}


/* (altx,alty) is alternative location, when block_cursor terminal option is
 * set. It is usually bottom right corner of the screen. */
void set_cursor(struct terminal *term, int x, int y, int altx, int alty)
{
	struct list_head *opt_tree = (struct list_head *) term->spec->ptr;

	term->dirty = 1;
	if (get_opt_bool_tree(opt_tree, "block_cursor")) {
		x = altx;
		y = alty;
	}
	if (x >= term->x) x = term->x - 1;
	if (y >= term->y) y = term->y - 1;
	if (x < 0) x = 0;
	if (y < 0) y = 0;
	term->cx = x;
	term->cy = y;
}


/* exec_thread() */
void exec_thread(unsigned char *path, int p)
{
	int plen = strlen(path + 1) + 2;

#if defined(HAVE_SETPGID) && !defined(BEOS) && !defined(HAVE_BEGINTHREAD)
	if (path[0] == 2) setpgid(0, 0);
#endif
	exe(path + 1);
	close(p);
	if (path[plen]) unlink(path + plen);
}


/* close_handle() */
void close_handle(void *p)
{
	int h = (int)p;

	close(h);
	set_handlers(h, NULL, NULL, NULL, NULL);
}


/* unblock_terminal() */
void unblock_terminal(struct terminal *term)
{
	close_handle((void *)term->blocked);
	term->blocked = -1;
	set_handlers(term->fdin, (void (*)(void *))in_term, NULL,
		     (void (*)(void *))destroy_terminal, term);
	unblock_itrm(term->fdin);
	redraw_terminal_cls(term);
	if (textarea_editor)	/* XXX */
		textarea_edit(1, NULL, NULL, NULL, NULL, NULL);
}


/* exec_on_terminal() */
void exec_on_terminal(struct terminal *term, unsigned char *path,
		      unsigned char *delete, int fg)
{
	int plen;
	int dlen = strlen(delete);

	if (path && !*path) return;
	if (!path) {
		path = "";
		plen = 0;
	} else {
		plen = strlen(path);
	}

#ifdef NO_FG_EXEC
	fg = 0;
#endif
	if (term->master) {
		if (!*path) dispatch_special(delete);
		else {
			int blockh;
			unsigned char *param;

			if (is_blocked() && fg) {
				unlink(delete);
				return;
			}

			param = mem_alloc(plen + dlen + 3);
			if (!param) return;

			param[0] = fg;
			strcpy(param + 1, path);
			strcpy(param + 1 + plen + 1, delete);
			if (fg == 1) block_itrm(term->fdin);

			blockh = start_thread((void (*)(void *, int))exec_thread,
					      param, plen + dlen + 3);
			if (blockh == -1) {
				if (fg == 1) unblock_itrm(term->fdin);
				mem_free(param);
				return;
			}

			mem_free(param);
			if (fg == 1) {
				term->blocked = blockh;
				set_handlers(blockh,
					     (void (*)(void *)) unblock_terminal,
					     NULL,
					     (void (*)(void *)) unblock_terminal,
					     term);
				set_handlers(term->fdin, NULL, NULL,
					     (void (*)(void *)) destroy_terminal,
					     term);
				/* block_itrm(term->fdin); */
			} else {
				set_handlers(blockh, close_handle, NULL,
					     close_handle, (void *) blockh);
			}
		}
	} else {
		unsigned char *data = mem_alloc(plen + dlen + 4);

		if (data) {
			data[0] = 0;
			data[1] = fg;
			strcpy(data + 2, path);
			strcpy(data + 3 + plen, delete);
			hard_write(term->fdout, data, plen + dlen + 4);
			mem_free(data);
		}
#if 0
		char x = 0;
		hard_write(term->fdout, &x, 1);
		x = fg;
		hard_write(term->fdout, &x, 1);
		hard_write(term->fdout, path, strlen(path) + 1);
		hard_write(term->fdout, delete, strlen(delete) + 1);
#endif
	}
}


/* do_terminal_function() */
void do_terminal_function(struct terminal *term, unsigned char code,
			  unsigned char *data)
{
	unsigned char *x_data = mem_alloc(strlen(data) + 2);

	if (!x_data) return;
	x_data[0] = code;
	strcpy(x_data + 1, data);
	exec_on_terminal(term, NULL, x_data, 0);
	mem_free(x_data);
}


/* set_terminal_title() */
void set_terminal_title(struct terminal *term, unsigned char *title)
{
	if (term->title && !strcmp(title, term->title)) return;
	if (term->title) mem_free(term->title);
	term->title = stracpy(title);
	do_terminal_function(term, TERM_FN_TITLE, title);
}
