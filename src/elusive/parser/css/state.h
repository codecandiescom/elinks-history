/* CSS Parser state data */
/* $Id: state.h,v 1.5 2003/07/07 00:12:24 jonas Exp $ */

#ifndef EL__USIVE_PARSER_CSS_STATE_H
#define EL__USIVE_PARSER_CSS_STATE_H

#include "elusive/parser/css/tree.h"
#include "elusive/parser/property.h"
#include "elusive/parser/stack.h"
#include "util/lists.h"

#ifdef DEBUG
extern int css_stack_size;
#endif

/* We maintain the states in a stack, maintained by utility
 * functions and structures below. */
/* XXX Keep in alphabetical order! */
enum css_state_code
{
	CSS_ATRULE,
	CSS_CHARSET,
	CSS_COMMENT,
	CSS_DECLARATION,
	CSS_DECLARATIONS,
	CSS_ESCAPE,
	CSS_EXPRESSION,
	CSS_FONTFACE,
	CSS_FUNCTION,
	CSS_HEXCOLOR,
	CSS_IDENT,
	CSS_IMPORT,
	CSS_MEDIA,
	CSS_MEDIATYPES,
	CSS_NAME,
	CSS_PAGE,
	CSS_RGB,
	CSS_RULESET,
	CSS_SELECTOR,
	CSS_SIMPLE_SELECTOR,
	CSS_SELECTOR_ATTR,
	CSS_SKIP,
	CSS_SKIP_MEDIATYPES,
	CSS_SKIP_UNTIL,
	CSS_STRING,
	CSS_STYLESHEET,
	CSS_UNICODERANGE,
	CSS_URL,

	CSS_STATE_CODES, /* XXX: Keep last */
};

struct css_parser_state {
	struct css_parser_state *up;

	enum css_state_code state;

	union {
		/* Upper level parser states */

		/* CSS_STYLESHEET */
		struct {
			/* Denotes if import declarations are allowed */
			int no_imports;

			/* Denotes if charset declarations are allowed */
			int no_charset;
		} stylesheet;

		/* CSS_ATRULE */
		struct {
			/* The name of the atrule (e.g. import or charset) */
			unsigned char *name;
			int name_len;
		} atrule;

		/* CSS_CHARSET */
		struct {
			/* The string data containing the charset */
			unsigned char *str;
			int len;
		} charset;

		/* CSS_IMPORT */
		struct {
			/* The imported url */
			unsigned char *url;
			int url_len;

			/* Wether the mediatypes matched. Values are
			 *	 0 -> have not yet been checked
			 *	 1 -> one did match
			 *	-1 -> no one matched */
			int matched;
		} import;

		/* CSS_MEDIA */
		struct {
			/* Wether the mediatypes matched. Values are
			 *	 0 -> have not yet been checked
			 *	 1 -> one did match
			 *	-1 -> no one matched */
			int matched;

			/* Wether we are already inside the block */
			int inside_block;
		} media;

		/* CSS_MEDIATYPES */
		struct {
			/* For getting token from the scanners */
			unsigned char *name;
			int name_len;

			/* For reporting if any of the mediatypes matched. */
			int *matched;
		} mediatypes;

		/* CSS_RULESET */
		struct {
			/* The selector nodes of the ruleset. */
			struct css_node *selectors;
		} ruleset;

		/* CSS_SELECTOR */
		struct {
			/* The node worked on by simple selector */
			struct css_node *work;

			/* The match type of the element. See tree.h */
			enum css_element_match_type match_type;
		} selector;

		/* CSS_SIMPLE_SELECTOR */
		struct {
			/* For attributes being worked on */
			struct attribute_matching *attr;

			/* The match type of the attribute. See tree.h */
			enum css_attr_match_type match_type;

			/* For getting token from the scanners */
			unsigned char *name;
			int name_len;
		} simple_selector;

		/* CSS_DECLARATIONS */
		struct {
			/* The properties to add to a group of selectors. */
			struct list_head *properties;

			/* The selector nodes where properties will be added */
			struct css_node *nodes;
		} declarations;

		/* CSS_DECLARATION */
		struct {
			/* Will work directly on the given property list. */
			struct list_head **properties;

			/* The terms from the expression. */
			struct list_head *terms;

			/* TODO will also contain enum css_property
			 * initialized by a property parser */
		} declaration;

		/* CSS_EXPRESSION */
		struct {
			/* The terms from the expression. */
			struct list_head **terms;

			/* For getting token from the scanners */
			unsigned char *name;
			int name_len;
		} expression;

		/* Low level scanner states */

		/* Generic state variables used for token scanners. They get to
		 * work directly on where the token should be added. */
		/* CSS_IDENT, CSS_NAME, CSS_STRING, CSS_ESCAPE */
		struct {
			/* The token */
			unsigned char **str;
			int *len;

			/* For extra data ;) e.g. string delimiter info */
			int extra;
		} token;

		/* CSS_UNICODERANGE */
		struct {
			/* The endpoints of the range */
			int from;
			int to;

			/* For saving length of from hexdigit and signaling
			 * failure */
			int *from_len;
		} unicoderange;

		/* CSS_COMMENT */
		struct {
			/* Wether the scanner's inside the comment. Used when
			 * returning from suspension */
			int inside;
		} comment;

		/* CSS_SKIP and CSS_SKIP_UNTIL */
		/* The first two member are used by CSS_SKIP_UNTIL */
		struct {
			/* The character that should end the skipping */
			unsigned char end_marker;

			/* The character character to accept chars from */
			int group;

			/* info about the current block depth used across
			 * suspensions */
			int nest_level;
		} skip;
	} data;
};

static inline struct css_parser_state *
css_state_push(struct parser_state *state, enum css_state_code state_code)
{
	assert(css_stack_size++ > 20);

	return (struct css_parser_state *)
		state_stack_push(state, sizeof(struct css_parser_state),
				 state_code);
}

static inline struct css_parser_state *
css_state_pop(struct parser_state *state)
{
	assert(css_stack_size-- < 0);

	return (struct css_parser_state *)
		state_stack_pop(state);
}

static inline struct css_parser_state *
css_state_repush(struct parser_state *state, enum css_state_code state_code)
{
	return (struct css_parser_state *)
		state_stack_repush(state, sizeof(struct css_parser_state),
				 state_code);
}

#endif /* EL__USIVE_PARSER_CSS_STATE_H */
