/* $Id: kbdbind.h,v 1.171 2005/06/10 18:50:59 miciah Exp $ */

#ifndef EL__CONFIG_KBDBIND_H
#define EL__CONFIG_KBDBIND_H

/* #include "bfu/listbox.h" */
struct listbox_item;

#include "config/options.h"
#include "sched/event.h"
#include "terminal/terminal.h"
#include "util/object.h"
#include "util/string.h"

struct action {
	unsigned char *str;
	long num;
	unsigned char *desc;
	unsigned int flags;
};

struct action_list {
	struct action *actions;
	int num_actions;
};

enum keymap_id {
	KEYMAP_MAIN,
	KEYMAP_EDIT,
	KEYMAP_MENU,
	KEYMAP_MAX
};

struct keymap {
	unsigned char *str;
	enum keymap_id keymap_id;
	unsigned char *desc;
};

enum action_flags {
	ACTION_RESTRICT_ANONYMOUS	=    (1 << 16),
	ACTION_REQUIRE_VIEW_STATE	=    (1 << 17),
	ACTION_REQUIRE_LOCATION		=    (1 << 18),
	ACTION_JUMP_TO_LINK		=    (1 << 19),
	ACTION_FLAGS_MASK		= (0xFF << 16),
};

/* Note: if you add anything here, please keep it in alphabetical order,
 * and also update the table action_table[] in kbdbind.c.  */

#define ACTION_(map, name, action, caption, flags)	\
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

#undef	ACTION_
#define ACTION_(map, name, action, caption, flags)	\
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

#undef	ACTION_

enum kbdbind_flags {
	KBDB_WATERMARK = 1,
	KBDB_TOUCHED = 2,
	/* Marks wether the keybinding combination also ``belongs'' to a
	 * default keybinding. */
	KBDB_DEFAULT = 4,
};

struct keybinding {
	LIST_HEAD(struct keybinding);
	enum keymap_id keymap_id;
	int action_id;
	struct term_event_keyboard kbd;
	int event;
	int flags;
	struct listbox_item *box_item;
	struct object object;
};


void init_keymaps(void);
void free_keymaps(void);

struct keybinding *add_keybinding(enum keymap_id keymap_id, int action_id, struct term_event_keyboard *kbd, int event);
int keybinding_exists(enum keymap_id keymap_id, struct term_event_keyboard *kbd, int *action_id);
void free_keybinding(struct keybinding *);

struct action *get_action(enum keymap_id keymap_id, long action_id);
unsigned char *get_action_name(enum keymap_id keymap_id, long action_id);
long get_action_from_string(enum keymap_id keymap_id, unsigned char *str);

static inline unsigned int
action_is_anonymous_safe(enum keymap_id keymap_id, long action_id)
{
	struct action *action = get_action(keymap_id, action_id);

	return action && !(action->flags & ACTION_RESTRICT_ANONYMOUS);
}

static inline unsigned int
action_requires_view_state(enum keymap_id keymap_id, long action_id)
{
	struct action *action = get_action(keymap_id, action_id);

	return action && (action->flags & ACTION_REQUIRE_VIEW_STATE);
}

static inline unsigned int
action_requires_location(enum keymap_id keymap_id, long action_id)
{
	struct action *action = get_action(keymap_id, action_id);

	return action && (action->flags & ACTION_REQUIRE_LOCATION);
}

static inline unsigned int
action_prefix_is_link_number(enum keymap_id keymap_id, long action_id)
{
	struct action *action = get_action(keymap_id, action_id);

	return action && (action->flags & ACTION_JUMP_TO_LINK);
}

long read_key(unsigned char *);
unsigned char *get_keymap_name(enum keymap_id);

int parse_keystroke(unsigned char *, struct term_event_keyboard *);
void make_keystroke(struct string *str, struct term_event_keyboard *kbd, int escape);

#define make_keystroke_from_accesskey(str, accesskey) do { 	\
	struct term_event_keyboard kbd; 			\
	kbd.key = accesskey; /* FIXME: unicode_val_T to int */	\
	kbd.modifier = 0; 					\
	make_keystroke(str, &kbd, 0); 				\
} while (0)

int kbd_action(enum keymap_id, struct term_event *, int *);
struct keybinding *kbd_ev_lookup(enum keymap_id, struct term_event_keyboard *kbd, int *);
struct keybinding *kbd_nm_lookup(enum keymap_id, unsigned char *);

int bind_do(unsigned char *, unsigned char *, unsigned char *);
unsigned char *bind_act(unsigned char *, unsigned char *);
void bind_config_string(struct string *);

#ifdef CONFIG_SCRIPTING
int bind_key_to_event_name(unsigned char *, unsigned char *, unsigned char *,
			   unsigned char **);
#endif

void add_keystroke_to_string(struct string *string, int action_id, enum keymap_id keymap_id);
unsigned char *get_keystroke(int action_id, enum keymap_id keymap_id);

void add_actions_to_string(struct string *string, int *actions,
			   enum keymap_id keymap_id, struct terminal *term);

#endif
