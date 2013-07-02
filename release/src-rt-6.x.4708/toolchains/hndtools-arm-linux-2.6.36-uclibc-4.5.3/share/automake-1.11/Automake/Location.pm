# Copyright (C) 2002, 2003, 2008  Free Software Foundation, Inc.

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

package Automake::Location;

=head1 NAME

Automake::Location - a class for location tracking, with a stack of contexts

=head1 SYNOPSIS

  use Automake::Location;

  # Create a new Location object
  my $where = new Automake::Location "foo.c:13";

  # Change the location
  $where->set ("foo.c:14");

  # Get the location (without context).
  # Here this should print "foo.c:14"
  print $where->get, "\n";

  # Push a context, and change the location
  $where->push_context ("included from here");
  $where->set ("bar.h:1");

  # Print the location and the stack of context (for debugging)
  print $where->dump;
  # This should display
  #   bar.h:1:
  #   foo.c:14:   included from here

  # Get the contexts (list of [$location_string, $description])
  for my $pair (reverse $where->contexts)
    {
      my ($loc, $descr) = @{$pair};
      ...
    }

  # Pop a context, and reset the location from the previous context.
  $where->pop_context;

  # Clone a Location.  Use this when storing the state of a location
  # that would otherwise be modified.
  my $where_copy = $where->clone;

  # Serialize a Location object (for passing through a thread queue,
  # for example)
  my @array = $where->serialize ();

  # De-serialize: recreate a Location object from a queue.
  my $where = new Automake::Location::deserialize ($queue);

=head1 DESCRIPTION

C<Location> objects are used to keep track of locations in Automake,
and used to produce diagnostics.

A C<Location> object is made of two parts: a location string, and
a stack of contexts.

For instance if C<VAR> is defined at line 1 in F<bar.h> which was
included at line 14 in F<foo.c>, then the location string should be
C<"bar.h:10"> and the context should be the pair (C<"foo.c:14">,
C<"included from here">).

Section I<SYNOPSIS> shows how to setup such a C<Location>, and access
the location string or the stack of contexts.

You can pass a C<Location> to C<Automake::Channels::msg>.

=cut

sub new ($;$)
{
  my ($class, $position) = @_;
  my $self = {
    position => $position,
    contexts => [],
  };
  bless $self, $class;
  return $self;
}

sub set ($$)
{
  my ($self, $position) = @_;
  $self->{'position'} = $position;
}

sub get ($)
{
  my ($self) = @_;
  return $self->{'position'};
}

sub push_context ($$)
{
  my ($self, $context) = @_;
  push @{$self->{'contexts'}}, [$self->get, $context];
  $self->set (undef);
}

sub pop_context ($)
{
  my ($self) = @_;
  my $pair = pop @{$self->{'contexts'}};
  $self->set ($pair->[0]);
  return @{$pair};
}

sub get_contexts ($)
{
  my ($self) = @_;
  return @{$self->{'contexts'}};
}

sub clone ($)
{
  my ($self) = @_;
  my $other = new Automake::Location ($self->get);
  my @contexts = $self->get_contexts;
  for my $pair (@contexts)
    {
      push @{$other->{'contexts'}}, [@{$pair}];
    }
  return $other;
}

sub dump ($)
{
  my ($self) = @_;
  my $res = ($self->get || 'INTERNAL') . ":\n";
  for my $pair (reverse $self->get_contexts)
    {
      $res .= $pair->[0] || 'INTERNAL';
      $res .= ": $pair->[1]\n";
    }
  return $res;
}

sub serialize ($)
{
  my ($self) = @_;
  my @serial = ();
  push @serial, $self->get;
  my @contexts = $self->get_contexts;
  for my $pair (@contexts)
    {
      push @serial, @{$pair};
    }
  push @serial, undef;
  return @serial;
}

sub deserialize ($)
{
  my ($queue) = @_;
  my $position = $queue->dequeue ();
  my $self = new Automake::Location $position;
  while (my $position = $queue->dequeue ())
    {
      my $context = $queue->dequeue ();
      push @{$self->{'contexts'}}, [$position, $context];
    }
  return $self;
}

=head1 SEE ALSO

L<Automake::Channels>

=head1 HISTORY

Written by Alexandre Duret-Lutz E<lt>F<adl@gnu.org>E<gt>.

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
