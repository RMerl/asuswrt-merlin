#! /usr/bin/perl
##
## Zebra interactive console
## Copyright (C) 2000 Vladimir B. Grebenschikov <vova@express.ru>
##
## This file is part of GNU Zebra.
##
## GNU Zebra is free software; you can redistribute it and/or modify it
## under the terms of the GNU General Public License as published by the
## Free Software Foundation; either version 2, or (at your option) any
## later version.
##
## GNU Zebra is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
## General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with GNU Zebra; see the file COPYING.  If not, write to the
## Free Software Foundation, Inc., 59 Temple Place - Suite 330,
## Boston, MA 02111-1307, USA.

use Net::Telnet ();
use Getopt::Std;

#use strict;

my $host = `hostname -s`; $host =~ s/\s//g;
my $port = 'zebra';
my $server = 'localhost';

# Check arguments
&getopts ('l:e:czborh');

&usage () if $opt_h;

# main 
{
  my $login_pass = $opt_l || $ENV{ZEBRA_PASSWORD} || 'zebra';
  my $enable_pass = $opt_e || $ENV{ZEBRA_ENABLE} || '';

  my $port = ($opt_z ? 'zebra' : 0) ||
	     ($opt_b ? 'bgpd' : 0) ||
             ($opt_o ? 'ospfd' : 0) ||
	     ($opt_r ? 'ripd' : 0) || 'zebra';

  my $cmd = join (' ', @ARGV);

  my $t = new Net::Telnet (Timeout => 10,
			   Prompt  => '/[\>\#] $/',
			   Port    => $port);

  $t->open ($server);

  $t->cmd ($login_pass);
  if ($enable_pass) {
      $t->cmd (String => 'en',
	       Prompt => '/Password: /');
      $t->cmd ($enable_pass);
  }
  $t->cmd ('conf t') if "$opt_c";

  if ($cmd)
    {
      docmd ($t, $cmd);
      exit (0); 
    }

  my $prompt = sprintf ("%s%s# ", $host,
			($port eq 'zebra') ? '' : "/$port");

  print "\nZEBRA interactive console ($port)\n\n" if -t STDIN;

  while (1)
    {
      $| = 1;
      print $prompt if -t STDIN;
      chomp ($cmd = <>);
      if (!defined ($cmd))
        {
	  print "\n" if -t STDIN;
	  exit(0);
        }
      exit (0) if ($cmd eq 'q' || $cmd eq 'quit');
 
      docmd ($t, $cmd) if $cmd !~ /^\s*$/;
    }

  exit(0);
}

sub docmd
{
  my ($t, $cmd) = @_;
  my @lines = $t->cmd ($cmd);
  print join ('', grep (!/[\>\#] $/, @lines)), "\n";
}

sub usage
{
  print "USAGE: $0 [-l LOGIN_PASSWORD] [-e ENABLE_PASSWORD] [-z|-b|-o|-r|-h] [<cmd>]\n",
        "\t-l - specify login password\n",
        "\t-e - specify enable password\n",
        "\t-c - execute command in configure mode\n",
        "\t-z - connect to zebra daemon\n",
        "\t-b - connect to bgpd  daemon\n",
        "\t-o - connect to ospfd daemon\n",
        "\t-r - connect to ripd  daemon\n",
        "\t-h - help\n";
  exit (1);
}
