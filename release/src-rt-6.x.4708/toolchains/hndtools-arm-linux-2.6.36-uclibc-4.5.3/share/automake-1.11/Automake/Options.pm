# Copyright (C) 2003, 2004, 2006, 2007, 2008, 2009  Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

package Automake::Options;

use strict;
use Exporter;
use Automake::Config;
use Automake::ChannelDefs;
use Automake::Channels;
use Automake::Version;

use vars qw (@ISA @EXPORT);

@ISA = qw (Exporter);
@EXPORT = qw (option global_option
	      set_option set_global_option
	      unset_option unset_global_option
	      process_option_list process_global_option_list
	      set_strictness $strictness $strictness_name
	      &FOREIGN &GNU &GNITS);

=head1 NAME

Automake::Options - keep track of Automake options

=head1 SYNOPSIS

  use Automake::Options;

  # Option lookup and setting.
  $opt = option 'name';
  $opt = global_option 'name';
  set_option 'name', 'value';
  set_global_option 'name', 'value';
  unset_option 'name';
  unset_global_option 'name';

  # Batch option setting.
  process_option_list $location, @names;
  process_global_option_list $location, @names;

  # Strictness lookup and setting.
  set_strictness 'foreign';
  set_strictness 'gnu';
  set_strictness 'gnits';
  if ($strictness >= GNU) { ... }
  print "$strictness_name\n";

=head1 DESCRIPTION

This packages manages Automake's options and strictness settings.
Options can be either local or global.  Local options are set using an
C<AUTOMAKE_OPTIONS> variable in a F<Makefile.am> and apply only to
this F<Makefile.am>.  Global options are set from the command line or
passed as an argument to C<AM_INIT_AUTOMAKE>, they apply to all
F<Makefile.am>s.

=cut

# Values are the Automake::Location of the definition, except
# for 'ansi2knr' whose value is a pair [filename, Location].
use vars '%_options';		# From AUTOMAKE_OPTIONS
use vars '%_global_options';	# from AM_INIT_AUTOMAKE or the command line.

=head2 Constants

=over 4

=item FOREIGN

=item GNU

=item GNITS

Strictness constants used as values for C<$strictness>.

=back

=cut

# Constants to define the "strictness" level.
use constant FOREIGN => 0;
use constant GNU     => 1;
use constant GNITS   => 2;

=head2 Variables

=over 4

=item C<$strictness>

The current strictness.  One of C<FOREIGN>, C<GNU>, or C<GNITS>.

=item C<$strictness_name>

The current strictness name.  One of C<'foreign'>, C<'gnu'>, or C<'gnits'>.

=back

=cut

# Strictness levels.
use vars qw ($strictness $strictness_name);

# Strictness level as set on command line.
use vars qw ($_default_strictness $_default_strictness_name);


=head2 Functions

=over 4

=item C<Automake::Options::reset>

Reset the options variables for the next F<Makefile.am>.

In other words, this gets rid of all local options in use by the
previous F<Makefile.am>.

=cut

sub reset ()
{
  %_options = %_global_options;
  # The first time we are run,
  # remember the current setting as the default.
  if (defined $_default_strictness)
    {
      $strictness = $_default_strictness;
      $strictness_name = $_default_strictness_name;
    }
  else
    {
      $_default_strictness = $strictness;
      $_default_strictness_name = $strictness_name;
    }
}

=item C<$value = option ($name)>

=item C<$value = global_option ($name)>

Query the state of an option.  If the option is unset, this
returns the empty list.  Otherwise it returns the option's value,
as set by C<set_option> or C<set_global_option>.

Note that C<global_option> should be used only when it is
important to make sure an option hasn't been set locally.
Otherwise C<option> should be the standard function to
check for options (be they global or local).

=cut

sub option ($)
{
  my ($name) = @_;
  return () unless defined $_options{$name};
  return $_options{$name};
}

sub global_option ($)
{
  my ($name) = @_;
  return () unless defined $_global_options{$name};
  return $_global_options{$name};
}

=item C<set_option ($name, $value)>

=item C<set_global_option ($name, $value)>

Set an option.  By convention, C<$value> is usually the location
of the option definition.

=cut

sub set_option ($$)
{
  my ($name, $value) = @_;
  $_options{$name} = $value;
}

sub set_global_option ($$)
{
  my ($name, $value) = @_;
  $_global_options{$name} = $value;
}


=item C<unset_option ($name)>

=item C<unset_global_option ($name)>

Unset an option.

=cut

sub unset_option ($)
{
  my ($name) = @_;
  delete $_options{$name};
}

sub unset_global_option ($)
{
  my ($name) = @_;
  delete $_global_options{$name};
}


=item C<process_option_list ($where, @options)>

=item C<process_global_option_list ($where, @options)>

Process Automake's option lists.  C<@options> should be a list of
words, as they occur in C<AUTOMAKE_OPTIONS> or C<AM_INIT_AUTOMAKE>.

Return 1 on error, 0 otherwise.

=cut

# $BOOL
# _process_option_list (\%OPTIONS, $WHERE, @OPTIONS)
# -------------------------------------------------
# Process a list of options.  Return 1 on error, 0 otherwise.
# \%OPTIONS is the hash to fill with options data, $WHERE is
# the location where @OPTIONS occurred.
sub _process_option_list (\%$@)
{
  my ($options, $where, @list) = @_;

  foreach (@list)
    {
      $options->{$_} = $where;
      if ($_ eq 'gnits' || $_ eq 'gnu' || $_ eq 'foreign')
	{
	  set_strictness ($_);
	}
      elsif (/^(.*\/)?ansi2knr$/)
	{
	  # An option like "../lib/ansi2knr" is allowed.  With no
	  # path prefix, we assume the required programs are in this
	  # directory.  We save the actual option for later.
	  $options->{'ansi2knr'} = [$_, $where];
	}
      elsif ($_ eq 'no-installman' || $_ eq 'no-installinfo'
	     || $_ eq 'dist-shar' || $_ eq 'dist-zip'
	     || $_ eq 'dist-tarZ' || $_ eq 'dist-bzip2'
	     || $_ eq 'dist-lzma' || $_ eq 'dist-xz'
	     || $_ eq 'no-dist-gzip' || $_ eq 'no-dist'
	     || $_ eq 'dejagnu' || $_ eq 'no-texinfo.tex'
	     || $_ eq 'readme-alpha' || $_ eq 'check-news'
	     || $_ eq 'subdir-objects' || $_ eq 'nostdinc'
	     || $_ eq 'no-exeext' || $_ eq 'no-define'
	     || $_ eq 'std-options'
	     || $_ eq 'color-tests' || $_ eq 'parallel-tests'
	     || $_ eq 'cygnus' || $_ eq 'no-dependencies')
	{
	  # Explicitly recognize these.
	}
      elsif ($_ =~ /^filename-length-max=(\d+)$/)
	{
	  delete $options->{$_};
	  $options->{'filename-length-max'} = [$_, $1];
	}
      elsif ($_ eq  'silent-rules')
        {
	  error ($where,
		 "option `$_' can only be used as argument to AM_INIT_AUTOMAKE\n"
		 . "but not in AUTOMAKE_OPTIONS makefile statements")
	    if $where->get !~ /^configure\./;
	}
      elsif ($_ eq 'tar-v7' || $_ eq 'tar-ustar' || $_ eq 'tar-pax')
	{
	  error ($where,
		 "option `$_' can only be used as argument to AM_INIT_AUTOMAKE\n"
		 . "but not in AUTOMAKE_OPTIONS makefile statements")
	    if $where->get !~ /^configure\./;
	  for my $opt ('tar-v7', 'tar-ustar', 'tar-pax')
	    {
	      next if $opt eq $_;
	      if (exists $options->{$opt})
		{
		  error ($where,
			 "options `$_' and `$opt' are mutually exclusive");
		  last;
		}
	    }
	}
      elsif (/^\d+\.\d+(?:\.\d+)?[a-z]?(?:-[A-Za-z0-9]+)?$/)
	{
	  # Got a version number.
	  if (Automake::Version::check ($VERSION, $&))
	    {
	      error ($where, "require Automake $_, but have $VERSION",
		     uniq_scope => US_GLOBAL);
	      return 1;
	    }
	}
      elsif (/^(?:--warnings=|-W)(.*)$/)
	{
	  foreach my $cat (split (',', $1))
	    {
	      msg 'unsupported', $where, "unknown warning category `$cat'"
		if switch_warning $cat;
	    }
	}
      else
	{
	  error ($where, "option `$_' not recognized",
		 uniq_scope => US_GLOBAL);
	  return 1;
	}
    }
  return 0;
}

sub process_option_list ($@)
{
  my ($where, @list) = @_;
  return _process_option_list (%_options, $where, @list);
}

sub process_global_option_list ($@)
{
  my ($where, @list) = @_;
  return _process_option_list (%_global_options, $where, @list);
}

=item C<set_strictness ($name)>

Set the current strictness level.
C<$name> should be one of C<'foreign'>, C<'gnu'>, or C<'gnits'>.

=cut

# Set strictness.
sub set_strictness ($)
{
  $strictness_name = $_[0];

  Automake::ChannelDefs::set_strictness ($strictness_name);

  if ($strictness_name eq 'gnu')
    {
      $strictness = GNU;
    }
  elsif ($strictness_name eq 'gnits')
    {
      $strictness = GNITS;
    }
  elsif ($strictness_name eq 'foreign')
    {
      $strictness = FOREIGN;
    }
  else
    {
      prog_error "level `$strictness_name' not recognized\n";
    }
}

1;

### Setup "GNU" style for perl-mode and cperl-mode.
## Local Variables:
## perl-indent-level: 2
## perl-continued-statement-offset: 2
## perl-continued-brace-offset: 0
## perl-brace-offset: 0
## perl-brace-imaginary-offset: 0
## perl-label-offset: -2
## cperl-indent-level: 2
## cperl-brace-offset: 0
## cperl-continued-brace-offset: 0
## cperl-label-offset: -2
## cperl-extra-newline-before-brace: t
## cperl-merge-trailing-else: nil
## cperl-continued-statement-offset: 2
## End:
