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

package Automake::ItemDef;
use strict;
use Carp;

=head1 NAME

Automake::ItemDef - base class for Automake::VarDef and Automake::RuleDef

=head1 DESCRIPTION

=head2 Methods

=over 4

=item C<my $def = Automake::new ($comment, $location, $owner)>

Create a new Makefile-item definition.

C<$comment> is any comment preceding the definition.  (Because
Automake reorders items in the output, it also tries to carry comments
around.)

C<$location> is the place where the definition occurred, it should be
an instance of L<Automake::Location>.

C<$owner> specifies who owns the rule.

=cut

sub new ($$$$)
{
  my ($class, $comment, $location, $owner) = @_;

  my $self = {
    comment => $comment,
    location => $location,
    owner => $owner,
  };
  bless $self, $class;

  return $self;
}

=item C<$def-E<gt>comment>

=item C<$def-E<gt>location>

=item C<$def-E<gt>owner>

Accessors to the various constituents of an C<ItemDef>.  See the
documentation of C<new>'s arguments for a description of these.

=cut

sub comment ($)
{
  my ($self) = @_;
  return $self->{'comment'};
}

sub location ($)
{
  my ($self) = @_;
  return $self->{'location'};
}

sub owner ($)
{
  my ($self) = @_;
  return $self->{'owner'};
}

=head1 SEE ALSO

L<Automake::VarDef>, and L<Automake::RuleDef>.

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
