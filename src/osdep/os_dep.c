/* Features which vary with the OS */
/* $Id: os_dep.c,v 1.71 2003/06/08 10:49:28 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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

#if defined(HAVE_LIBGPM) && defined(HAVE_GPM_H)
#define USE_GPM
#endif

#ifdef USE_GPM
#include <gpm.h>
#endif

#ifdef HAVE_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif


#include "elinks.h"

#include "intl/gettext/libintl.h"
#include "osdep/os_dep.h"
#include "lowlevel/signals.h"
#include "lowlevel/select.h"
#include "terminal/kbd.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "sched/session.h"
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
is_safe_in_shell(unsigned char c)
{
	return c == '@' || c == '+' || c == '.' || c == '/' || isA(c);
}

void
check_shell_security(unsigned char **cmd)
{
	unsigned char *c = *cmd;

	while (*c) {
		if (!is_safe_in_shell(*c)) *c = '_';
		c++;
	}
}

static int
get_e(char *env)
{
	char *v = getenv(env);

	if (v) return atoi(v);

	return 0;
}

#if defined(OS2)

#define INCL_MOU
#define INCL_VIO
#define INCL_DOSPROCESS
#define INCL_DOSERRORS
#define INCL_WINCLIPBOARD
#define INCL_WINSWITCHLIST
#include <os2.h>
#include <io.h>
#include <process.h>
#include <sys/video.h>
#ifdef HAVE_SYS_FMUTEX_H
#include <sys/builtin.h>
#include <sys/fmutex.h>
#endif

#ifdef X2
/* from xf86sup - XFree86 OS/2 support driver */
#include <pty.h>
#endif

#endif


#ifdef USE_OPEN_PREALLOC
int
open_prealloc(char *name, int flags, int mode, int siz)
{
	/* This is good for OS/2, where this will prevent file fragmentation,
	 * preallocating the desired file size upon open(). */
	return open(name, flags | O_SIZE, mode, (unsigned long) siz);
}

void
prealloc_truncate(int h, int siz)
{
	ftruncate(h, siz);
}
#endif


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
		if (!(*x = ws.ws_col) && !(*x = get_e("COLUMNS"))) *x = 80;
		if (!(*y = ws.ws_row) && !(*y = get_e("LINES"))) *y = 24;
		return 0;
	} else {
		if (!(*x = get_e("COLUMNS"))) *x = 80;
		if (!(*y = get_e("LINES"))) *y = 24;
	}

	return 0;
}

#elif defined(OS2)

#define A_DECL(type, var) type var##1, var##2, *var = _THUNK_PTR_STRUCT_OK(&var##1) ? &var##1 : &var##2

int
is_xterm(void)
{
	static int xt = -1;

	if (xt == -1) xt = !!getenv("WINDOWID");

	return xt;
}

int winch_pipe[2];
int winch_thread_running = 0;

#define WINCH_SLEEPTIME 500 /* time in ms for winch thread to sleep */

void
winch_thread(void)
{
	/* A thread which regularly checks whether the size of
	   window has changed. Then raise SIGWINCH or notifiy
	   the thread responsible to handle this. */
	static int old_xsize, old_ysize;
	static int cur_xsize, cur_ysize;

	signal(SIGPIPE, SIG_IGN);
	if (get_terminal_size(0, &old_xsize, &old_ysize)) return;
	while (1) {
		if (get_terminal_size(0, &cur_xsize, &cur_ysize)) return;
		if ((old_xsize != cur_xsize) || (old_ysize != cur_ysize)) {
			old_xsize = cur_xsize;
			old_ysize = cur_ysize;
			write(winch_pipe[1], "x", 1);
			/* Resizing may take some time. So don't send a flood
                     of requests?! */
			_sleep2(2*WINCH_SLEEPTIME);
		} else {
			_sleep2(WINCH_SLEEPTIME);
		}
	}
}

void
winch(void *s)
{
	char c;

	while (can_read(winch_pipe[0]) && read(winch_pipe[0], &c, 1) == 1);
	((void (*)())s)();
}

void
handle_terminal_resize(int fd, void (*fn)())
{
	if (!is_xterm()) return;
	if (!winch_thread_running) {
		if (c_pipe(winch_pipe) < 0) return;
		winch_thread_running = 1;
		_beginthread((void (*)(void *))winch_thread, NULL, 0x32000, NULL);
	}
	set_handlers(winch_pipe[0], winch, NULL, NULL, fn);
}

void
unhandle_terminal_resize(int fd)
{
	set_handlers(winch_pipe[0], NULL, NULL, NULL, NULL);
}

int
get_terminal_size(int fd, int *x, int *y)
{
	if (!x || !y) return -1;
	if (is_xterm()) {
#ifdef X2
		/* int fd; */
		int arc;
		struct winsize win;

		/* fd = STDIN_FILENO; */
		arc = ptioctl(1, TIOCGWINSZ, &win);
		if (arc) {
/*
			debug("%d", errno);
*/
			*x = 80;
			*y = 24;
			return 0;
		}
		*y = win.ws_row;
		*x = win.ws_col;
/*
		debug("%d %d", *x, *y);
*/

		return 0;
#else
		*x = 80;
		*y = 24;
		return 0;
#endif
	} else {
		int a[2] = { 0, 0 };

		_scrsize(a);
		*x = a[0];
		*y = a[1];
		if (*x == 0) {
			*x = get_e("COLUMNS");
			if (*x == 0) *x = 80;
		}
		if (*y == 0) {
			*y = get_e("LINES");
			if (*y == 0) *y = 24;
		}
	}

	return 0;
}

#elif defined(WIN32)

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

/* Filename */

int
check_file_name(unsigned char *file)
{
	return 1;		/* !!! FIXME */
}

/* Exec */

int
can_twterm() /* Check if it make sense to call a twterm. */
{
	static int xt = -1;

	if (xt == -1) xt = !!getenv("TWDISPLAY");

	return xt;
}


#if defined(UNIX) || defined(WIN32)

int
is_xterm(void)
{
	static int xt = -1;

	if (xt == -1) xt = (getenv("DISPLAY") && *getenv("DISPLAY"));

	return xt;
}

#elif defined(BEOS)

int
is_xterm(void)
{
	return 0;
}

#elif defined(RISCOS)

int
is_xterm(void)
{
       return 1;
}

#endif

tcount resize_count = 0;

#if defined(UNIX) || defined(WIN32) || defined(BEOS) || defined(RISCOS)

#if defined(BEOS) && defined(HAVE_SETPGID)

int
exe(char *path)
{
	int s;
	int p = fork();

	if (!p) {
		setpgid(0, 0);
		system(path);
		_exit(0);
	}

	if (p > 0)
		waitpid(p, &s, 0);
	else
		return system(path);

	return 0;
}

#elif defined(WIN32)

int
exe(char *path)
{
	int r;
	unsigned char *x1 = !GETSHELL ? DEFAULT_SHELL : GETSHELL;
	unsigned char *x = *path != '"' ? " /c start /wait " : " /c start /wait \"\" ";
	unsigned char *p = malloc((strlen(x1) + strlen(x) + strlen(path)) * 2 + 1);

	if (!p) return -1;
	strcpy(p, x1);
	strcat(p, x);
	strcat(p, path);
	x = p;
	while (*x) {
		if (*x == '\\') {
			memmove(x + 1, x, strlen(x) + 1);
			x++;
		}
		x++;
	}
	r = system(p);
	free(p);

	return r;
}

#else

int
exe(char *path)
{
	return system(path);
}

#endif

char *
get_clipboard_text()	/* !!! FIXME */
{
	char *ret = mem_alloc(1);

	if (ret) ret[0] = 0;

	return ret;
}

void
set_clipboard_text(char *data)
{
	/* !!! FIXME */
}

/* Set xterm-like term window's title. */
void
set_window_title(unsigned char *title)
{
	unsigned char *s;
	int xsize, ysize;
	int j = 0;

	/* Check if we're in a xterm-like terminal. */
	if (!is_xterm()) return;

	/* Retrieve terminal dimensions. */
	if (get_terminal_size(0, &xsize, &ysize)) return;

	/* Check if terminal width is reasonnable. */
	if (xsize < 1 || xsize > 1024) return;

	/* Allocate space for title + 3 ending points + null char. */
	s = (unsigned char *) mem_alloc((xsize + 3 + 1) * sizeof(unsigned char));
	if (!s) return;

	/* Copy title to s if different from NULL */
	if (title) {
		int i;

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
	char *winid;
	Display *display;
	Window window, root, parent, *children;
	XTextProperty text_prop;
	Status status;
	unsigned int num_children;
	char *ret = NULL;

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

#elif defined(OS2)

int
exe(char *path)
{
	int flags = P_SESSION;
	int pid, ret;
	char *shell;

	shell = GETSHELL;
	if (!shell) shell = DEFAULT_SHELL;
	if (is_xterm()) flags |= P_BACKGROUND;

	pid = spawnlp(flags, shell, shell, "/c", path, NULL);
	if (pid != -1)
		waitpid(pid, &ret, 0);
	else
		ret = -1;

	return ret;
}

char *
get_clipboard_text(void)
{
	PTIB tib;
	PPIB pib;
	HAB hab;
	HMQ hmq;
	ULONG oldType;
	char *ret = 0;

	DosGetInfoBlocks(&tib, &pib);

	oldType = pib->pib_ultype;

	pib->pib_ultype = 3;

	hab = WinInitialize(0);
	if (hab != NULLHANDLE) {
		hmq = WinCreateMsgQueue(hab, 0);
		if (hmq != NULLHANDLE) {

			if (WinOpenClipbrd(hab)) {
				ULONG fmtInfo = 0;

				if (WinQueryClipbrdFmtInfo(hab, CF_TEXT, &fmtInfo)!=FALSE)
				{
					ULONG selClipText = WinQueryClipbrdData(hab, CF_TEXT);

					if (selClipText)
					{
						PCHAR pchClipText = (PCHAR)selClipText;

						ret = stracpy(pchClipText);
					}
				}

				WinCloseClipbrd(hab);
			}

			WinDestroyMsgQueue(hmq);
		}
		WinTerminate(hab);
	}

	pib->pib_ultype = oldType;

	return ret;
}

void
set_clipboard_text(char *data)
{
	PTIB tib;
	PPIB pib;
	HAB hab;
	HMQ hmq;
	ULONG oldType;

	DosGetInfoBlocks(&tib, &pib);

	oldType = pib->pib_ultype;

	pib->pib_ultype = 3;

	hab = WinInitialize(0);
	if (hab != NULLHANDLE) {
		hmq = WinCreateMsgQueue(hab, 0);
		if (hmq != NULLHANDLE) {
			if (WinOpenClipbrd(hab)) {
				PVOID pvShrObject = NULL;
				if (DosAllocSharedMem(&pvShrObject, NULL, strlen(data) + 1, PAG_COMMIT | PAG_WRITE | OBJ_GIVEABLE) == NO_ERROR) {
					strcpy(pvShrObject, data);
					WinSetClipbrdData(hab, (ULONG)pvShrObject, CF_TEXT, CFI_POINTER);
				}
				WinCloseClipbrd(hab);
			}
			WinDestroyMsgQueue(hmq);
		}
		WinTerminate(hab);
	}

	pib->pib_ultype = oldType;
}

unsigned char *
get_window_title(void)
{
#ifndef OS2_DEBUG
	char *org_switch_title;
	char *org_win_title = NULL;
	static PTIB tib = NULL;
	static PPIB pib = NULL;
	ULONG oldType;
	HSWITCH hSw = NULLHANDLE;
	SWCNTRL swData;
	HAB hab;
	HMQ hmq;

	/* save current process title */

	if (!pib) DosGetInfoBlocks(&tib, &pib);
	oldType = pib->pib_ultype;
	memset(&swData, 0, sizeof swData);
	if (hSw == NULLHANDLE) hSw = WinQuerySwitchHandle(0, pib->pib_ulpid);
	if (hSw != NULLHANDLE && !WinQuerySwitchEntry(hSw, &swData)) {
		/*org_switch_title = mem_alloc(strlen(swData.szSwtitle)+1);
		strcpy(org_switch_title, swData.szSwtitle);*/
		/* Go to PM */
		pib->pib_ultype = 3;
		hab = WinInitialize(0);
		if (hab != NULLHANDLE) {
			hmq = WinCreateMsgQueue(hab, 0);
			if (hmq != NULLHANDLE) {
				org_win_title = mem_alloc(MAXNAMEL + 1);
				if (org_win_title)
					WinQueryWindowText(swData.hwnd,
							   MAXNAMEL + 1,
							   org_win_title);
					org_win_title[MAXNAMEL] = 0;
				/* back From PM */
				WinDestroyMsgQueue(hmq);
			}
			WinTerminate(hab);
		}
		pib->pib_ultype = oldType;
	}
	return org_win_title;
#else
	return NULL;
#endif
}

void
set_window_title(unsigned char *title)
{
#ifndef OS2_DEBUG
	static PTIB tib;
	static PPIB pib;
	ULONG oldType;
	static HSWITCH hSw;
	SWCNTRL swData;
	HAB hab;
	HMQ hmq;
	char new_title[MAXNAMEL];

	if (!title) return;
	if (!pib) DosGetInfoBlocks(&tib, &pib);
	oldType = pib->pib_ultype;
	memset(&swData, 0, sizeof swData);
	if (hSw == NULLHANDLE) hSw = WinQuerySwitchHandle(0, pib->pib_ulpid);
	if (hSw != NULLHANDLE && !WinQuerySwitchEntry(hSw, &swData)) {
		char *p;

		safe_strncpy(new_title, title, MAXNAMEL - 1);
		while ((p = strchr(new_title, 1))) *p = ' ';
		safe_strncpy(swData.szSwtitle, new_title, MAXNAMEL - 1);
		WinChangeSwitchEntry(hSw, &swData);
		/* Go to PM */
		pib->pib_ultype = 3;
		hab = WinInitialize(0);
		if (hab != NULLHANDLE) {
			hmq = WinCreateMsgQueue(hab, 0);
			if (hmq != NULLHANDLE) {
				if (swData.hwnd)
					WinSetWindowText(swData.hwnd, new_title);
				/* back From PM */
				WinDestroyMsgQueue(hmq);
			}
			WinTerminate(hab);
		}
	}
	pib->pib_ultype = oldType;
#endif
}

#if 0
void
set_window_title(int init, const char *url)
{
	static char *org_switch_title;
	static char *org_win_title;
	static PTIB tib;
	static PPIB pib;
	static ULONG oldType;
	static HSWITCH hSw;
	static SWCNTRL swData;
	HAB hab;
	HMQ hmq;
	char new_title[MAXNAMEL];

	switch (init)
	{
	case 1:
		DosGetInfoBlocks( &tib, &pib );
		oldType = pib->pib_ultype;
		memset( &swData, 0, sizeof swData );
		hSw = WinQuerySwitchHandle( 0, pib->pib_ulpid );
		if ( hSw!=NULLHANDLE && !WinQuerySwitchEntry( hSw, &swData ) )
		{
			org_switch_title = mem_alloc( strlen( swData.szSwtitle )+1 );
			strcpy( org_switch_title, swData.szSwtitle );
			pib->pib_ultype = 3;
			hab = WinInitialize( 0 );
			hmq = WinCreateMsgQueue( hab, 0 );
			if ( hab!=NULLHANDLE && hmq!=NULLHANDLE )
			{
				org_win_title = mem_alloc( MAXNAMEL+1 );
				WinQueryWindowText( swData.hwnd, MAXNAMEL+1, org_win_title );
				WinDestroyMsgQueue( hmq );
				WinTerminate( hab );
			}
			pib->pib_ultype = oldType;
		}
		break;
	case -1:
		pib->pib_ultype = 3;
		hab = WinInitialize( 0 );
		hmq = WinCreateMsgQueue( hab, 0 );
		if ( hSw!=NULLHANDLE && hab!=NULLHANDLE && hmq!=NULLHANDLE )
		{
			safe_strncpy( swData.szSwtitle, org_switch_title, MAXNAMEL );
			WinChangeSwitchEntry( hSw, &swData );

			if ( swData.hwnd )
				WinSetWindowText( swData.hwnd, org_win_title );
			WinDestroyMsgQueue( hmq );
			WinTerminate( hab );
		}
		pib->pib_ultype = oldType;
		mem_free(org_switch_title);
		mem_free(org_win_title);
		break;
	case 0:
		if ( url != NULL && *url )
		{
			safe_strncpy(new_title, url, MAXNAMEL - 10);
			strcat(new_title, " - Links");
			pib->pib_ultype = 3;
			hab = WinInitialize( 0 );
			hmq = WinCreateMsgQueue( hab, 0 );
			if ( hSw!=NULLHANDLE && hab!=NULLHANDLE && hmq!=NULLHANDLE )
			{
				safe_strncpy( swData.szSwtitle, new_title, MAXNAMEL );
				WinChangeSwitchEntry( hSw, &swData );

				if ( swData.hwnd )
					WinSetWindowText( swData.hwnd, new_title );
					WinDestroyMsgQueue( hmq );
					WinTerminate( hab );
			}
			pib->pib_ultype = oldType;
		}
		break;
	}
}
#endif

int
resize_window(int x, int y)
{
	A_DECL(VIOMODEINFO, vmi);

	resize_count++;
	if (is_xterm()) return -1;
	vmi->cb = sizeof(*vmi);
	if (VioGetMode(vmi, 0)) return -1;
	vmi->col = x;
	vmi->row = y;
	if (VioSetMode(vmi, 0)) return -1;
	/*
	unsigned char cmdline[16];
	sprintf(cmdline, "mode ");
	snprint(cmdline + 5, 5, x);
	strcat(cmdline, ",");
	snprint(cmdline + strlen(cmdline), 5, y);
	*/
	return 0;
}

#endif

/* Threads */

#if defined(HAVE_BEGINTHREAD) || defined(BEOS) || defined(HAVE_PTHREADS)

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

#ifdef HAVE_PTHREADS
void *
bgpt(struct tdata *t)
{
	bgt(t);
	return NULL;
}
#endif

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

#include <be/kernel/OS.h>

int thr_sem_init = 0;
sem_id thr_sem;


struct active_thread {
	LIST_HEAD(struct active_thread);

	thread_id tid;
	void (*fn)(void *);
	void *data;
};

INIT_LIST_HEAD(active_threads);

int32
started_thr(void *data)
{
	struct active_thread *thrd = data;

	thrd->fn(thrd->data);
	if (acquire_sem(thr_sem) < B_NO_ERROR) return 0;
	del_from_list(thrd);
	free(thrd);
	release_sem(thr_sem);

	return 0;
}

int
start_thr(void (*fn)(void *), void *data, unsigned char *name)
{
	struct active_thread *thrd;
	int tid;

	if (!thr_sem_init) {
		thr_sem = create_sem(0, "thread_sem");
		if (thr_sem < B_NO_ERROR) return -1;
		thr_sem_init = 1;
	} else if (acquire_sem(thr_sem) < B_NO_ERROR) return -1;

	thrd = malloc(sizeof(struct active_thread));
	if (!thrd) goto rel;
	thrd->fn = fn;
	thrd->data = data;
	thrd->tid = spawn_thread(started_thr, name, B_NORMAL_PRIORITY, thrd);
	tid = thrd->tid;

	if (tid < B_NO_ERROR) {
		free(thrd);

rel:
		release_sem(thr_sem);
		return -1;
	}

	resume_thread(thrd->tid);
	add_to_list(active_threads, thrd);
	release_sem(thr_sem);

	return tid;
}

void
terminate_osdep(void)
{
	struct list_head *p;
	struct active_thread *thrd;

	if (acquire_sem(thr_sem) < B_NO_ERROR) return;
	foreach (thrd, active_threads) kill_thread(thrd->tid);

	while ((p = active_threads.next) != &active_threads) {
		del_from_list(p);
		free(p);
	}
	release_sem(thr_sem);
}

int
start_thread(void (*fn)(void *, int), void *ptr, int l)
{
	int p[2];
	struct tdata *t;

	if (c_pipe(p) < 0) return -1;

	t = malloc(sizeof(struct tdata) + l);
	if (!t) return -1;
	t->fn = fn;
	t->h = p[1];
	memcpy(t->data, ptr, l);
	if (start_thr((void (*)(void *))bgt, t, "elinks_thread") < 0) {
		close(p[0]);
		close(p[1]);
		mem_free(t);
		return -1;
	}

	return p[0];
}


#elif defined(HAVE_BEGINTHREAD)

int
start_thread(void (*fn)(void *, int), void *ptr, int l)
{
	int p[2];
	struct tdata *t;

	if (c_pipe(p) < 0) return -1;
	if (set_nonblocking_fd(p[0]) < 0) return -1;
	if (set_nonblocking_fd(p[1]) < 0) return -1;

	t = malloc(sizeof(struct tdata) + l);
	if (!t) return -1;
	t->fn = fn;
	t->h = p[1];
	memcpy(t->data, ptr, l);
	if (_beginthread((void (*)(void *))bgt, NULL, 65536, t) == -1) {
		close(p[0]);
		close(p[1]);
		mem_free(t);
		return -1;
	}

	return p[0];
}

#ifdef HAVE_READ_KBD

int tp = -1;
int ti = -1;

void
input_thread(void *p)
{
	char c[2];
	int h = (int)p;

	signal(SIGPIPE, SIG_IGN);
	while (1) {
		/*c[0] = _read_kbd(0, 1, 1);
		if (c[0]) if (write(h, c, 1) <= 0) break;
		else {
			int w;
			printf("1");fflush(stdout);
			c[1] = _read_kbd(0, 1, 1);
			printf("2");fflush(stdout);
			w = write(h, c, 2);
			printf("3");fflush(stdout);
			if (w <= 0) break;
			if (w == 1) if (write(h, c+1, 1) <= 0) break;
			printf("4");fflush(stdout);
		}*/
           /* for the records:
                 _read_kbd(0, 1, 1) will
                 read a char, don't echo it, wait for one available and
                 accept CTRL-C.
                 Knowing that, I suggest we replace this call completly!
            */
                *c = _read_kbd(0, 1, 1);
                write(h, c, 1);
	}
	close(h);
}
#endif /* #ifdef HAVE_READ_KBD */

#if defined(HAVE_MOUOPEN) && !defined(USE_GPM)

#define USING_OS2_MOUSE

#ifdef HAVE_SYS_FMUTEX_H
_fmutex mouse_mutex;
int mouse_mutex_init = 0;
#endif
int mouse_h = -1;

struct os2_mouse_spec {
	int p[2];
	void (*fn)(void *, unsigned char *, int);
	void *data;
	unsigned char buffer[sizeof(struct event)];
	int bufptr;
	int terminate;
};

void
mouse_thread(void *p)
{
	int status;
	struct os2_mouse_spec *oms = p;
	A_DECL(HMOU, mh);
	A_DECL(MOUEVENTINFO, ms);
	A_DECL(USHORT, rd);
	A_DECL(USHORT, mask);
	struct event ev;

	signal(SIGPIPE, SIG_IGN);
	ev.ev = EV_MOUSE;
	if (MouOpen(NULL, mh)) goto ret;
	mouse_h = *mh;
	*mask = MOUSE_MOTION_WITH_BN1_DOWN | MOUSE_BN1_DOWN |
		MOUSE_MOTION_WITH_BN2_DOWN | MOUSE_BN2_DOWN |
		MOUSE_MOTION_WITH_BN3_DOWN | MOUSE_BN3_DOWN |
		MOUSE_MOTION;
	MouSetEventMask(mask, *mh);
	*rd = MOU_WAIT;
	status = -1;

	while (1) {
		int w, ww;

		if (MouReadEventQue(ms, rd, *mh)) break;
#ifdef HAVE_SYS_FMUTEX_H
		_fmutex_request(&mouse_mutex, _FMR_IGNINT);
#endif
		if (!oms->terminate) MouDrawPtr(*mh);
#ifdef HAVE_SYS_FMUTEX_H
		_fmutex_release(&mouse_mutex);
#endif
		ev.x = ms->col;
		ev.y = ms->row;
		/*debug("status: %d %d %d", ms->col, ms->row, ms->fs);*/
		if (ms->fs & (MOUSE_BN1_DOWN | MOUSE_BN2_DOWN | MOUSE_BN3_DOWN))
			ev.b = status = B_DOWN | (ms->fs & MOUSE_BN1_DOWN ? B_LEFT : ms->fs & MOUSE_BN2_DOWN ? B_MIDDLE : B_RIGHT);
		else if (ms->fs & (MOUSE_MOTION_WITH_BN1_DOWN | MOUSE_MOTION_WITH_BN2_DOWN | MOUSE_MOTION_WITH_BN3_DOWN)) {
			int b = ms->fs & MOUSE_MOTION_WITH_BN1_DOWN ? B_LEFT : ms->fs & MOUSE_MOTION_WITH_BN2_DOWN ? B_MIDDLE : B_RIGHT;

			if (status == -1)
				b |= B_DOWN;
			else
				b |= B_DRAG;
			ev.b = status = b;
		} else {
			if (status == -1) continue;
			ev.b = (status & BM_BUTT) | B_UP;
			status = -1;
		}
		if (hard_write(oms->p[1], (unsigned char *)&ev, sizeof(struct event)) != sizeof(struct event)) break;
	}
#ifdef HAVE_SYS_FMUTEX_H
	_fmutex_request(&mouse_mutex, _FMR_IGNINT);
#endif
	mouse_h = -1;
	MouClose(*mh);
#ifdef HAVE_SYS_FMUTEX_H
	_fmutex_release(&mouse_mutex);
#endif

ret:
	close(oms->p[1]);
	/*free(oms);*/
}

void
mouse_handle(struct os2_mouse_spec *oms)
{
	int r = read(oms->p[0], oms->buffer + oms->bufptr,
		     sizeof(struct event) - oms->bufptr);

	if (r <= 0) {
		unhandle_mouse(oms);
		return;
	}

	oms->bufptr += r;
	if (oms->bufptr == sizeof(struct event)) {
		oms->bufptr = 0;
		oms->fn(oms->data, oms->buffer, sizeof(struct event));
	}
}

void *
handle_mouse(int cons, void (*fn)(void *, unsigned char *, int),
	     void *data)
{
	struct os2_mouse_spec *oms;

	if (is_xterm()) return NULL;
#ifdef HAVE_SYS_FMUTEX_H
	if (!mouse_mutex_init) {
		if (_fmutex_create(&mouse_mutex, 0)) return NULL;
		mouse_mutex_init = 1;
	}
#endif
		/* This is never freed but it's allocated only once */
	oms = malloc(sizeof(struct os2_mouse_spec));
	if (!oms) return NULL;
	oms->fn = fn;
	oms->data = data;
	oms->bufptr = 0;
	oms->terminate = 0;
	if (c_pipe(oms->p)) {
		free(oms);
		return NULL;
	}
	_beginthread(mouse_thread, NULL, 0x10000, (void *)oms);
	set_handlers(oms->p[0], (void (*)(void *))mouse_handle, NULL, NULL, oms);

	return oms;
}

void
unhandle_mouse(void *om)
{
	struct os2_mouse_spec *oms = om;

	want_draw();
	oms->terminate = 1;
	set_handlers(oms->p[0], NULL, NULL, NULL, NULL);
	close(oms->p[0]);
	done_draw();
}

void
want_draw(void)
{
	A_DECL(NOPTRRECT, pa);
#ifdef HAVE_SYS_FMUTEX_H
	if (mouse_mutex_init) _fmutex_request(&mouse_mutex, _FMR_IGNINT);
#endif
	if (mouse_h != -1) {
		static int x = -1, y = -1;
		static tcount c = -1;

		if (x == -1 || y == -1 || (c != resize_count)) {
			get_terminal_size(1, &x, &y);
			c = resize_count;
		}

		pa->row = 0;
		pa->col = 0;
		pa->cRow = y - 1;
		pa->cCol = x - 1;
		MouRemovePtr(pa, mouse_h);
	}
}

void
done_draw(void)
{
#ifdef HAVE_SYS_FMUTEX_H
	if (mouse_mutex_init) _fmutex_release(&mouse_mutex);
#endif
}

#endif /* if HAVE_MOUOPEN */

#elif defined(HAVE_CLONE)

/* This is maybe buggy... */

#include <sched.h>

struct thread_stack {
	LIST_HEAD(struct thread_stack);

	int pid;
	void *stack;
	void (*fn)(void *, int);
	int h;
	int l;
	unsigned char data[1];
};

void
bglt(struct thread_stack *ts)
{
	ts->fn(ts->data, ts->h);
	write(ts->h, "x", 1);
	close(ts->h);
}

INIT_LIST_HEAD(thread_stacks);

int
start_thread(void (*fn)(void *, int), void *ptr, int l)
{
	struct thread_stack *ts;
	int p[2];
	int f;

	if (c_pipe(p) < 0) return -1;
	if (set_nonblocking_fd(p[0]) < 0) return -1;
	if (set_nonblocking_fd(p[1]) < 0) return -1;

	/*if (!(t = malloc(sizeof(struct tdata) + l))) return -1;
	t->fn = fn;
	t->h = p[1];
	memcpy(t->data, ptr, l);*/
	foreach (ts, thread_stacks) {
		if (ts->pid == -1 || kill(ts->pid, 0)) {
			if (ts->l >= l) {
				goto ts_ok;
			} else {
				struct thread_stack *tts = ts;

				ts = ts->prev;
				del_from_list(tts); free(tts->stack); free(tts);
			}
		}
	}

	ts = malloc(sizeof(struct thread_stack) + l);
	if (!ts) goto fail;

	ts->stack = malloc(0x10000);
	if (!ts->stack) {
		free(ts);
		goto fail;
	}

	ts->l = l;
	add_to_list(thread_stacks, ts);

ts_ok:
	ts->fn = fn;
	ts->h = p[1];
	memcpy(ts->data, ptr, l);

	ts->pid = __clone((int (*)(void *))bglt, (char *)ts->stack + 0x8000,
			  CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD,
			  ts);
	if (ts->pid == -1) {

fail:
		close(p[0]);
		close(p[1]);
		return -1;
	}

	return p[0];
}

#elif defined(HAVE_PTHREADS)

#include <pthread.h>

int
start_thread(void (*fn)(void *, int), void *ptr, int l)
{
	pthread_t thread;
	struct tdata *t;
	int p[2];
	int f;

	if (c_pipe(p) < 0) return -1;
	if (set_nonblocking_fd(p[0]) < 0) return -1;
	if (set_nonblocking_fd(p[1]) < 0) return -1;

	t = malloc(sizeof(struct tdata) + l);
	if (!t) return -1;

	t->fn = fn;
	t->h = p[1];
	memcpy(t->data, ptr, l);
	if (pthread_create(&thread, NULL, (void *(*)(void *))bgpt, t)) {
		close(p[0]);
		close(p[1]);
		mem_free(t);
		return -1;
	}

	return p[0];
}

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

#ifndef USING_OS2_MOUSE
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

#if defined(OS2)

int
get_ctl_handle(void)
{
	return get_input_handle();
}

#else

int
get_ctl_handle(void)
{
	return 0;
}

#endif

#if defined(BEOS)

#elif defined(HAVE_BEGINTHREAD) && defined(HAVE_READ_KBD)
int
get_input_handle(void)
{
	int fd[2];

	if (ti != -1) return ti;
	if (is_xterm()) return 0;
	if (c_pipe(fd) < 0) return 0;
	ti = fd[0];
	tp = fd[1];
	_beginthread(input_thread, NULL, 0x10000, (void *)tp);
/*
#if defined(HAVE_MOUOPEN) && !defined(USE_GPM)
	_beginthread(mouse_thread, NULL, 0x10000, (void *)tp);
#endif
*/
	return fd[0];
}

#elif defined(WIN32)

void input_function(int fd);
void set_proc_id(int id);

int
get_input_handle(void)
{
	int	fd[2];
	static int ti = -1, tp = -1;
	int	pid;

	if (ti != -1) return ti;
	if (c_pipe (fd) < 0) return 0;
	ti = fd[0];
	tp = fd[1];

	pid = fork();
	if (!pid)
		input_function (tp);
	else
		set_proc_id (pid);

	return ti;
}

#else

int
get_input_handle(void)
{
	return 0;
}

#endif /* defined(HAVE_BEGINTHREAD) && defined(HAVE_READ_KBD) */


#ifndef HAVE_CFMAKERAW
void
cfmakeraw(struct termios *t)
{
	t->c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
	t->c_oflag &= ~OPOST;
	t->c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
	t->c_cflag &= ~(CSIZE|PARENB);
	t->c_cflag |= CS8;
	t->c_cc[VMIN] = 1;
	t->c_cc[VTIME] = 0;
}
#endif

#ifdef USE_GPM

struct gpm_mouse_spec {
	int h;
	void (*fn)(void *, unsigned char *, int);
	void *data;
};

static void
gpm_mouse_in(struct gpm_mouse_spec *gms)
{
	Gpm_Event gev;
	struct event ev;

	if (Gpm_GetEvent(&gev) <= 0) {
		set_handlers(gms->h, NULL, NULL, NULL, NULL);
		return;
	}

	ev.ev = EV_MOUSE;
	ev.x = gev.x - 1;
	ev.y = gev.y - 1;
	if (gev.buttons & GPM_B_LEFT)
		ev.b = B_LEFT;
	else if (gev.buttons & GPM_B_MIDDLE)
		ev.b = B_MIDDLE;
	else if (gev.buttons & GPM_B_RIGHT)
		ev.b = B_RIGHT;
	else
		return;

	if (gev.type & GPM_DOWN)
		ev.b |= B_DOWN;
	else if (gev.type & GPM_UP)
		ev.b |= B_UP;
	else if (gev.type & GPM_DRAG)
		ev.b |= B_DRAG;
	else
		return;

	gms->fn(gms->data, (char *)&ev, sizeof(struct event));
}

void *
handle_mouse(int cons, void (*fn)(void *, unsigned char *, int),
	     void *data)
{
	int h;
	Gpm_Connect conn;
	struct gpm_mouse_spec *gms;

	conn.eventMask = ~GPM_MOVE;
	conn.defaultMask = GPM_MOVE;
	conn.minMod = 0;
	conn.maxMod = 0;

	h = Gpm_Open(&conn, cons);
	if (h < 0) return NULL;

	gms = mem_alloc(sizeof(struct gpm_mouse_spec));
	if (!gms) return NULL;
	gms->h = h;
	gms->fn = fn;
	gms->data = data;
	set_handlers(h, (void (*)(void *))gpm_mouse_in, NULL, NULL, gms);

	return gms;
}

void
unhandle_mouse(void *h)
{
	struct gpm_mouse_spec *gms = h;

	if (!gms) return;

	set_handlers(gms->h, NULL, NULL, NULL, NULL);
	Gpm_Close();
	mem_free(gms);
}

#elif !defined(USING_OS2_MOUSE)

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

#endif /* #ifdef USE_GPM */

/* Create a bitmask consisting from system-independent envirnoment modifiers.
 * This is then complemented by system-specific modifiers in an appropriate
 * get_system_env() routine. */
static int
get_common_env(void)
{
	int env = 0;

	/* TODO: This should be named uniformly ;-). --pasky */
	if (is_xterm()) env |= ENV_XWIN;
	if (can_twterm()) env |= ENV_TWIN;
	if (getenv("STY")) env |= ENV_SCREEN;

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

int
get_system_env(void)
{
	int env = get_common_env();

	/* !!! FIXME: telnet */
	if (!is_xterm()) env |= ENV_OS2VIO;

	return env;
}

#elif defined(BEOS)

int
get_system_env(void)
{
	int env = get_common_env();
	unsigned char *term = getenv("TERM");

	if (!term || (upcase(term[0]) == 'B' && upcase(term[1]) == 'E'))
		env |= ENV_BE;

	return env;
}

#elif defined(WIN32)

int
get_system_env(void)
{
	return get_common_env() | ENV_WIN32;
}

#else

int
get_system_env(void)
{
	return get_common_env();
}

#endif

static void
exec_new_elinks(struct terminal *term, unsigned char *xterm,
		unsigned char *exe_name, unsigned char *param)
{
	unsigned char *str = straconcat(xterm, " ", exe_name, " ", param, NULL);

	if (!str) return;
	exec_on_terminal(term, str, "", 2);
	mem_free(str);
}

static void
open_in_new_tab(struct terminal *term, unsigned char *exe_name,
                unsigned char *param)
{
	struct window *tab;
	struct initial_session_info *info;
	struct event ev = {EV_INIT, 0, 0, 0};

	tab = init_tab(term);
	if (!tab) return;

	info = mem_calloc(1, sizeof(struct initial_session_info));
	if (!info) {
		mem_free(tab);
		return;
	}
	info->base_session = -1;

	/* FIXME: This param parsing is way too ugly (and could prepare us some
	 * nice surprises in the future) to survive in the codebase. We should
	 * call some common creating the commandline directly in the open_in_..
	 * function. --pasky */

	if (!strncmp(param, "-base-session ", 13)) {
		info->base_session = atoi(param + strlen("-base-session "));
	} else {
		info->url = decode_shell_safe_url(param);
	}

	ev.b = (long) info;
	tab->handler(tab, &ev, 0);
}

static void
open_in_new_twterm(struct terminal *term, unsigned char *exe_name,
		   unsigned char *param)
{
	unsigned char *twterm;

	twterm = getenv("ELINKS_TWTERM");
	if (!twterm) twterm = getenv("LINKS_TWTERM");
	if (!twterm) twterm = "twterm -e";
	exec_new_elinks(term, twterm, exe_name, param);
}

static void
open_in_new_xterm(struct terminal *term, unsigned char *exe_name,
		  unsigned char *param)
{
	unsigned char *xterm;

	xterm = getenv("ELINKS_XTERM");
	if (!xterm) xterm = getenv("LINKS_XTERM");
	if (!xterm) xterm = "xterm -e";
	exec_new_elinks(term, xterm, exe_name, param);
}

static void
open_in_new_screen(struct terminal *term, unsigned char *exe_name,
		   unsigned char *param)
{
	exec_new_elinks(term, "screen", exe_name, param);
}

#ifdef OS2
void
open_in_new_vio(struct terminal *term, unsigned char *exe_name,
		unsigned char *param)
{
	exec_new_elinks(term, "cmd /c start /c /f /win", exe_name, param);
}

void
open_in_new_fullscreen(struct terminal *term, unsigned char *exe_name,
		       unsigned char *param)
{
	exec_new_elinks(term, "cmd /c start /c /f /fs", exe_name, param);
}
#endif

#ifdef WIN32
void
open_in_new_win32(struct terminal *term, unsigned char *exe_name,
		  unsigned char *param)
{
	exec_new_elinks(term, "", exe_name, param);
}
#endif

#ifdef BEOS
void
open_in_new_be(struct terminal *term, unsigned char *exe_name,
	       unsigned char *param)
{
	exec_new_elinks(term, "Terminal", exe_name, param);
}
#endif

struct {
	enum term_env_type env;
	void (*fn)(struct terminal *term, unsigned char *, unsigned char *);
	unsigned char *text;
} oinw[] = {
	{ENV_CONSOLE, open_in_new_tab, N_("~Tab")},
	{ENV_XWIN, open_in_new_xterm, N_("~Xterm")},
	{ENV_TWIN, open_in_new_twterm, N_("T~wterm")},
	{ENV_SCREEN, open_in_new_screen, N_("~Screen")},
#ifdef OS2
	{ENV_OS2VIO, open_in_new_vio, N_("~Window")},
	{ENV_OS2VIO, open_in_new_fullscreen, N_("~Full screen")},
#endif
#ifdef WIN32
	{ENV_WIN32, open_in_new_win32, N_("~Window")},
#endif
#ifdef BEOS
	{ENV_BE, open_in_new_be, N_("~BeOS terminal")},
#endif
	{0, NULL, NULL}
};

struct open_in_new *
get_open_in_new(int environment)
{
	int i;
	struct open_in_new *oin = NULL;
	int noin = 0;

	for (i = 0; oinw[i].env; i++) {
		struct open_in_new *x;

		if (!(environment & oinw[i].env))
			continue;

		x = mem_realloc(oin, (noin + 2) * sizeof(struct open_in_new));
		if (!x) continue;

		oin = x;
		oin[noin].text = oinw[i].text;
		oin[noin].fn = oinw[i].fn;
		noin++;
		oin[noin].text = NULL;
		oin[noin].fn = NULL;
	}

	return oin;
}

/* Returns:
 * 0 if it is impossible to open anything in anything new
 * 1 if there is one possible object capable of being spawn
 * >1 if there is >1 such available objects (it may not be the actual number of
 *    them, though) */
int
can_open_in_new(struct terminal *term)
{
	struct open_in_new *oin = get_open_in_new(term->environment);

	if (!oin) return 0;
	if (!oin[1].text) {
		mem_free(oin);
		return 1;
	}
	mem_free(oin);
	return 2;
}

int
can_resize_window(int environment)
{
	if (environment & ENV_OS2VIO) return 1;
	return 0;
}

int
can_open_os_shell(int environment)
{
#ifdef OS2
	if (environment & ENV_XWIN) return 0;
#endif
	return 1;
}

#ifndef OS2
void
set_highpri(void)
{
}

#else
void
set_highpri(void)
{
	DosSetPriority(PRTYS_PROCESS, PRTYC_FOREGROUNDSERVER, 0, 0);
}
#endif

unsigned char *
get_system_str(int xwin)
{
	unsigned char *system_str;

	system_str = stracpy(SYSTEM_STR);
	if (!system_str) return NULL;

	if (xwin) add_to_strn(&system_str, "-xwin");

	return system_str;
}
