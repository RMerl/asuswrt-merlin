# Copyright (C) 2003  Free Software Foundation, Inc.

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

package Automake::RuleDef;
use strict;
use Carp;
use Automake::ChannelDefs;
use Automake::ItemDef;

require Exporter;
use vars '@ISA', '@EXPORT';
@ISA = qw/Automake::ItemDef Exporter/;
@EXPORT = qw (&RULE_AUTOMAKE &RULE_USER);

=head1 NAME

Automake::RuleDef - a class for rule definitions

=head1 SYNOPSIS

  use Automake::RuleDef;
  use Automake::Location;

=head1 DESCRIPTION

This class gathers data related to one Makefile-rule definition.

=head2 Constants

=over 4

=item C<RULE_AUTOMAKE>, C<RULE_USER>

Possible owners for rules.

=cut

use constant RULE_AUTOMAKE => 0; # Rule defined by Automake.
use constant RULE_USER => 1;     # Rule defined in the user's Makefile.am.

sub new ($$$$$)
{
  my ($class, $name, $comment, $location, $owner, $source) = @_;

  my $self = Automake::ItemDef::new ($class, $comment, $location, $owner);
  $self->{'source'} = $source;
  $self->{'name'} = $name;
  return $self;
}

sub source ($)
{
  my ($self) = @_;
  return $self->{'source'};
}

sub name ($)
{
  my ($self) = @_;
  return $self->{'name'};
}

=back

=head1 SEE ALSO

L<Automake::Rule>, L<Automake::ItemDef>.

=cut

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
