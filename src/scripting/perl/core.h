/* $Id: core.h,v 1.5 2005/04/01 09:47:28 zas Exp $ */

#ifndef EL__SCRIPTING_PERL_CORE_H
#define EL__SCRIPTING_PERL_CORE_H

#ifdef CONFIG_PERL

#include <EXTERN.h>
#include <perl.h>
#include <perlapi.h>

struct module;

extern PerlInterpreter *my_perl;

void init_perl(struct module *module);
void cleanup_perl(struct module *module);

#endif

#endif
