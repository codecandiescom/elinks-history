/* OS/2 support fo ELinks. It has pretty different life than rest of ELinks. */
/* $Id: os2.c,v 1.2 2003/10/27 01:12:32 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef OS2

#ifdef X2
/* from xf86sup - XFree86 OS/2 support driver */
#include <pty.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "elinks.h"

#include "osdep/os_depx.h"


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
			 * of requests?! */
			_sleep2(2 * WINCH_SLEEPTIME);
		} else {
			_sleep2(WINCH_SLEEPTIME);
		}
	}
}

void
winch(void *s)
{
	unsigned char c;

	while (can_read(winch_pipe[0]) && safe_read(winch_pipe[0], &c, 1) == 1);
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
		int a[2] = {0, 0};

		_scrsize(a);
		*x = a[0];
		*y = a[1];
		if (!*x) {
			*x = get_e("COLUMNS");
			if (!*x) *x = 80;
		}
		if (!*y) {
			*y = get_e("LINES");
			if (!*y) *y = 24;
		}
	}

	return 0;
}


int
exe(unsigned char *path)
{
	int flags = P_SESSION;
	int pid;
	int ret = -1;
	unsigned char *shell;

	shell = GETSHELL;
	if (!shell) shell = DEFAULT_SHELL;
	if (is_xterm()) flags |= P_BACKGROUND;

	pid = spawnlp(flags, shell, shell, "/c", path, NULL);
	if (pid != -1)
		waitpid(pid, &ret, 0);

	return ret;
}

unsigned char *
get_clipboard_text(void)
{
	PTIB tib;
	PPIB pib;
	HAB hab;
	HMQ hmq;
	ULONG oldType;
	unsigned char *ret = 0;

	DosGetInfoBlocks(&tib, &pib);

	oldType = pib->pib_ultype;

	pib->pib_ultype = 3;

	hab = WinInitialize(0);
	if (hab != NULLHANDLE) {
		hmq = WinCreateMsgQueue(hab, 0);
		if (hmq != NULLHANDLE) {
			if (WinOpenClipbrd(hab)) {
				ULONG fmtInfo = 0;

				if (WinQueryClipbrdFmtInfo(hab, CF_TEXT, &fmtInfo) != FALSE) {
					ULONG selClipText = WinQueryClipbrdData(hab, CF_TEXT);

					if (selClipText) {
						PCHAR pchClipText = (PCHAR) selClipText;

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
set_clipboard_text(unsigned char *data)
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
	unsigned char *org_switch_title;
	unsigned char *org_win_title = NULL;
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
	unsigned char new_title[MAXNAMEL];

	if (!title) return;
	if (!pib) DosGetInfoBlocks(&tib, &pib);
	oldType = pib->pib_ultype;
	memset(&swData, 0, sizeof swData);
	if (hSw == NULLHANDLE) hSw = WinQuerySwitchHandle(0, pib->pib_ulpid);
	if (hSw != NULLHANDLE && !WinQuerySwitchEntry(hSw, &swData)) {
		unsigned char *p;

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

	switch (init) {
	case 1:
		DosGetInfoBlocks(&tib, &pib);
		oldType = pib->pib_ultype;
		memset(&swData, 0, sizeof swData);
		hSw = WinQuerySwitchHandle(0, pib->pib_ulpid);
		if (hSw != NULLHANDLE && !WinQuerySwitchEntry(hSw, &swData)) {
			org_switch_title = mem_alloc(strlen(swData.szSwtitle) + 1);
			strcpy(org_switch_title, swData.szSwtitle);
			pib->pib_ultype = 3;
			hab = WinInitialize(0);
			hmq = WinCreateMsgQueue(hab, 0);
			if (hab != NULLHANDLE && hmq != NULLHANDLE) {
				org_win_title = mem_alloc(MAXNAMEL + 1);
				WinQueryWindowText(swData.hwnd, MAXNAMEL + 1, org_win_title);
				WinDestroyMsgQueue(hmq);
				WinTerminate(hab);
			}
			pib->pib_ultype = oldType;
		}
		break;
	case -1:
		pib->pib_ultype = 3;
		hab = WinInitialize(0);
		hmq = WinCreateMsgQueue(hab, 0);
		if (hSw != NULLHANDLE && hab != NULLHANDLE && hmq != NULLHANDLE) {
			safe_strncpy(swData.szSwtitle, org_switch_title, MAXNAMEL);
			WinChangeSwitchEntry(hSw, &swData);

			if (swData.hwnd)
				WinSetWindowText(swData.hwnd, org_win_title);
			WinDestroyMsgQueue(hmq);
			WinTerminate(hab);
		}
		pib->pib_ultype = oldType;
		mem_free(org_switch_title);
		mem_free(org_win_title);
		break;
	case 0:
		if (url && *url) {
			safe_strncpy(new_title, url, MAXNAMEL - 10);
			strcat(new_title, " - Links");
			pib->pib_ultype = 3;
			hab = WinInitialize(0);
			hmq = WinCreateMsgQueue(hab, 0);
			if (hSw != NULLHANDLE && hab != NULLHANDLE && hmq != NULLHANDLE) {
				safe_strncpy(swData.szSwtitle, new_title, MAXNAMEL);
				WinChangeSwitchEntry(hSw, &swData);

				if (swData.hwnd)
					WinSetWindowText(swData.hwnd, new_title);
				WinDestroyMsgQueue(hmq);
				WinTerminate(hab);
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
#if 0
	unsigned char cmdline[16];
	sprintf(cmdline, "mode ");
	snprint(cmdline + 5, 5, x);
	strcat(cmdline, ",");
	snprint(cmdline + strlen(cmdline), 5, y);
#endif
	return 0;
}


int
get_ctl_handle(void)
{
	return get_input_handle();
}


int
get_system_env(void)
{
	int env = get_common_env();

	/* !!! FIXME: telnet */
	if (!is_xterm()) env |= ENV_OS2VIO;

	return env;
}


void
open_in_new_vio(struct terminal *term, unsigned char *exe_name,
		unsigned char *param)
{
	exec_new_elinks(term, DEFAULT_OS2_WINDOW_CMD, exe_name, param);
}

void
open_in_new_fullscreen(struct terminal *term, unsigned char *exe_name,
		       unsigned char *param)
{
	exec_new_elinks(term, DEFAULT_OS2_FULLSCREEN_CMD, exe_name, param);
}


void
set_highpri(void)
{
	DosSetPriority(PRTYS_PROCESS, PRTYC_FOREGROUNDSERVER, 0, 0);
}

# endif
