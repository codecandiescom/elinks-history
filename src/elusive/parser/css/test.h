/* $Id: test.h,v 1.1 2003/02/25 14:15:50 jonas Exp $ */

#include "elusive/parser/css/state.h"
#include "elusive/parser/parser.h"

extern int css_stacksize;

unsigned char *
code2name(enum css_state_code code);

void
print_state(struct parser_state *state, unsigned char *src, int len);
	
void
print_token(unsigned char *desc, unsigned char *src, int len);

void
print_css_node(unsigned char *desc, struct css_node *node);
