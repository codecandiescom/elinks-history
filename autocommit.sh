#!/bin/bash
cvs ci $* -f configure.in acconfig.h aclocal.m4 config.h.in Makefile.* src/Makefile.* src/*/Makefile.* src/*/*/Makefile.* configure stamp-h.in
