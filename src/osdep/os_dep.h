/* $Id: os_dep.h,v 1.13 2003/05/06 18:25:29 zas Exp $ */

#ifndef EL__OS_DEP_H
#define EL__OS_DEP_H


/* Some common ascii codes. */
#define ASCII_BS 8
#define ASCII_TAB 9
#define ASCII_LF 10
#define ASCII_CR 13
#define ASCII_ESC 27
#define ASCII_DEL 127

/* System-type identifier */
#define SYS_UNIX	1
#define SYS_OS2		2
#define SYS_WIN32	3
#define SYS_BEOS	4
#define SYS_RISCOS	5

/* hardcoded limit of 10 OSes in default.c */

#if defined(__EMX__)
#define OS2
#elif defined(_WIN32) || defined (__CYGWIN__)
#define WIN32
#ifdef UNIX
#undef UNIX
#endif
#elif defined(__BEOS__)
#define BEOS
#elif defined(__riscos__)
#define RISCOS
#else
#define UNIX
#endif

#ifdef __EMX__
#define strcasecmp stricmp
#define strncasecmp strnicmp
#ifndef HAVE_STRCASECMP
#define HAVE_STRCASECMP
#endif
#ifndef HAVE_STRNCASECMP
#define HAVE_STRNCASECMP
#endif
#define read _read
#define write _write
#ifdef O_SIZE
#define USE_OPEN_PREALLOC
#endif
#endif

#if defined(UNIX)

static inline int dir_sep(char x) { return x == '/'; }
#define NEWLINE	"\n"
#define FS_UNIX_RIGHTS
#define FS_UNIX_HARDLINKS
#define FS_UNIX_SOFTLINKS
#define FS_UNIX_USERS
#include <pwd.h>
#include <grp.h>
#define SYSTEM_ID SYS_UNIX
#define SYSTEM_NAME "Unix"
#define SYSTEM_STR "unix"
#define DEFAULT_SHELL "/bin/sh"
#define GETSHELL getenv("SHELL")
#ifdef HAVE_SYS_UN_H
#define USE_AF_UNIX
#else
#define DONT_USE_AF_UNIX
#endif
#define ASSOC_BLOCK
#define ASSOC_CONS_XWIN

#elif defined(OS2)

static inline int dir_sep(char x) { return x == '/' || x == '\\'; }
#define NEWLINE	"\r\n"
/*#define NO_ASYNC_LOOKUP*/
#define SYSTEM_ID SYS_OS2
#define SYSTEM_NAME "OS/2"
#define SYSTEM_STR "os2"
#define DEFAULT_SHELL "cmd.exe"
#define GETSHELL getenv("COMSPEC")
#define NO_FG_EXEC
#define DOS_FS
#define NO_FILE_SECURITY
#define NO_FORK_ON_EXIT
#define ASSOC_CONS_XWIN

#elif defined(WIN32)

static inline int dir_sep(char x) { return x == '/' || x == '\\'; }
#define NEWLINE	"\r\n"
/*#define NO_ASYNC_LOOKUP*/
#define SYSTEM_ID SYS_WIN32
#define SYSTEM_NAME "Win32"
#define SYSTEM_STR "win32"
#define DEFAULT_SHELL "cmd.exe"
#define GETSHELL getenv("COMSPEC")
#define NO_FG_EXEC
#define DOS_FS
#define NO_FORK_ON_EXIT

#elif defined(BEOS)

static inline int dir_sep(char x) { return x == '/'; }
#define NEWLINE	"\n"
#define FS_UNIX_RIGHTS
#define FS_UNIX_SOFTLINKS
#define FS_UNIX_USERS
#include <pwd.h>
#include <grp.h>
#define SYSTEM_ID SYS_BEOS
#define SYSTEM_NAME "BeOS"
#define SYSTEM_STR "beos"
#define DEFAULT_SHELL "/bin/sh"
#define GETSHELL getenv("SHELL")
#define NO_FORK_ON_EXIT
#define ASSOC_BLOCK

#include <sys/time.h>
#include <sys/types.h>
#include <net/socket.h>

int be_socket(int, int, int);
int be_connect(int, struct sockaddr *, int);
int be_getpeername(int, struct sockaddr *, int *);
int be_getsockname(int, struct sockaddr *, int *);
int be_listen(int, int);
int be_accept(int, struct sockaddr *, int *);
int be_bind(int, struct sockaddr *, int);
int be_pipe(int *);
int be_read(int, void *, int);
int be_write(int, void *, int);
int be_close(int);
int be_select(int, struct fd_set *, struct fd_set *, struct fd_set *, struct timeval *);
int be_getsockopt(int, int, int, void *, int *);

#elif defined(RISCOS)

static inline int dir_sep(char x) { return x == '/' || x == '\\'; }
#define NEWLINE        "\n"
#define SYSTEM_ID SYS_RISCOS
#define SYSTEM_NAME "RISC OS"
#define SYSTEM_STR "riscos"
#define DEFAULT_SHELL "gos"
#define GETSHELL getenv("SHELL")
#define NO_FG_EXEC
#define NO_FILE_SECURITY
#define NO_FORK_ON_EXIT

#endif

#if !defined(HAVE_BEGINTHREAD) && !defined(BEOS) && !defined(HAVE_PTHREADS) && !defined(HAVE_CLONE)
#define THREAD_SAFE_LOOKUP
#endif

#ifndef HAVE_SA_STORAGE
#define sockaddr_storage sockaddr
#endif


/* TODO: This should be in a separate .h file! */

struct terminal;

struct open_in_new {
	unsigned char *text;
	void (*fn)(struct terminal *term, unsigned char *, unsigned char *);
};

int get_system_env();
int is_xterm();
int can_twterm();
int get_terminal_size(int, int *, int *);
void handle_terminal_resize(int, void (*)());
void unhandle_terminal_resize(int);
void set_bin(int);
int c_pipe(int *);
int get_input_handle();
int get_output_handle();
int get_ctl_handle();
void want_draw();
void done_draw();
void terminate_osdep();
void *handle_mouse(int, void (*)(void *, unsigned char *, int), void *);
void unhandle_mouse(void *);
int check_file_name(unsigned char *);
int start_thread(void (*)(void *, int), void *, int);
char *get_clipboard_text();
void set_clipboard_text(char *);
void set_window_title(unsigned char *);
unsigned char *get_window_title();
int is_safe_in_shell(unsigned char);
void check_shell_security(unsigned char **);
void block_stdin();
void unblock_stdin();
int exe(char *);
int resize_window(int, int);
int can_resize_window(int);
int can_open_os_shell(int);
struct open_in_new *get_open_in_new(int);
int can_open_in_new(struct terminal *);
void set_highpri();

#ifdef USE_OPEN_PREALLOC
int open_prealloc(char *, int, int, int);
void prealloc_truncate(int, int);
#else
static inline void prealloc_truncate(int x, int y) { }
#endif

unsigned char *get_system_str(int);

int set_nonblocking_fd(int);
int set_blocking_fd(int);

#endif
