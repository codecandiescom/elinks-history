/* $Id: ruleset.h,v 1.2 2003/01/20 00:57:50 jonas Exp $ */

#ifndef EL__USIVE_PARSER_CSS_RULESET_H
#define EL__USIVE_PARSER_CSS_RULESET_H

#include "elusive/parser/parser.h"

/* The setup:
 *
 * Rulesets with multiple selectors are dublicated. This will make the syntax
 * tree less complex to navigate since all style properties belonging to a node
 * will be in one place. However a bit more memory consuming. Example:
 *
 *      h1 a, h2 a { color: green }
 *
 * will be broken up and interpreted as:
 *
 *      h1 a { color: green } h2 a { color: green }
 *
 * when adding them to the syntax tree. Later something like:
 *
 *      h1 a { color: black }
 *
 * will trivially just update already added color property.
 *
 * The selector parser handles selectors. On return the selector parser has
 * either signaled an error or swapped it's stack state with the ruleset parser
 * so a selector grouping will accumulate selector parser states underneath the
 * ruleset parser state.
 *
 * Upon entering the block the ruleset parser will get each property and add
 * them to a list. When the block is done all properties will be separately
 * added to the css_nodes in the accumulated selector states.
 *
 * If something goes wrong the ruleset parser acts as a fallback and will skip
 * until the end of the block and remove the selector states.
 */

/* Parsing of rulesets */
enum pstate_code
css_parse_ruleset(struct parser_state *state, unsigned char **src, int *len);

/* Parsing of selectors */
enum pstate_code
css_parse_selector(struct parser_state *state, unsigned char **src, int *len);

/* Parsing of a property declaration */
enum pstate_code
css_parse_declaration(struct parser_state *state, unsigned char **src, int *len);

/* Parsing of property settings */
enum pstate_code
css_parse_expression(struct parser_state *state, unsigned char **src, int *len);

/* Parsing of functions in selectors and expressions.
 * This can have optimized versions and just act as multiplexor. */
enum pstate_code
css_parse_function(struct parser_state *state, unsigned char **src, int *len);

/* Parsing of rgb functions */
enum pstate_code
css_parse_rgb(struct parser_state *state, unsigned char **src, int *len);

#endif /* EL__USIVE_PARSER_CSS_RULESET_H */
