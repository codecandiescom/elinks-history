#!/bin/bash
cvs ci -m "$1" -f configure.in acconfig.h aclocal.m4 config.h.in Makefile.in configure stamp-h.in
