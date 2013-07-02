# Copyright (C) 2003, 2006  Free Software Foundation, Inc.

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

package Automake::Wrap;

use strict;

require Exporter;
use vars '@ISA', '@EXPORT_OK';
@ISA = qw/Exporter/;
@EXPORT_OK = qw/wrap makefile_wrap/;

=head1 NAME

Automake::Wrap - a paragraph formatter

=head1 SYNOPSIS

  use Automake::Wrap 'wrap', 'makefile_wrap';

  print wrap ($first_ident, $next_ident, $end_of_line, $max_length,
              @values);

  print makefile_wrap ("VARIABLE = ", "    ", @values);

=head1 DESCRIPTION

This modules provide facility to format list of strings.  It is
comparable to Perl's L<Text::Wrap>, however we can't use L<Text::Wrap>
because some versions will abort when some word to print exceeds the
maximum length allowed.  (Ticket #17141, fixed in Perl 5.8.0.)

=head2 Functions

=over 4

=cut

# tab_length ($TXT)
# -----------------
# Compute the length of TXT, counting tab characters as 8 characters.
sub tab_length($)
{
  my ($txt) = @_;
  my $len = length ($txt);
  $len += 7 * ($txt =~ tr/\t/\t/);
  return $len;
}

=item C<wrap ($head, $fill, $eol, $max_len, @values)>

Format C<@values> as a block of text that starts with C<$head>,
followed by the strings in C<@values> separated by spaces or by
C<"$eol\n$fill"> so that the length of each line never exceeds
C<$max_len>.

The C<$max_len> constraint is ignored for C<@values> items which
are too big to fit alone one a line.

The constructed paragraph is C<"\n">-terminated.

=cut

sub wrap($$$$@)
{
  my ($head, $fill, $eol, $max_len, @values) = @_;

  my $result = $head;
  my $column = tab_length ($head);

  my $fill_len = tab_length ($fill);
  my $eol_len = tab_length ($eol);

  my $not_first_word = 0;

  foreach (@values)
    {
      my $len = tab_length ($_);

      # See if the new variable fits on this line.
      # (The + 1 is for the space we add in front of the value.).
      if ($column + $len + $eol_len + 1 > $max_len
	  # Do not break before the first word if it does not fit on
	  # the next line anyway.
	  && ($not_first_word || $fill_len + $len + $eol_len + 1 <= $max_len))
	{
	  # Start a new line.
	  $result .= "$eol\n" . $fill;
	  $column = $fill_len;
	}
      elsif ($not_first_word)
	{
	  # Add a space only if result does not already end
	  # with a space.
	  $_ = " $_" if $result =~ /\S\z/;
	  ++$len;
	}
      $result .= $_;
      $column += $len;
      $not_first_word = 1;
    }

  $result .= "\n";
  return $result;
}


=item C<makefile_wrap ($head, $fill, @values)>

Format C<@values> in a way which is suitable for F<Makefile>s.
This is comparable to C<wrap>, except C<$eol> is known to
be C<" \\">, and the maximum length has been hardcoded to C<72>.

A space is appended to C<$head> when this is not already
the case.

This can be used to format variable definitions or dependency lines.

  makefile_wrap ('VARIABLE =', "\t", @values);
  makefile_wrap ('rule:', "\t", @dependencies);

=cut

sub makefile_wrap ($$@)
{
  my ($head, $fill, @values) = @_;
  if (@values)
    {
      $head .= ' ' if $head =~ /\S\z/;
      return wrap $head, $fill, " \\", 72, @values;
    }
  return "$head\n";
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
