#!/bin/sh

## get some parameters from the makefile

srcdir=$1
top_builddir=$2
export SHELL srcdir top_builddir

$3
