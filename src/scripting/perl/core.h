/* $Id: core.h,v 1.4 2005/01/03 03:40:51 jonas Exp $ */

#ifndef EL__SCRIPTING_PERL_CORE_H
#define EL__SCRIPTING_PERL_CORE_H

#ifdef CONFIG_PERL

#include <EXTERN.h>
#include <perl.h>
#include <perlapi.h>

extern PerlInterpreter *my_perl;

#endif

#endif
