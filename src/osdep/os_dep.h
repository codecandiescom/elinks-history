/* $Id: os_dep.h,v 1.28 2003/10/27 02:55:17 pasky Exp $ */

#ifndef EL__OSDEP_OS_DEP_H
#define EL__OSDEP_OS_DEP_H


/* Some common ascii codes. */
#define ASCII_BS 8
#define ASCII_TAB 9
#define ASCII_LF 10
#define ASCII_CR 13
#define ASCII_ESC 27
#define ASCII_DEL 127


struct terminal;

struct open_in_new {
	unsigned char *text;
	void (*fn)(struct terminal *term, unsigned char *, unsigned char *);
};

int get_system_env(void);
int get_e(unsigned char *env);
int is_xterm(void);
int is_twterm(void);
int get_terminal_size(int, int *, int *);
void handle_terminal_resize(int, void (*)(void));
void unhandle_terminal_resize(int);
void set_bin(int);
int c_pipe(int *);
int get_input_handle(void);
int get_output_handle(void);
int get_ctl_handle(void);
void want_draw(void);
void done_draw(void);
void terminate_osdep(void);
void *handle_mouse(int, void (*)(void *, unsigned char *, int), void *);
void unhandle_mouse(void *);
int check_file_name(unsigned char *);
int start_thread(void (*)(void *, int), void *, int);
unsigned char *get_clipboard_text(void);
void set_clipboard_text(unsigned char *);
void set_window_title(unsigned char *);
unsigned char *get_window_title(void);
void block_stdin(void);
void unblock_stdin(void);
int exe(unsigned char *);
int resize_window(int, int);
int can_resize_window(int);
int can_open_os_shell(int);
struct open_in_new *get_open_in_new(int);
int can_open_in_new(struct terminal *);
void set_highpri(void);

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
