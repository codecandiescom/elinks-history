#!/bin/bash
cvs ci $* -f configure.in acconfig.h aclocal.m4 config.h.in Makefile.in src/Makefile.in src/*/Makefile.in src/*/*/Makefile.in configure stamp-h.in
