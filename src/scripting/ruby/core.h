/* $Id: core.h,v 1.2 2005/01/18 15:07:55 jonas Exp $ */

#ifndef EL__SCRIPTING_RUBY_CORE_H
#define EL__SCRIPTING_RUBY_CORE_H

struct session;

VALUE erb_module;

void alert_ruby_error(struct session *ses, unsigned char *msg);
void erb_report_error(struct session *ses, int state);

#endif
