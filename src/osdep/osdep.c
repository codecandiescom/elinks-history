/* Features which vary with the OS */
/* $Id: osdep.c,v 1.127 2004/05/25 17:39:20 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#ifdef HAVE_IO_H
#include <io.h> /* For win32 && set_bin(). */
#endif
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_SIGNAL_H
#include <sys/signal.h>
#endif
#include <sys/types.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif
#include <termios.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif


#include "elinks.h"

#include "lowlevel/select.h"
#include "lowlevel/signals.h"
#include "osdep/osdep.h"
#include "sched/session.h"
#include "terminal/kbd.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"


/* Set a file descriptor to non-blocking mode. It returns a non-zero value
 * on error. */
int
set_nonblocking_fd(int fd)
{
#if defined(O_NONBLOCK) || defined(O_NDELAY)
	int flags = fcntl(fd, F_GETFL, 0);

	if (flags < 0) return -1;
#if defined(O_NONBLOCK)
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
	return fcntl(fd, F_SETFL, flags | O_NDELAY);
#endif

#elif defined(FIONBIO)
	int flag = 1;

	return ioctl(fd, FIONBIO, &flag);
#else
	return 0;
#endif
}

/* Set a file descriptor to blocking mode. It returns a non-zero value on
 * error. */
int
set_blocking_fd(int fd)
{
#if defined(O_NONBLOCK) || defined(O_NDELAY)
	int flags = fcntl(fd, F_GETFL, 0);

	if (flags < 0) return -1;
#if defined(O_NONBLOCK)
	return fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
#else
	return fcntl(fd, F_SETFL, flags & ~O_NDELAY);
#endif

#elif defined(FIONBIO)
	int flag = 0;

	return ioctl(fd, FIONBIO, &flag);
#else
	return 0;
#endif
}

int
get_e(unsigned char *env)
{
	char *v = getenv(env);

	return (v ? atoi(v) : 0);
}

unsigned char *
get_cwd(void)
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

void
set_cwd(unsigned char *path)
{
	if (path) while (chdir(path) && errno == EINTR);
}



/* Terminal size */

#if defined(UNIX) || defined(BEOS) || defined(RISCOS) || defined(WIN32)

static void
sigwinch(void *s)
{
	((void (*)(void))s)();
}

void
handle_terminal_resize(int fd, void (*fn)(void))
{
	install_signal_handler(SIGWINCH, sigwinch, fn, 0);
}

void
unhandle_terminal_resize(int fd)
{
	install_signal_handler(SIGWINCH, NULL, NULL, 0);
}

int
get_terminal_size(int fd, int *x, int *y)
{
	struct winsize ws;

	if (!x || !y) return -1;
	if (ioctl(1, TIOCGWINSZ, &ws) != -1) {
		*x = ws.ws_col;
		*y = ws.ws_row;
	}
	if (!*x) {
		*x = get_e("COLUMNS");
		if (!*x) *x = 80;
	}
	if (!*y) {
		*y = get_e("LINES");
		if (!*y) *y = 24;
	}

	return 0;
}

#endif

/* Pipe */

#if defined(UNIX) || defined(BEOS) || defined(RISCOS)

void
set_bin(int fd)
{
}

int
c_pipe(int *fd)
{
	return pipe(fd);
}

#elif defined(OS2) || defined(WIN32)

void
set_bin(int fd)
{
	setmode(fd, O_BINARY);
}

int
c_pipe(int *fd)
{
	int r = pipe(fd);

	if (!r) {
		set_bin(fd[0]);
		set_bin(fd[1]);
	}

	return r;
}

#endif

/* Exec */

int
is_twterm(void) /* Check if it make sense to call a twterm. */
{
	static int tw = -1;

	if (tw == -1) tw = !!getenv("TWDISPLAY");

	return tw;
}

int
is_gnuscreen(void)
{
	static int screen = -1;

	if (screen == -1) screen = !!getenv("STY");

	return screen;
}


#if defined(UNIX) || defined(WIN32)

int
is_xterm(void)
{
	static int xt = -1;

	if (xt == -1) {
		unsigned char *display = getenv("DISPLAY");
		unsigned char *windowid = getenv("WINDOWID");

		if (!windowid || !*windowid)
			windowid = getenv("KONSOLE_DCOP_SESSION");
		xt = (display && *display && windowid && *windowid);
	}

	return xt;
}

#endif

unsigned int resize_count = 0;

#if defined(UNIX) || defined(WIN32) || defined(BEOS) || defined(RISCOS)

#if defined(BEOS) && defined(HAVE_SETPGID)

#elif defined(WIN32)

#else

int
exe(unsigned char *path)
{
	return system(path);
}

#endif

unsigned char *
get_clipboard_text(void)	/* !!! FIXME */
{
	unsigned char *ret = mem_alloc(1);

	if (ret) ret[0] = 0;

	return ret;
}

void
set_clipboard_text(unsigned char *data)
{
	/* GNU Screen's clipboard */
	if (is_gnuscreen()) {
		struct string str;

		if (!init_string(&str)) return;

		add_to_string(&str, "screen -X register . '");
		for (; *data; ++data)
			if (*data == '\'')
				add_to_string(&str, "'\\''");
			else
				add_char_to_string(&str, *data);
		add_char_to_string(&str, '\'');

		if (str.length) exe(str.source);
		if (str.source) done_string(&str);
	}

	/* TODO: internal clipboard */
}

/* Set xterm-like term window's title. */
void
set_window_title(unsigned char *title)
{
	unsigned char *s;
	int xsize, ysize;
	register int j = 0;

	/* Check if we're in a xterm-like terminal. */
	if (!is_xterm() && !is_gnuscreen()) return;

	/* Retrieve terminal dimensions. */
	if (get_terminal_size(0, &xsize, &ysize)) return;

	/* Check if terminal width is reasonnable. */
	if (xsize < 1 || xsize > 1024) return;

	/* Allocate space for title + 3 ending points + null char. */
	s = (unsigned char *) mem_alloc((xsize + 3 + 1) * sizeof(unsigned char));
	if (!s) return;

	/* Copy title to s if different from NULL */
	if (title) {
		register int i;

		/* We limit title length to terminal width and ignore control
		 * chars if any. Note that in most cases window decoration
		 * reduces printable width, so it's just a precaution. */
		for (i = 0; title[i] && i < xsize; i++) {
			if (title[i] >= ' ')
				s[j++] = title[i];
			else
				s[j++] = ' ';
		}

		/* If title is truncated, add "..." */
		if (i == xsize) {
			s[j++] = '.';
			s[j++] = '.';
			s[j++] = '.';
		}
	}
	s[j] = '\0';

	/* Send terminal escape sequence + title string */
	printf("\033]0;%s\a", s);
	fflush(stdout);

	mem_free(s);
}

#ifdef HAVE_X11
static int x_error = 0;

static int
catch_x_error(void)
{
	x_error = 1;
	return 0;
}
#endif

unsigned char *
get_window_title(void)
{
#ifdef HAVE_X11
	/* Following code is stolen from our beloved vim. */
	unsigned char *winid;
	Display *display;
	Window window, root, parent, *children;
	XTextProperty text_prop;
	Status status;
	unsigned int num_children;
	unsigned char *ret = NULL;

	if (!is_xterm())
		return NULL;

	winid = getenv("WINDOWID");
	if (!winid)
		return NULL;
	window = (Window) atol(winid);
	if (!window)
		return NULL;

	display = XOpenDisplay(NULL);
	if (!display)
		return NULL;

	/* If WINDOWID is bad, we don't want X to abort us. */
	x_error = 0;
	XSetErrorHandler((int (*)(Display *, XErrorEvent *))catch_x_error);

	status = XGetWMName(display, window, &text_prop);
	/* status = XGetWMIconName(x11_display, x11_window, &text_prop); */
	while (!x_error && (!status || !text_prop.value)) {
		if (!XQueryTree(display, window, &root, &parent, &children, &num_children))
			break;
		if (children)
			XFree((void *) children);
		if (parent == root || parent == 0)
			break;
		window = parent;
		status = XGetWMName(display, window, &text_prop);
	}

	if (!x_error && status && text_prop.value) {
		ret = stracpy(text_prop.value);
		XFree(text_prop.value);
	}

	XCloseDisplay(display);

	return ret;
#else
	return NULL;
#endif
}

int
resize_window(int x, int y)
{
	return -1;
}

#endif

/* Threads */

#if defined(HAVE_BEGINTHREAD) || defined(BEOS)

struct tdata {
	void (*fn)(void *, int);
	int h;
	unsigned char data[1];
};

void
bgt(struct tdata *t)
{
	signal(SIGPIPE, SIG_IGN);
	t->fn(t->data, t->h);
	write(t->h, "x", 1);
	close(t->h);
	free(t);
}

#endif

#if defined(UNIX) || defined(OS2) || defined(RISCOS)

void
terminate_osdep(void)
{
}

#endif

#ifndef BEOS

void
block_stdin(void)
{
}

void
unblock_stdin(void)
{
}

#endif

#if defined(BEOS)

#elif defined(HAVE_BEGINTHREAD)

#else /* HAVE_BEGINTHREAD */

int
start_thread(void (*fn)(void *, int), void *ptr, int l)
{
	int p[2];
	int f;

	if (c_pipe(p) < 0) return -1;
	if (set_nonblocking_fd(p[0]) < 0) return -1;
	if (set_nonblocking_fd(p[1]) < 0) return -1;

	f = fork();
	if (!f) {
		struct terminal *term;

		/* Close input in this thread; otherwise, if it will live
		 * longer than its parent, it'll block the terminal until it'll
		 * quit as well; this way it will hopefully just die unseen and
		 * in background, causing no trouble. */
		/* Particularly, when async dns resolving was in progress and
		 * someone quitted ELinks, it could make a delay before the
		 * terminal would be really freed and returned to shell. */
		foreach (term, terminals)
			if (term->fdin > 0)
				close(term->fdin);

		close(p[0]);
		fn(ptr, p[1]);
		write(p[1], "x", 1);
		close(p[1]);
		/* We use _exit() here instead of exit(), see
		 * http://www.erlenstar.demon.co.uk/unix/faq_2.html#SEC6 for
		 * reasons. Fixed by Sven Neumann <sven@convergence.de>. */
		_exit(0);
	}
	if (f == -1) {
		close(p[0]);
		close(p[1]);
		return -1;
	}

	close(p[1]);
	return p[0];
}

#endif

#ifndef OS2_MOUSE
void
want_draw(void)
{
}

void
done_draw(void)
{
}
#endif

int
get_output_handle(void)
{
	return 1;
}

int
get_ctl_handle()
{
	static int fd = -1;

	if (isatty(0)) return 0;
	if (fd < 0) fd = open("/dev/tty", O_RDONLY);
	return fd;
}

#if defined(BEOS)

#elif defined(HAVE_BEGINTHREAD) && defined(HAVE_READ_KBD)

#elif defined(WIN32)

#else

int
get_input_handle(void)
{
	static int fd = -1;

	if (isatty(0)) return 0;
	if (fd < 0) fd = open("/dev/tty", O_RDONLY);
	return fd;
}

#endif /* defined(HAVE_BEGINTHREAD) && defined(HAVE_READ_KBD) */


void
elinks_cfmakeraw(struct termios *t)
{
#ifdef HAVE_CFMAKERAW
	cfmakeraw(t);
#ifdef VMIN
	t->c_cc[VMIN] = 1; /* cfmakeraw() is broken on AIX --mikulas */
#endif
#else
	t->c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
	t->c_oflag &= ~OPOST;
	t->c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
	t->c_cflag &= ~(CSIZE|PARENB);
	t->c_cflag |= CS8;
	t->c_cc[VMIN] = 1;
	t->c_cc[VTIME] = 0;
#endif
}

#if defined(CONFIG_GPM) && defined(CONFIG_MOUSE)


#elif !defined(OS2_MOUSE)

void *
handle_mouse(int cons, void (*fn)(void *, unsigned char *, int),
	     void *data)
{
	return NULL;
}

void
unhandle_mouse(void *data)
{
}

void
suspend_mouse(void *data)
{
}

void
resume_mouse(void *data)
{
}

#endif /* #ifdef CONFIG_GPM && CONFIG_MOUSE */

/* Create a bitmask consisting from system-independent envirnoment modifiers.
 * This is then complemented by system-specific modifiers in an appropriate
 * get_system_env() routine. */
static int
get_common_env(void)
{
	int env = 0;

	if (is_xterm()) env |= ENV_XWIN;
	if (is_twterm()) env |= ENV_TWIN;
	if (is_gnuscreen()) env |= ENV_SCREEN;

	/* ENV_CONSOLE is always set now and indicates that we are working w/ a
	 * displaying-capable character-adressed terminal. Sounds purely
	 * theoretically now, but it already makes some things easier and it
	 * could give us interesting opportunities later (after graphical
	 * frontends will be introduced, when working in some mysterious daemon
	 * mode or who knows what ;). --pasky */
	env |= ENV_CONSOLE;

	return env;
}

#if defined(OS2)

#elif defined(BEOS)

#elif defined(WIN32)

#else

int
get_system_env(void)
{
	return get_common_env();
}

#endif

int
can_resize_window(int environment)
{
	return !!(environment & ENV_OS2VIO);
}

#ifndef OS2
int
can_open_os_shell(int environment)
{
	if (environment & ENV_XWIN) return 0;
	return 1;
}

void
set_highpri(void)
{
}
#endif

unsigned char *
get_system_str(int xwin)
{
	return xwin ? SYSTEM_STR "-xwin" : SYSTEM_STR;
}
