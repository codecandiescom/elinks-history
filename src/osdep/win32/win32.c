/* Win32 support fo ELinks. It has pretty different life than rest of ELinks. */
/* $Id: win32.c,v 1.13 2004/04/17 01:30:32 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "osdep/system.h"

#ifdef WIN32

#include <windows.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "elinks.h"

#include "osdep/win32/win32.h"
#include "osdep/osdep.h"
#include "terminal/terminal.h"


static int w32_input_pid;

static char* keymap[] = {
	"\E[5~", /* VK_PRIOR */
	"\E[6~", /* VK_NEXT */
	"\E[F", /* VK_END */
	"\E[H", /* VK_HOME */
	"\E[D", /* VK_LEFT */
	"\E[A", /* VK_UP */
	"\E[C", /* VK_RIGHT */
	"\E[B", /* VK_DOWN */
	"", /* VK_SELECT */
	"", /* VK_PRINT */
	"", /* VK_EXECUTE */
	"", /* VK_SNAPSHOT */
	"\E[2~", /* VK_INSERT */
	"\E[3~" /* VK_DELETE */
};


void
input_function(int fd)
{
	BOOL bSuccess;
	HANDLE hStdIn, hStdOut;
	DWORD dwMode;
	INPUT_RECORD inputBuffer;
	DWORD dwInputEvents;
	COORD coordScreen;
	DWORD cCharsRead;
	CONSOLE_CURSOR_INFO cci;
	CONSOLE_SCREEN_BUFFER_INFO csbi;

	/* let's put up a meaningful console title */
	bSuccess = SetConsoleTitle("ELinks - Console mode browser");

	/* get the standard handles */
	hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	hStdIn = GetStdHandle(STD_INPUT_HANDLE);

	/* set up mouse and window input */
	bSuccess = GetConsoleMode(hStdIn, &dwMode);

	bSuccess = SetConsoleMode(hStdIn, (dwMode & ~(ENABLE_LINE_INPUT |
			ENABLE_ECHO_INPUT)) | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT);

	cci.dwSize = 100;
	cci.bVisible = TRUE;
	bSuccess = SetConsoleCursorInfo(hStdOut, &cci);
	/* This is the main input loop. Read from the input queue and process */
	/* the events read */
	do {
		/* read an input events from the input event queue */
		bSuccess = ReadConsoleInput(hStdIn, &inputBuffer, 1, &dwInputEvents);
		switch (inputBuffer.EventType) {
		case KEY_EVENT:
			if (inputBuffer.Event.KeyEvent.bKeyDown)
			{
				char c = inputBuffer.Event.KeyEvent.uChar.AsciiChar;

				if (!c) {
					int vkey = inputBuffer.Event.KeyEvent.wVirtualKeyCode;

					if (vkey >= VK_PRIOR && vkey <= VK_DELETE)
					{
						char *p = keymap[vkey - VK_PRIOR];

						if (*p)
							if (write(fd, p, strlen(p)) < 0)
								bSuccess = FALSE;
					}
					break;
				}
				if (write(fd, &c, 1) < 0)
					bSuccess = FALSE;
			}
			break;
		case MOUSE_EVENT:
			if (inputBuffer.Event.MouseEvent.dwEventFlags == 0 &&
				inputBuffer.Event.MouseEvent.dwButtonState)
			{
				char	mstr[] = "\E[Mxxx";

				mstr[3] = ' ' | 0;
				mstr[4] = ' ' + 1 +
					inputBuffer.Event.MouseEvent.dwMousePosition.X;
				mstr[5] = ' ' + 1 +
					inputBuffer.Event.MouseEvent.dwMousePosition.Y;
				if (write(fd, mstr, 6) < 0)
					bSuccess = FALSE;
				mstr[3] = ' ' | 3;
				if (write(fd, mstr, 6) < 0)
					bSuccess = FALSE;
			}
			break;
		case WINDOW_BUFFER_SIZE_EVENT:
			write(fd, "\E[R", 3);
			break;
		} /* switch */
		/* when we receive an esc down key, drop out of do loop */
	} while (bSuccess);

	exit(0);
}

#if 0
void
handle_terminal_resize(int fd, void (*fn)())
{
		return;
}

void
unhandle_terminal_resize(int fd)
{
		return;
}

int
get_terminal_size(int fd, int *x, int *y)
{
	CONSOLE_SCREEN_BUFFER_INFO	s;

	if (GetConsoleScreenBufferInfo (GetStdHandle (STD_OUTPUT_HANDLE), &s))
	{
		*x = s.dwSize.X - 1;
		*y = s.dwSize.Y - 1;
		return 0;
	}
	*x = 80;
	*y = 25;
	return 0;
}
#endif

void
terminate_osdep(void)
{
	kill (w32_input_pid, SIGINT);
}

void
set_proc_id(int id)
{
	w32_input_pid = id;
}


int
exe(unsigned char *path)
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


void input_function(int fd);
void set_proc_id(int id);

int
get_input_handle(void)
{
	int fd[2];
	static int ti = -1;
	static int tp = -1;
	int pid;

	if (ti != -1) return ti;
	if (c_pipe(fd) < 0) return 0;
	ti = fd[0];
	tp = fd[1];

	pid = fork();
	if (!pid)
		input_function(tp);
	else
		set_proc_id(pid);

	return ti;
}


int
get_system_env(void)
{
	return get_common_env() | ENV_WIN32;
}

#endif
