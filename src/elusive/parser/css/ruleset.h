/* $Id: ruleset.h,v 1.3 2003/02/25 14:15:50 jonas Exp $ */

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
/* Example: 'div, p, ul { color: green; }' */
enum pstate_code
css_parse_ruleset(struct parser_state *state, unsigned char **src, int *len);

/* Parsing of selectors */
/* Example: 'div h1#foo.bar a' part of 'div h1#foo.bar a { display: none }' */
enum pstate_code
css_parse_selector(struct parser_state *state, unsigned char **src, int *len);

/* Parsing of simple selectors */
/* Example: 'h1#foo.bar' part of the 'div h1#foo.bar a' selector */
enum pstate_code
css_parse_simple_selector(struct parser_state *state, unsigned char **src, int *len);

/* Parsing of attributes in simple selectors */
/* Example: '#foo' or '.bar' part of the 'div h1#foo.bar a' selector */
enum pstate_code
css_parse_selector_attr(struct parser_state *state, unsigned char **src, int *len);

/* Parsing of declarations */
/* Example: code from '{' to '}' in 'div { color: black; margin: 0em }' */
enum pstate_code
css_parse_declarations(struct parser_state *state, unsigned char **src, int *len);

/* Parsing of a property declaration */
/* Example: 'color: black' part of the 'div { color: black; margin: 0em }' */
enum pstate_code
css_parse_declaration(struct parser_state *state, unsigned char **src, int *len);

/* Parsing of property settings */
/* Example: '0em 2em' part of the 'margin: 0em 2em' declaration */
enum pstate_code
css_parse_expression(struct parser_state *state, unsigned char **src, int *len);

/* Parsing of functions in selectors and expressions.
 * This can have optimized versions and just act as multiplexor. */
/* Example: the 'content("before")' part of 'p:before { content("before") }' */
enum pstate_code
css_parse_function(struct parser_state *state, unsigned char **src, int *len);

/* Parsing of rgb functions */
/* Example: 'rgb(10%, 50%, 10%)' */
enum pstate_code
css_parse_rgb(struct parser_state *state, unsigned char **src, int *len);

#endif /* EL__USIVE_PARSER_CSS_RULESET_H */
