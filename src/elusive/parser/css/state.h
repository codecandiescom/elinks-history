/* CSS Parser state data */
/* $Id: state.h,v 1.1 2003/01/19 20:25:21 jonas Exp $ */

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
	CSS_SKIP,
	CSS_SKIPBLOCK,
	CSS_STRING,
	CSS_STYLESHEET,
	CSS_UNICODERANGE,
	CSS_URL,
	CSS_WHITESPACE,

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

			/* The stylesheet node */
			struct stylesheet *node;
		} stylesheet;

		/* CSS_ATRULE */
		struct {
			/* The name of the atrule (e.g. import or charset) */
			unsigned char *name;
			int name_len;

			/* For passing on the stylesheet node */
			struct stylesheet *stylesheet;
		} atrule;

		/* CSS_CHARSET */
		struct {
			/* The stylesheet node for getting mediatypes and
			 * adding charset url. */
			struct stylesheet *stylesheet;

			/* The string data containing the charset */
			unsigned char *str;
			int len;
		} charset;

		/* CSS_IMPORT */
		struct {
			/* The imported url */
			unsigned char *url;
			int url_len;

			/* The stylesheet node for getting mediatypes,
			 * adding import url and signaling if import should
			 * be accepted (NULL means not). */
			struct stylesheet *stylesheet;
		} import;

		/* CSS_MEDIA */
		struct {
			/* For signaling if we're inside the block */
			int inside;

			/* The stylesheet node for getting mediatypes, passing
			 * on stylesheet node and signaling if media block
			 * should be accepted (NULL means not). */
			struct stylesheet *stylesheet;
		} media;

		/* CSS_MEDIATYPES */
		struct {
			/* For getting token from the scanners */
			unsigned char *name;
			int name_len;

			/* The stylesheet node for checking bitmaps and
			 * signaling if there was a match or not. */
			struct stylesheet **stylesheet;
		} mediatypes;

		/* CSS_RULESET */
		struct {
			/* The properties to add to a group of selectors. */
			struct list_head *properties;

			/* The stylesheet node */
			struct stylesheet *stylesheet;

			/* For signaling errors */
			int ack;
		} ruleset;

		/* CSS_SELECTOR */
		struct {
			/* The node corresponding to a given selector. */
			struct css_node *node;

			/* For getting token from the scanners */
			unsigned char *name;
			int name_len;
		} selector;

		/* CSS_DECLARATION */
		struct {
			/* Will work directly on the given property list. */
			struct list_head *properties;

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
		/* XXX Depending on <inside> being initialized to 0 */
		struct {
			/* Wether the scanner's inside the comment. Used when
			 * returning from suspension */
			int inside;
		} comment;

		/* CSS_SKIPBLOCK */
		struct {
			/* Info about the current block depth used across
			 * suspensions */
			int nest_level;
		} skipblock;
	} data;
};

static inline struct css_parser_state *
css_state_push(struct parser_state *state, enum css_state_code state_code)
{
#ifdef DEBUG
	if (css_stack_size++ > 20) internal("CSS stack items skyrocketing");
#endif

	return (struct css_parser_state *)
		state_stack_push(state, sizeof(struct css_parser_state),
				 state_code);
}

static inline struct css_parser_state *
css_state_pop(struct parser_state *state)
{
#ifdef DEBUG
	css_stack_size--;
#endif

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
