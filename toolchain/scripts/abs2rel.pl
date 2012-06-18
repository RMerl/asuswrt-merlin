#!/usr/bin/perl
# 
# Copyright (C) 2007 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
# $Id: abs2rel.pl 8570 2007-09-01 15:20:33Z nico $

use strict;
require File::Spec;

my $source = shift @ARGV;
my $target = shift @ARGV;
my $result = File::Spec->abs2rel($source, $target);

print "$result\n";
