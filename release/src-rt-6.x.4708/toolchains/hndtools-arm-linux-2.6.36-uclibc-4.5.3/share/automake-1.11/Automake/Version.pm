# Copyright (C) 2001, 2002, 2003  Free Software Foundation, Inc.

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

package Automake::Version;
use strict;
use Automake::ChannelDefs;

=head1 NAME

Automake::Version - version comparison

=head1 SYNOPSIS

  use Automake::Version;

  print "Version $version is older than required version $required\n"
    if Automake::Version::check ($version, $required);

=head1 DESCRIPTION

This module provides support for comparing versions string
as they are used in Automake.

A version is a string that looks like
C<MAJOR.MINOR[.MICRO][ALPHA][-FORK]> where C<MAJOR>, C<MINOR>, and
C<MICRO> are digits, C<ALPHA> is a character, and C<FORK> any
alphanumeric word.

Usually, C<ALPHA> is used to label alpha releases or intermediate
snapshots, C<FORK> is used for CVS branches or patched releases, and
C<MICRO> is used for bug fixes releases on the C<MAJOR.MINOR> branch.

For the purpose of ordering, C<1.4> is the same as C<1.4.0>, but
C<1.4g> is the same as C<1.4.99g>.  The C<FORK> identifier is ignored
in the ordering, except when it looks like C<-pMINOR[ALPHA]>: some
versions were labeled like C<1.4-p3a>, this is the same as an alpha
release labeled C<1.4.3a>.  Yes, it's horrible, but Automake did not
support two-dot versions in the past.

=head2 FUNCTIONS

=over 4

=item C<split ($version)>

Split the string C<$version> into the corresponding C<(MAJOR, MINOR,
MICRO, ALPHA, FORK)> tuple.  For instance C<'1.4g'> would be split
into C<(1, 4, 99, 'g', '')>.  Return C<()> on error.

=cut

sub split ($)
{
  my ($ver) = @_;

  # Special case for versions like 1.4-p2a.
  if ($ver =~ /^(\d+)\.(\d+)(?:-p(\d+)([a-z]+)?)$/)
  {
    return ($1, $2, $3, $4 || '', '');
  }
  # Common case.
  elsif ($ver =~ /^(\d+)\.(\d+)(?:\.(\d+))?([a-z])?(?:-([A-Za-z0-9]+))?$/)
  {
    return ($1, $2, $3 || (defined $4 ? 99 : 0), $4 || '', $5 || '');
  }
  return ();
}

=item C<compare (\@LVERSION, \@RVERSION)>

Compare two version tuples, as returned by C<split>.

Return 1, 0, or -1, if C<LVERSION> is found to be respectively
greater than, equal to, or less than C<RVERSION>.

=cut

sub compare (\@\@)
{
  my @l = @{$_[0]};
  my @r = @{$_[1]};

  for my $i (0, 1, 2)
  {
    return 1  if ($l[$i] > $r[$i]);
    return -1 if ($l[$i] < $r[$i]);
  }
  for my $i (3, 4)
  {
    return 1  if ($l[$i] gt $r[$i]);
    return -1 if ($l[$i] lt $r[$i]);
  }
  return 0;
}

=item C<check($VERSION, $REQUIRED)>

Handles the logic of requiring a version number in Automake.
C<$VERSION> should be Automake's version, while C<$REQUIRED>
is the version required by the user input.

Return 0 if the required version is satisfied, 1 otherwise.

=cut

sub check ($$)
{
  my ($version, $required) = @_;
  my @version = Automake::Version::split ($version);
  my @required = Automake::Version::split ($required);

  prog_error "version is incorrect: $version"
    if $#version == -1;

  # This should not happen, because process_option_list and split_version
  # use similar regexes.
  prog_error "required version is incorrect: $required"
    if $#required == -1;

  # If we require 3.4n-foo then we require something
  # >= 3.4n, with the `foo' fork identifier.
  return 1
    if ($required[4] ne '' && $required[4] ne $version[4]);

  return 0 > compare (@version, @required);
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
