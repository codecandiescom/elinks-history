/* $Id: tree.h,v 1.1 2003/02/25 14:17:25 jonas Exp $ */

#ifndef EL__USIVE_PARSER_CSS_TREE_H
#define EL__USIVE_PARSER_CSS_TREE_H

#include "elusive/parser/css/util.h"
#include "elusive/parser/parser.h"
#include "util/hash.h"
#include "util/lists.h"

/* The CSS hash items are used to store css_nodes with the same 'base' element
 * in stylesheet nodes hash. This way selectors 'div.foo bar' and 'div p' will
 * both be entered in the node list of the 'div' item. */
struct css_hash_item {
	/* The element name and length. Used as the hash key */

	unsigned char *name;
	int namelen;

	/* The nodes which have similar element names */
	struct list_head nodes;
};

/* The stylesheet structure is the root of all style_nodes */
struct stylesheet {
	/* The charset of the stylesheet. If none is specified with the
	 * @charset rule the charset specified in the <link> tag is used. 
	 * If this is also not specified the charset of the markup document
	 * is used. */

	struct css_string charset;

	/* The base url of the stylesheet is the first loaded css document.
	 * This is used for figuring out relative urls in imports. */

	unsigned char *baseurl;
	int baseurl_len;

	/* The mediatypes that should be included in the parsed stylesheet.
	 * Controls which imports and @media-rule blocks will be accepted.
	 * This is a bitmap made up of entries specified in atrule.h. If none
	 * are specified all will be accepted. */

	int mediatypes;

	/* The list of imported urls. This is not directly useful other than
	 * maybe for info in a document manager. */

	struct list_head imports; /* -> struct property */

	/* A hash for quick referencing the toplevel style_nodes. Note that the
	 * '*' element will be hosting '.myclass' and other selectors that does
	 * not begin with an element. */

	struct hash *hash; /* -> struct css_hash_item */

	/* For fast lookups of classes, ids etc. */

	struct css_hash_item *universal;
};

/* Specifiers for how to match element relations including their attributes in
 * the document tree */
enum css_match_type {
	/* - Element - */

	/* Example: 'div p' matches when p is contained in a div
	 * element */
	MATCH_DESCENDANT		= (1 << 0),

	/* Example: 'div * p' matches when p is a grandchild or later
	 * decendant of a div element. */
	MATCH_UNIVERSAL			= (1 << 1),

	/* Example: 'div > p' matches when p is (a direct) child of a div
	 * element. */
	MATCH_CHILD			= (1 << 2), /* h1 + p */

	/* Example: 'div + p' matches when div and p share the same parent
	 * and div immediately precedes p. */
	MATCH_ADJACENT_SIBLING		= (1 << 3), /* h1 > p */

	/* Example: 'p:first-line' or 'h1:first-letter' */
	/* TODO should they be placed here ? --jonas */
	MATCH_PSEUDO_ELEMENT		= (1 << 4), /* h1 + p */

	/* - Attribute - */

	/* Example: h1[class=pitbull] matches <h1 class="pitbull"> */
	MATCH_EXACT			= (1 << 5),

	/* Example: a[rel~=home] matches <a rel="top home index" [etc]>
	 * HTML class attributes have this matching type */
	MATCH_SPACED_LIST_ENTRY		= (1 << 6),

	/* Example: p[lang|=en] matches <p lang="en"> and <p lang="en-US"> */
	MATCH_HYPHENED_LIST_START	= (1 << 7),

	/* Example: p:first-child or a:hover */
	/* TODO should they be placed here ? --jonas */
	MATCH_PSEUDO_CLASS		= (1 << 8),

	/* Example: h1.pitbull matches <h1 class="pitbull"> */
	MATCH_CLASS			= (1 << 9),

	/* Example: h1#pitbull matches <h1 id="pitbull"> */
	MATCH_ID			= (1 << 10),

	/* Placeholder dummy used for parsing */
	MATCH_ATTRIBUTE			= (1 << 11),
};

struct css_attr_match {
	/* List handles */

	struct css_attr_match *next;
	struct css_attr_match *prev;

	/* Info about how to perform the matching */
	enum css_match_type type;

	/* The attribute name (class, id etc.) */
	struct css_string name;

	/* The value (class names or ids) */
	struct css_string value;
};

/* A css_node contains info equal to that of a simple selector. This could be
 * 'div.foo' or 'p#bar.baz'. Since any attribute can be used in selectors to
 * pinpoint elements in the document and since attributes can occur more than
 * once everything is stored in a list. */
/* TODO Possibly make shortcuts to commonly used html attributes (class/id).
 * TODO Use skiplists. */
struct selector_node {
	/* Tree position stuff. First, for easy lists handling, position in the
	 * list of leafs of the parent node. */

	struct css_node *next;
	struct css_node *prev;

	/* Position stuff for setting handling scoping in the css environment
	 * when walking the document tree and matching. */

	struct css_node *next_env;
	struct css_node *prev_env;

	/* Info about how to perform the matching */
	enum css_match_type match_type;

	/* See util.h for explaination. */
	struct css_string *str;

	/* Properties of the node. Colors, text-alignment etc.
	 * For missing properties, we will just ascend to the parent and look
	 * there if the style is inherited. */
	/* See property.h for a description of the struct property. */

	struct list_head properties; /* -> struct property */

	/* The attributes that have to match. */

	struct list_head attributes; /* -> struct css_attr_match */

	/* The children of the node. */

	struct list_head leafs; /* -> struct selector_node */
};

/* Initializes a stylesheet structure. Returns NULL upon allocation failure. */
struct stylesheet *
init_stylesheet();

/* Release the stylesheet and all its content. */
void
done_stylesheet(struct stylesheet *stylesheet);

/* Initializes a node structure. Returns NULL upon allocation failure. */
struct selector_node *
init_selector_node();

/* Releases the node structure and all its attributes and leafs. */
void
done_selector_node(struct selector_node *node);

/* Prepares the css node to be added to the stylesheet tree. The node can first
 * be really added when all the attributes has been established so that
 * dublicate entries are avoided. The real adding is done with the add_css_node
 * below by the ruleset parser. It returns the node structure or NULL if failed
 * (the syntax tree is not touched then). */
struct selector_node *
spawn_selector_node(struct parser_state *state, struct css_string *str);

/* Adds the css node in state->current either to the stylesheet hash or to the
 * state->root node. This is done by checking if the node is already known. Is
 * this case the node in state->current is deallocated and substituted with the
 * already added node. */
void
add_selector_node(struct parser_state *state);

/* Adds an attribute to match to the css node in state->current in the correct
 * alphabetical order. It returns the attribute structure for further
 * initialization (the value) or NULL if allocation failed. */
struct css_attr *
add_css_attr(struct parser_state *state, unsigned char *name,
	     int namelen, enum css_attr_match_type type);

/* Returns value string of an attribute with this name. NULL means there's no
 * such attribute set, otherwise a pointer is returned that points to
 * dynamically allocated memory (which you have to free after doing what you
 * needed to, yes). */
unsigned char *
get_css_property(struct css_node *node, unsigned char *name);

#endif
