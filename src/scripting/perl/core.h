/* $Id: core.h,v 1.2 2004/04/23 15:51:39 pasky Exp $ */

#ifndef EL__SCRIPTING_PERL_CORE_H
#define EL__SCRIPTING_PERL_CORE_H

#ifdef HAVE_PERL

#include <EXTERN.h>
#include <perl.h>
#include <perlapi.h>

#include "modules/module.h"

extern PerlInterpreter *my_perl;
extern struct module perl_scripting_module;

#endif

#endif
