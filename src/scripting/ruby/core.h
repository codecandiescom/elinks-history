/* $Id: core.h,v 1.1 2005/01/18 10:29:40 jonas Exp $ */

#ifndef EL__SCRIPTING_RUBY_CORE_H
#define EL__SCRIPTING_RUBY_CORE_H

VALUE erb_module;

void alert_ruby_error(unsigned char *msg);
void erb_report_error(int state);

#endif
