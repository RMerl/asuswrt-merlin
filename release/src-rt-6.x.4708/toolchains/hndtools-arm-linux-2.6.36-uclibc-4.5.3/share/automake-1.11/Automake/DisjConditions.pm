# Copyright (C) 1997, 2001, 2002, 2003, 2004, 2006  Free Software Foundation, Inc.

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

package Automake::DisjConditions;

use Carp;
use strict;
use Automake::Condition qw/TRUE FALSE/;

=head1 NAME

Automake::DisjConditions - record a disjunction of Conditions

=head1 SYNOPSIS

  use Automake::Condition;
  use Automake::DisjConditions;

  # Create a Condition to represent "COND1 and not COND2".
  my $cond = new Automake::Condition "COND1_TRUE", "COND2_FALSE";
  # Create a Condition to represent "not COND3".
  my $other = new Automake::Condition "COND3_FALSE";

  # Create a DisjConditions to represent
  #   "(COND1 and not COND2) or (not COND3)"
  my $set = new Automake::DisjConditions $cond, $other;

  # Return the list of Conditions involved in $set.
  my @conds = $set->conds;

  # Return one of the Condition involved in $set.
  my $cond = $set->one_cond;

  # Return true iff $set is always true (i.e. its subconditions
  # cover all cases).
  if ($set->true) { ... }

  # Return false iff $set is always false (i.e. is empty, or contains
  # only false conditions).
  if ($set->false) { ... }

  # Return a string representing the DisjConditions.
  #   "COND1_TRUE COND2_FALSE | COND3_FALSE"
  my $str = $set->string;

  # Return a human readable string representing the DisjConditions.
  #   "(COND1 and !COND2) or (!COND3)"
  my $str = $set->human;

  # Merge (OR) several DisjConditions.
  my $all = $set->merge($set2, $set3, ...)

  # Invert a DisjConditions, i.e., create a new DisjConditions
  # that complements $set.
  my $inv = $set->invert;

  # Multiply two DisjConditions.
  my $prod = $set1->multiply ($set2);

  # Return the subconditions of a DisjConditions with respect to
  # a Condition.  See the description for a real example.
  my $subconds = $set->sub_conditions ($cond);

  # Check whether a new definition in condition $cond would be
  # ambiguous w.r.t. existing definitions in $set.
  ($msg, $ambig_cond) = $set->ambiguous_p ($what, $cond);

=head1 DESCRIPTION

A C<DisjConditions> is a disjunction of C<Condition>s.  In Automake
they are used to represent the conditions into which Makefile
variables and Makefile rules are defined.

If the variable C<VAR> is defined as

  if COND1
    if COND2
      VAR = value1
    endif
  endif
  if !COND3
    if COND4
      VAR = value2
    endif
  endif

then it will be associated a C<DisjConditions> created with
the following statement.

  new Automake::DisjConditions
    (new Automake::Condition ("COND1_TRUE", "COND2_TRUE"),
     new Automake::Condition ("COND3_FALSE", "COND4_TRUE"));

As you can see, a C<DisjConditions> is made from a list of
C<Condition>s.  Since C<DisjConditions> is a disjunction, and
C<Condition> is a conjunction, the above can be read as
follows.

  (COND1 and COND2) or ((not COND3) and COND4)

That's indeed the condition in which C<VAR> has a value.

Like C<Condition> objects, a C<DisjConditions> object is unique
with respect to its conditions.  Two C<DisjConditions> objects created
for the same set of conditions will have the same address.  This makes
it easy to compare C<DisjConditions>s: just compare the references.

=head2 Methods

=over 4

=item C<$set = new Automake::DisjConditions [@conds]>

Create a C<DisjConditions> object from the list of C<Condition>
objects passed in arguments.

If the C<@conds> list is empty, the C<DisjConditions> is assumed to be
false.

As explained previously, the reference (object) returned is unique
with respect to C<@conds>.  For this purpose, duplicate elements are
ignored.

=cut

# Keys in this hash are DisjConditions strings. Values are the
# associated object DisjConditions.  This is used by `new' to reuse
# DisjConditions objects with identical conditions.
use vars '%_disjcondition_singletons';

sub new ($;@)
{
  my ($class, @conds) = @_;
  my @filtered_conds = ();
  for my $cond (@conds)
    {
      confess "`$cond' isn't a reference" unless ref $cond;
      confess "`$cond' isn't an Automake::Condition"
	unless $cond->isa ("Automake::Condition");

      # This is a disjunction of conditions, so we drop
      # false conditions.  We'll always treat an "empty"
      # DisjConditions as false for this reason.
      next if $cond->false;

      push @filtered_conds, $cond;
    }

  my $string;
  if (@filtered_conds)
    {
      @filtered_conds = sort { $a->string cmp $b->string } @filtered_conds;
      $string = join (' | ', map { $_->string } @filtered_conds);
    }
  else
    {
      $string = 'FALSE';
    }

  # Return any existing identical DisjConditions.
  my $me = $_disjcondition_singletons{$string};
  return $me if $me;

  # Else, create a new DisjConditions.

  # Store conditions as keys AND as values, because blessed
  # objects are converted to strings when used as keys (so
  # at least we still have the value when we need to call
  # a method).
  my %h = map {$_ => $_} @filtered_conds;

  my $self = {
    hash => \%h,
    string => $string,
    conds => \@filtered_conds,
  };
  bless $self, $class;

  $_disjcondition_singletons{$string} = $self;
  return $self;
}


=item C<CLONE>

Internal special subroutine to fix up the self hashes in
C<%_disjcondition_singletons> upon thread creation.  C<CLONE> is invoked
automatically with ithreads from Perl 5.7.2 or later, so if you use this
module with earlier versions of Perl, it is not thread-safe.

=cut

sub CLONE
{
  foreach my $self (values %_disjcondition_singletons)
    {
      my %h = map { $_ => $_ } @{$self->{'conds'}};
      $self->{'hash'} = \%h;
    }
}


=item C<@conds = $set-E<gt>conds>

Return the list of C<Condition> objects involved in C<$set>.

=cut

sub conds ($ )
{
  my ($self) = @_;
  return @{$self->{'conds'}};
}

=item C<$cond = $set-E<gt>one_cond>

Return one C<Condition> object involved in C<$set>.

=cut

sub one_cond ($)
{
  my ($self) = @_;
  return (%{$self->{'hash'}},)[1];
}

=item C<$et = $set-E<gt>false>

Return 1 iff the C<DisjConditions> object is always false (i.e., if it
is empty, or if it contains only false C<Condition>s). Return 0
otherwise.

=cut

sub false ($ )
{
  my ($self) = @_;
  return 0 == keys %{$self->{'hash'}};
}

=item C<$et = $set-E<gt>true>

Return 1 iff the C<DisjConditions> object is always true (i.e. covers all
conditions). Return 0 otherwise.

=cut

sub true ($ )
{
  my ($self) = @_;
  return $self->invert->false;
}

=item C<$str = $set-E<gt>string>

Build a string which denotes the C<DisjConditions>.

=cut

sub string ($ )
{
  my ($self) = @_;
  return $self->{'string'};
}

=item C<$cond-E<gt>human>

Build a human readable string which denotes the C<DisjConditions>.

=cut

sub human ($ )
{
  my ($self) = @_;

  return $self->{'human'} if defined $self->{'human'};

  my $res = '';
  if ($self->false)
    {
      $res = 'FALSE';
    }
  else
    {
      my @c = $self->conds;
      if (1 == @c)
	{
	  $res = $c[0]->human;
	}
      else
	{
	  $res = '(' . join (') or (', map { $_->human } $self->conds) . ')';
	}
    }
  $self->{'human'} = $res;
  return $res;
}


=item C<$newcond = $cond-E<gt>merge (@otherconds)>

Return a new C<DisjConditions> which is the disjunction of
C<$cond> and C<@otherconds>.  Items in C<@otherconds> can be
@C<Condition>s or C<DisjConditions>.

=cut

sub merge ($@)
{
  my ($self, @otherconds) = @_;
  new Automake::DisjConditions (
    map { $_->isa ("Automake::DisjConditions") ? $_->conds : $_ }
        ($self, @otherconds));
}


=item C<$prod = $set1-E<gt>multiply ($set2)>

Multiply two conditional sets.

  my $set1 = new Automake::DisjConditions
    (new Automake::Condition ("A_TRUE"),
     new Automake::Condition ("B_TRUE"));
  my $set2 = new Automake::DisjConditions
    (new Automake::Condition ("C_FALSE"),
     new Automake::Condition ("D_FALSE"));

C<$set1-E<gt>multiply ($set2)> will return

  new Automake::DisjConditions
    (new Automake::Condition ("A_TRUE", "C_FALSE"),
     new Automake::Condition ("B_TRUE", "C_FALSE"),;
     new Automake::Condition ("A_TRUE", "D_FALSE"),
     new Automake::Condition ("B_TRUE", "D_FALSE"));

The argument can also be a C<Condition>.

=cut

# Same as multiply() but take a list of Conditionals as second argument.
# We use this in invert().
sub _multiply ($@)
{
  my ($self, @set) = @_;
  my @res = map { $_->multiply (@set) } $self->conds;
  return new Automake::DisjConditions (Automake::Condition::reduce_or @res);
}

sub multiply ($$)
{
  my ($self, $set) = @_;
  return $self->_multiply ($set) if $set->isa('Automake::Condition');
  return $self->_multiply ($set->conds);
}

=item C<$inv = $set-E<gt>invert>

Invert a C<DisjConditions>.  Return a C<DisjConditions> which is true
when C<$set> is false, and vice-versa.

  my $set = new Automake::DisjConditions
    (new Automake::Condition ("A_TRUE", "B_TRUE"),
     new Automake::Condition ("A_FALSE", "B_FALSE"));

Calling C<$set-E<gt>invert> will return the following C<DisjConditions>.

  new Automake::DisjConditions
    (new Automake::Condition ("A_TRUE", "B_FALSE"),
     new Automake::Condition ("A_FALSE", "B_TRUE"));

We implement the inversion by a product-of-sums to sum-of-products
conversion using repeated multiplications.  Because of the way we
implement multiplication, the result of inversion is in canonical
prime implicant form.

=cut

sub invert($ )
{
  my ($self) = @_;

  return $self->{'invert'} if defined $self->{'invert'};

  # The invert of an empty DisjConditions is TRUE.
  my $res = new Automake::DisjConditions TRUE;

  #   !((a.b)+(c.d)+(e.f))
  # = (!a+!b).(!c+!d).(!e+!f)
  # We develop this into a sum of product iteratively, starting from TRUE:
  # 1) TRUE
  # 2) TRUE.!a + TRUE.!b
  # 3) TRUE.!a.!c + TRUE.!b.!c + TRUE.!a.!d + TRUE.!b.!d
  # 4) TRUE.!a.!c.!e + TRUE.!b.!c.!e + TRUE.!a.!d.!e + TRUE.!b.!d.!e
  #    + TRUE.!a.!c.!f + TRUE.!b.!c.!f + TRUE.!a.!d.!f + TRUE.!b.!d.!f
  foreach my $cond ($self->conds)
    {
      $res = $res->_multiply ($cond->not);
    }

  # Cache result.
  $self->{'invert'} = $res;
  # It's tempting to also set $res->{'invert'} to $self, but that
  # is a bad idea as $self hasn't been normalized in any way.
  # (Different inputs can produce the same inverted set.)
  return $res;
}

=item C<$self-E<gt>simplify>

Return a C<Disjunction> which is a simplified canonical form of C<$self>.
This canonical form contains only prime implicants, but it can contain
non-essential prime implicants.

=cut

sub simplify ($)
{
  my ($self) = @_;
  return $self->invert->invert;
}

=item C<$self-E<gt>sub_conditions ($cond)>

Return the subconditions of C<$self> that contains C<$cond>, with
C<$cond> stripped.  More formally, return C<$res> such that
C<$res-E<gt>multiply ($cond) == $self-E<gt>multiply ($cond)> and
C<$res> does not mention any of the variables in C<$cond>.

For instance, consider:

  my $a = new Automake::DisjConditions
    (new Automake::Condition ("A_TRUE", "B_TRUE"),
     new Automake::Condition ("A_TRUE", "C_FALSE"),
     new Automake::Condition ("A_TRUE", "B_FALSE", "C_TRUE"),
     new Automake::Condition ("A_FALSE"));
  my $b = new Automake::DisjConditions
    (new Automake::Condition ("A_TRUE", "B_FALSE"));

Calling C<$a-E<gt>sub_conditions ($b)> will return the following
C<DisjConditions>.

  new Automake::DisjConditions
    (new Automake::Condition ("C_FALSE"), # From A_TRUE C_FALSE
     new Automake::Condition ("C_TRUE")); # From A_TRUE B_FALSE C_TRUE"

=cut

sub sub_conditions ($$)
{
  my ($self, $subcond) = @_;

  # Make $subcond blindingly apparent in the DisjConditions.
  # For instance `$b->multiply($a->conds)' (from the POD example) is:
  # 	(new Automake::Condition ("FALSE"),
  # 	 new Automake::Condition ("A_TRUE", "B_FALSE", "C_FALSE"),
  # 	 new Automake::Condition ("A_TRUE", "B_FALSE", "C_TRUE"),
  # 	 new Automake::Condition ("FALSE"))
  my @prodconds = $subcond->multiply ($self->conds);

  # Now, strip $subcond from the remaining (i.e., non-false) Conditions.
  my @res = map { $_->false ? () : $_->strip ($subcond) } @prodconds;

  return new Automake::DisjConditions @res;
}

=item C<($string, $ambig_cond) = $condset-E<gt>ambiguous_p ($what, $cond)>

Check for an ambiguous condition.  Return an error message and the
other condition involved if we have an ambiguity.  Return an empty
string and FALSE otherwise.

C<$what> is the name of the thing being defined, to use in the error
message.  C<$cond> is the C<Condition> under which it is being
defined.  C<$condset> is the C<DisjConditions> under which it had
already been defined.

=cut

sub ambiguous_p ($$$)
{
  my ($self, $var, $cond) = @_;

  # Note that these rules don't consider the following
  # example as ambiguous.
  #
  #   if COND1
  #     FOO = foo
  #   endif
  #   if COND2
  #     FOO = bar
  #   endif
  #
  # It's up to the user to not define COND1 and COND2
  # simultaneously.

  return ("$var multiply defined in condition " . $cond->human, $cond)
    if exists $self->{'hash'}{$cond};

  foreach my $vcond ($self->conds)
    {
      return ("$var was already defined in condition " . $vcond->human
	      . ", which includes condition ". $cond->human, $vcond)
	if $vcond->true_when ($cond);

      return ("$var was already defined in condition " . $vcond->human
	      . ", which is included in condition " . $cond->human, $vcond)
	if $cond->true_when ($vcond);
    }
  return ('', FALSE);
}

=head1 SEE ALSO

L<Automake::Condition>.

=head1 HISTORY

C<AM_CONDITIONAL>s and supporting code were added to Automake 1.1o by
Ian Lance Taylor <ian@cygnus.org> in 1997.  Since then it has been
improved by Tom Tromey <tromey@redhat.com>, Richard Boulton
<richard@tartarus.org>, Raja R Harinath <harinath@cs.umn.edu>, Akim
Demaille <akim@epita.fr>, Pavel Roskin <proski@gnu.org>, and
Alexandre Duret-Lutz <adl@gnu.org>.

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
