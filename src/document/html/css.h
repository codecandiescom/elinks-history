/* $Id: css.h,v 1.1 2004/01/17 01:26:58 pasky Exp $ */

#ifndef EL__DOCUMENT_HTML_CSS_H
#define EL__DOCUMENT_HTML_CSS_H

struct html_element;

/* This is a super-simplistic CSS micro-engine. */

/* What Is Gonna Be A FAQ: Why isn't the CSS support optional? */
/* Answer: Because I want to eliminate all trivial styling (like html_h*())
 * and substitute it with default stylesheet information, so that this can be
 * user-configured. On the other side we might yet reconsider whether we want
 * that before 1.0. Maybe we don't. */

/* TODO: Its job is separated to two phases. The first phase is a parser, it
 * takes a string with CSS code and transforms it to an internal set of
 * structures describing the data (let's call it a "rawer"). The second phase
 * is an applier, which applies given rawer to the current element. --pasky */

/* Currently only the element's 'style' attribute is checked, therefore the
 * first stage is not exported yet (it'd be useless for that). Only the second
 * stage is available now, and it doesn't take the stylesheet argument yet.  It
 * will automatically scan the current element, and if a 'style' attribute is
 * found, it is parsed and applied to the current element. */


/* This function takes @element, applies its 'style' attribute onto its
 * attributes (if it contains such an attribute), and returns non-zero value
 * if any action was taken, zero if the @element was not modified. */
int css_apply(struct html_element *element);

#endif
