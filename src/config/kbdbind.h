/* $Id: kbdbind.h,v 1.154 2005/05/17 21:55:14 zas Exp $ */

#ifndef EL__CONFIG_KBDBIND_H
#define EL__CONFIG_KBDBIND_H

/* #include "bfu/listbox.h" */
struct listbox_item;

#include "config/options.h"
#include "sched/event.h"
#include "terminal/terminal.h"
#include "util/object.h"
#include "util/string.h"

struct strtonum {
	unsigned char *str;
	long num;
	unsigned char *desc;
};

enum keymap {
	KEYMAP_MAIN,
	KEYMAP_EDIT,
	KEYMAP_MENU,
	KEYMAP_MAX
};

enum action_flags {
	ACTION_RESTRICT_ANONYMOUS	=    (1 << 16),
	ACTION_REQUIRE_VIEW_STATE	=    (1 << 17),
	ACTION_JUMP_TO_LINK		=    (1 << 18),
	ACTION_FLAGS_MASK		= (0xFF << 16),
};

/* Note: if you add anything here, please keep it in alphabetical order,
 * and also update the table action_table[] in kbdbind.c.  */

#define ACTION_INFO(map, name, action, caption, flags)	\
	ACT_##map##_OFFSET_##action

enum main_action_offset {
#include "config/actions-main.inc"

	MAIN_ACTIONS,
};

enum edit_action_offset {
#include "config/actions-edit.inc"

	EDIT_ACTIONS
};

enum menu_action_offset {
#include "config/actions-menu.inc"

	MENU_ACTIONS
};

#undef	ACTION_INFO
#define ACTION_INFO(map, name, action, caption, flags)	\
	ACT_##map##_##action

enum main_action {
#include "config/actions-main.inc"
};

enum edit_action {
#include "config/actions-edit.inc"
};

enum menu_action {
#include "config/actions-menu.inc"
};

#undef	ACTION_INFO

enum kbdbind_flags {
	KBDB_WATERMARK = 1,
	KBDB_TOUCHED = 2,
	/* Marks wether the keybinding combination also ``belongs'' to a
	 * default keybinding. */
	KBDB_DEFAULT = 4,
};

struct keybinding {
	LIST_HEAD(struct keybinding);
	enum keymap keymap;
	int action;
	struct term_event_keyboard kbd;
	int event;
	int flags;
	struct listbox_item *box_item;
	struct object object;
};


void init_keymaps(void);
void free_keymaps(void);

struct keybinding *add_keybinding(enum keymap km, int action, struct term_event_keyboard *kbd, int event);
int keybinding_exists(enum keymap km, struct term_event_keyboard *kbd, int *action);
void free_keybinding(struct keybinding *);

long read_key(unsigned char *);
int read_action(enum keymap keymap, unsigned char *action);
unsigned char *write_action(enum keymap, int);
unsigned char *write_keymap(enum keymap);

int parse_keystroke(unsigned char *, struct term_event_keyboard *);
void make_keystroke(struct string *str, struct term_event_keyboard *kbd, int escape);

#define make_keystroke_from_accesskey(str, accesskey) do { 	\
	struct term_event_keyboard kbd; 			\
	kbd.key = accesskey; /* FIXME: long to int */ 		\
	kbd.modifier = 0; 					\
	make_keystroke(str, &kbd, 0); 				\
} while (0)

int kbd_action(enum keymap, struct term_event *, int *);
struct keybinding *kbd_ev_lookup(enum keymap, struct term_event_keyboard *kbd, int *);
struct keybinding *kbd_nm_lookup(enum keymap, unsigned char *);

int bind_do(unsigned char *, unsigned char *, unsigned char *);
unsigned char *bind_act(unsigned char *, unsigned char *);
void bind_config_string(struct string *);

#ifdef CONFIG_SCRIPTING
int bind_key_to_event_name(unsigned char *, unsigned char *, unsigned char *,
			   unsigned char **);
#endif

void add_keystroke_to_string(struct string *string, int action, enum keymap map);
unsigned char *get_keystroke(int action, enum keymap map);

void add_actions_to_string(struct string *string, int *actions,
			   enum keymap map, struct terminal *term);

#endif
