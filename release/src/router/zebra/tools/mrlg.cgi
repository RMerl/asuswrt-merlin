#!/usr/bin/perl
##
## Zebra Looking Glass version 1.0
## 01 FEB 2000
## Copyright (C) 2000 John W. Fraizer III <john.fraizer@enterzone.net>
## *All* copyright notices must remain in place to use this code.
##
## The latest version of this code is available at:
## ftp://ftp.enterzone.net/looking-glass/
##
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

require 5.002;
use POSIX;
use Net::Telnet ();



## Set the URL for your site.
$url="http://www.sample.com/mrlg.cgi";

## Set your router variables in sub set_router and modify the selections in Main to match.


############################################################
#Main
############################################################
{

## Set the router default
@Form{'router'} = "router1";

## Get the form results now so we can override the default router 
get_form();

print "Content-type: text/html\n\n";

print '
<html>
<head>
<title>Multi-Router Looking Glass for Zebra</title>
</head>
<body bgcolor=white>
	
<font face=arial size=3 color=blue>
<h1>Multi-Router Looking Glass for Zebra</h1>
Copyright 2000 - John Fraizer, EnterZone Inc.
<br>
';

print '
<font color=black>
';
print "<form METHOD=\"POST\" action=\"$url\">\n";
print "<B>Router:</B>  <SELECT Name=\"router\" Size=1>\n";
print "<OPTION Value=\"$Form{'router'}\">$Form{'router'}\n";
print '
<OPTION Value="router1">router1
<OPTION Value="router2">router2
<OPTION Value="router3">router3
<OPTION Value="router4">router4
</select>
<br><br>
<B>Query</B>:
<br>
<input type=radio name=query value=1>show ip bgp<br>
<input type=radio name=query value=2>show ip bgp summary<br>
<input type=radio name=query value=3>show ip route<br>
<input type=radio name=query value=4>show interface<br>
<input type=radio name=query value=5>show ipv6 bgp<br>
<input type=radio name=query value=6>show ipv6 bgp summary<br>
<input type=radio name=query value=7>show ipv6 route<br>
<br>
<B>Argument:</B> <input type=text name=arg length=20 maxlength=60>
<input type="submit" value="Execute"></form>	
';

## Set up the address, pw and ports, etc for the selected router.
set_router();

## Set up which command is to be executed (and then execute it!)
set_command();


print '
<br><br>
</font>
<font color=blue face=arial size=2>
Multi-Router Looking Glass for Zebra version 1.0<br>
Written by: John Fraizer -
<a href="http://www.ez-hosting.net/">EnterZone, Inc</a><br>
Source code: <a href="ftp://ftp.enterzone.net/looking-glass/">ftp://ftp.enterzone.net/looking-glass/</a>
</body>
</html>
';

## All done!

exit (0); 
}


############################################################
sub get_form
############################################################
{
        
        #read STDIN
        read(STDIN, $buffer, $ENV{'CONTENT_LENGTH'});

        # Split the name-value pairs
        @pairs = split(/&/, $buffer);
  
        # For each name-value pair:
        foreach $pair (@pairs)
                {
                
                # Split the pair up into individual variables.
                local($name, $value) = split(/=/, $pair);

                # Decode the form encoding on the name and value variables.
                $name =~ tr/+/ /;
                $name =~ s/%([a-fA-F0-9][a-fA-F0-9])/pack("C", hex($1))/eg;
 
                $value =~ tr/+/ /;
                $value =~ s/%([a-fA-F0-9][a-fA-F0-9])/pack("C", hex($1))/eg;

                # If they try to include server side includes, erase them, so they
                # aren't a security risk if the html gets returned.  Another
                # security hole plugged up.
                $value =~ s/<!--(.|\n)*-->//g;
        
                @Form{$name} = $value ;
                 
                }
        
}       

############################################################
sub set_router
############################################################

## $server is the IP address of the router running zebra
## $login_pass is the password of the router
## $bgpd is the port that bgpd will answer on
## $zebra is the port that zebra will answer on
## if $zebra is "", it will disable sh ip route and sh int for that router.
## if $full_tables is set to "1" for a router, full BGP and IP ROUTE table dumps will be allowed via the looking glass.
## This is a BAD thing to do if you have multiple full views on a router.  That's why the option is there.

{
if ($Form{'router'} eq 'router1')
        {
$server = '10.1.1.1';
$login_pass = 'zebra';
$bgpd = "2605";
$zebra = "";
$full_tables=1;

        }

elsif ($Form{'router'} eq 'router2')
        {
$server = '10.1.1.2';
$login_pass = 'zebra';
$bgpd = "2605";
$zebra = "2601";
        }

elsif ($Form{'router'} eq 'router3')
        {
$server = '10.1.1.3';
$login_pass = 'zebra';
$bgpd = "2605";
$zebra = "2601";
$full_tables=1;
        }

elsif ($Form{'router'} eq 'router4')
        {
$server = '10.1.1.4';
$login_pass = 'zebra';
$bgpd = "2605";
$zebra = "2601";
        }


}


############################################################
sub set_command
############################################################
{
if ($Form{'query'} eq '1')
	{
	sh_ip_bgp('ip');
	}

elsif ($Form{'query'} eq '2')
	{
	sh_ip_bgp_sum('ip');
	}

if ($Form{'query'} eq '3')
	{
	sh_ip_route('ip');
	}

if ($Form{'query'} eq '4')
	{
	sh_int();
	}
if ($Form{'query'} eq '5')
	{
	sh_ip_bgp('ipv6');
	}
if ($Form{'query'} eq '6')
	{
	sh_ip_bgp_sum('ipv6');
	}
if ($Form{'query'} eq '7')
	{
	sh_ip_route('ipv6');
	}
}
############################################################
sub sh_ip_bgp
############################################################
{
my $protocol = shift(@_);
$port = $bgpd;
if ($protocol ne 'ip' && $protocol ne 'ipv6')
	{
	print "Invalid protocol: $protocol\n";
	print "protocol must be 'ip' or 'ipv6'\n\n";
	return;
	}
$command = "show $protocol bgp $Form{'arg'}";
if ($Form{'arg'} eq '')
	{
	if ($full_tables eq '1')
		{
		execute_command();
		}
	else
		{
		print "Sorry.  Displaying the FULL routing table would put too much load on the router!\n\n";
		}
	}
else
	{
	execute_command();
	}
}

############################################################
sub sh_ip_bgp_sum
############################################################
{
	my $protocol = shift(@_);
	$port = $bgpd;
	if ($protocol ne 'ip' && $protocol ne 'ipv6')
		{
		print "Invalid protocol: $protocol\n";
		print "protocol must be 'ip' or 'ipv6'\n\n";
		return;
		}
	$command = "show $protocol bgp summary";
	execute_command();
}

############################################################
sub sh_ip_route
############################################################
{

if ($zebra eq '')
	{
	print "Sorry. The <b>show ip route</b> command is disabled for this router."
	}
else
	{

	$port = $zebra;
	my $protocol = shift(@_);
	if ($protocol ne 'ip' && $protocol ne 'ipv6')
		{
		print "Invalid protocol: $protocol\n";
		print "protocol must be 'ip' or 'ipv6'\n\n";
		return;
		}
	$command = "show $protocol route $Form{'arg'}";
	if ($Form{'arg'} eq '')
		{
		if ($full_tables eq '1')
			{
			execute_command();
			}
		else
			{
			print "Sorry.  Displaying the FULL routing table would put too much load on the router!\n\n";
			}
		}
	else
		{
		execute_command();
		}
	}
}

############################################################
sub sh_int
############################################################
{
if ($zebra eq '')
	{
	print "Sorry. The <b>show interface</b> command is disabled for this router."
	}
else
	{
	$port = $zebra;
	$command = "show interface $Form{'arg'}";
	execute_command();
	}
}



############################################################
sub execute_command
############################################################
## This code is based on:
##
## Zebra interactive console
## Copyright (C) 2000 Vladimir B. Grebenschikov <vova@express.ru>
##


{

print "Executing command = $command";

#  my $port = ($opt_z ? 'zebra' : 0) ||
#             ($opt_b ? 'bgpd' : 0) ||
#             ($opt_o ? 'ospfd' : 0) ||
#	     ($opt_r ? 'ripd' : 0) || 'bgpd';

my $cmd = $command;


  my $t = new Net::Telnet (Timeout => 10,
			   Prompt  => '/[\>\#] $/',
			   Port    => $port);

  $t->open ($server);

  $t->cmd ($login_pass);

  if ($cmd)
    {
      docmd ($t, $cmd);
    }

}

############################################################
sub docmd
############################################################
{
  my ($t, $cmd) = @_;
  my @lines = $t->cmd ($cmd);
  print "<pre>\n";
  print join ('', grep (!/[\>\#] $/, @lines)), "\n";
  print "</pre>";
}



