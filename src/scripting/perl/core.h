/* $Id: core.h,v 1.1 2004/03/02 17:06:15 witekfl Exp $ */

#ifndef EL__SCRIPTING_PERL_CORE_H
#define EL__SCRIPTING_PERL_CORE_H

#ifdef HAVE_PERL

#include <EXTERN.h>
#include <perl.h>
#include <perlapi.h>

#include "modules/module.h"
#include "sched/event.h"
#include "sched/session.h"

extern PerlInterpreter *my_perl;
extern struct module perl_scripting_module;

#endif

#endif
