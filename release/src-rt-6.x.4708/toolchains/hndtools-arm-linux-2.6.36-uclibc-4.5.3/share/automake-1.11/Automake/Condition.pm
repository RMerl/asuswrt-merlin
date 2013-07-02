# Copyright (C) 1997, 2001, 2002, 2003, 2006, 2008  Free Software
# Foundation, Inc.

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

package Automake::Condition;
use strict;
use Carp;

require Exporter;
use vars '@ISA', '@EXPORT_OK';
@ISA = qw/Exporter/;
@EXPORT_OK = qw/TRUE FALSE reduce_and reduce_or/;

=head1 NAME

Automake::Condition - record a conjunction of conditionals

=head1 SYNOPSIS

  use Automake::Condition;

  # Create a condition to represent "COND1 and not COND2".
  my $cond = new Automake::Condition "COND1_TRUE", "COND2_FALSE";
  # Create a condition to represent "not COND3".
  my $other = new Automake::Condition "COND3_FALSE";

  # Create a condition to represent
  #   "COND1 and not COND2 and not COND3".
  my $both = $cond->merge ($other);

  # Likewise, but using a list of conditional strings
  my $both2 = $cond->merge_conds ("COND3_FALSE");

  # Strip from $both any subconditions which are in $other.
  # This is the opposite of merge.
  $cond = $both->strip ($other);

  # Return the list of conditions ("COND1_TRUE", "COND2_FALSE"):
  my @conds = $cond->conds;

  # Is $cond always true?  (Not in this example)
  if ($cond->true) { ... }

  # Is $cond always false? (Not in this example)
  if ($cond->false) { ... }

  # Return the list of conditionals as a string:
  #  "COND1_TRUE COND2_FALSE"
  my $str = $cond->string;

  # Return the list of conditionals as a human readable string:
  #  "COND1 and !COND2"
  my $str = $cond->human;

  # Return the list of conditionals as a AC_SUBST-style string:
  #  "@COND1_TRUE@@COND2_FALSE@"
  my $subst = $cond->subst_string;

  # Is $cond true when $both is true?  (Yes in this example)
  if ($cond->true_when ($both)) { ... }

  # Is $cond redundant w.r.t. {$other, $both}?
  # (Yes in this example)
  if ($cond->redundant_wrt ($other, $both)) { ... }

  # Does $cond imply any of {$other, $both}?
  # (Not in this example)
  if ($cond->implies_any ($other, $both)) { ... }

  # Remove superfluous conditionals assuming they will eventually
  # be multiplied together.
  # (Returns @conds = ($both) in this example, because
  # $other and $cond are implied by $both.)
  @conds = Automake::Condition::reduce_and ($other, $both, $cond);

  # Remove superfluous conditionals assuming they will eventually
  # be summed together.
  # (Returns @conds = ($cond, $other) in this example, because
  # $both is a subset condition of $cond: $cond is true whenever $both
  # is true.)
  @conds = Automake::Condition::reduce_or ($other, $both, $cond);

  # Invert a Condition.  This returns a list of Conditions.
  @conds = $both->not;

=head1 DESCRIPTION

A C<Condition> is a conjunction of conditionals (i.e., atomic conditions
defined in F<configure.ac> by C<AM_CONDITIONAL>.  In Automake they
are used to represent the conditions into which F<Makefile> variables and
F<Makefile> rules are defined.

If the variable C<VAR> is defined as

  if COND1
    if COND2
      VAR = value
    endif
  endif

then it will be associated a C<Condition> created with
the following statement.

  new Automake::Condition "COND1_TRUE", "COND2_TRUE";

Remember that a C<Condition> is a I<conjunction> of conditionals, so
the above C<Condition> means C<VAR> is defined when C<COND1>
B<and> C<COND2> are true. There is no way to express disjunctions
(i.e., I<or>s) with this class (but see L<DisjConditions>).

Another point worth to mention is that each C<Condition> object is
unique with respect to its conditionals.  Two C<Condition> objects
created for the same set of conditionals will have the same address.
This makes it easy to compare C<Condition>s: just compare the
references.

  my $c1 = new Automake::Condition "COND1_TRUE", "COND2_TRUE";
  my $c2 = new Automake::Condition "COND1_TRUE", "COND2_TRUE";
  $c1 == $c2;  # True!

=head2 Methods

=over 4

=item C<$cond = new Automake::Condition [@conds]>

Return a C<Condition> objects for the conjunctions of conditionals
listed in C<@conds> as strings.

An item in C<@conds> should be either C<"FALSE">, C<"TRUE">, or have
the form C<"NAME_FALSE"> or C<"NAME_TRUE"> where C<NAME> can be
anything (in practice C<NAME> should be the name of a conditional
declared in F<configure.ac> with C<AM_CONDITIONAL>, but it's not
C<Automake::Condition>'s responsibility to ensure this).

An empty C<@conds> means C<"TRUE">.

As explained previously, the reference (object) returned is unique
with respect to C<@conds>.  For this purpose, duplicate elements are
ignored, and C<@conds> is rewritten as C<("FALSE")> if it contains
C<"FALSE"> or two contradictory conditionals (such as C<"NAME_FALSE">
and C<"NAME_TRUE">.)

Therefore the following two statements create the same object (they
both create the C<"FALSE"> condition).

  my $c3 = new Automake::Condition "COND1_TRUE", "COND1_FALSE";
  my $c4 = new Automake::Condition "COND2_TRUE", "FALSE";
  $c3 == $c4;   # True!
  $c3 == FALSE; # True!

=cut

# Keys in this hash are conditional strings. Values are the
# associated object conditions.  This is used by `new' to reuse
# Condition objects with identical conditionals.
use vars '%_condition_singletons';
# Do NOT reset this hash here.  It's already empty by default,
# and any setting would otherwise occur AFTER the `TRUE' and `FALSE'
# constants definitions.
#   %_condition_singletons = ();

sub new ($;@)
{
  my ($class, @conds) = @_;
  my $self = {
    hash => {},
  };
  bless $self, $class;

  # Accept strings like "FOO BAR" as shorthand for ("FOO", "BAR").
  @conds = map { split (' ', $_) } @conds;

  for my $cond (@conds)
    {
      next if $cond eq 'TRUE';

      # Catch some common programming errors:
      # - A Condition passed to new
      confess "`$cond' is a reference, expected a string" if ref $cond;
      # - A Condition passed as a string to new
      confess "`$cond' does not look like a condition" if $cond =~ /::/;

      # Detect cases when @conds can be simplified to FALSE.
      if (($cond eq 'FALSE' && $#conds > 0)
	  || ($cond =~ /^(.*)_TRUE$/ && exists $self->{'hash'}{"${1}_FALSE"})
	  || ($cond =~ /^(.*)_FALSE$/ && exists $self->{'hash'}{"${1}_TRUE"}))
	{
	  return &FALSE;
	}

      $self->{'hash'}{$cond} = 1;
    }

  my $key = $self->string;
  if (exists $_condition_singletons{$key})
    {
      return $_condition_singletons{$key};
    }
  $_condition_singletons{$key} = $self;
  return $self;
}

=item C<$newcond = $cond-E<gt>merge (@otherconds)>

Return a new condition which is the conjunction of
C<$cond> and C<@otherconds>.

=cut

sub merge ($@)
{
  my ($self, @otherconds) = @_;
  new Automake::Condition (map { $_->conds } ($self, @otherconds));
}

=item C<$newcond = $cond-E<gt>merge_conds (@conds)>

Return a new condition which is the conjunction of C<$cond> and
C<@conds>, where C<@conds> is a list of conditional strings, as
passed to C<new>.

=cut

sub merge_conds ($@)
{
  my ($self, @conds) = @_;
  new Automake::Condition $self->conds, @conds;
}

=item C<$newcond = $cond-E<gt>strip ($minuscond)>

Return a new condition which has all the conditionals of C<$cond>
except those of C<$minuscond>.  This is the opposite of C<merge>.

=cut

sub strip ($$)
{
  my ($self, $minus) = @_;
  my @res = grep { not $minus->has ($_) } $self->conds;
  return new Automake::Condition @res;
}

=item C<@list = $cond-E<gt>conds>

Return the set of conditionals defining C<$cond>, as strings.  Note that
this might not be exactly the list passed to C<new> (or a
concatenation of such lists if C<merge> was used), because of the
cleanup mentioned in C<new>'s description.

For instance C<$c3-E<gt>conds> will simply return C<("FALSE")>.

=cut

sub conds ($ )
{
  my ($self) = @_;
  my @conds = keys %{$self->{'hash'}};
  return ("TRUE") unless @conds;
  return sort @conds;
}

# Undocumented, shouldn't be needed outside of this class.
sub has ($$)
{
  my ($self, $cond) = @_;
  return exists $self->{'hash'}{$cond};
}

=item C<$cond-E<gt>false>

Return 1 iff this condition is always false.

=cut

sub false ($ )
{
  my ($self) = @_;
  return $self->has ('FALSE');
}

=item C<$cond-E<gt>true>

Return 1 iff this condition is always true.

=cut

sub true ($ )
{
  my ($self) = @_;
  return 0 == keys %{$self->{'hash'}};
}

=item C<$cond-E<gt>string>

Build a string which denotes the condition.

For instance using the C<$cond> definition from L<SYNOPSYS>,
C<$cond-E<gt>string> will return C<"COND1_TRUE COND2_FALSE">.

=cut

sub string ($ )
{
  my ($self) = @_;

  return $self->{'string'} if defined $self->{'string'};

  my $res = '';
  if ($self->false)
    {
      $res = 'FALSE';
    }
  else
    {
      $res = join (' ', $self->conds);
    }
  $self->{'string'} = $res;
  return $res;
}

=item C<$cond-E<gt>human>

Build a human readable string which denotes the condition.

For instance using the C<$cond> definition from L<SYNOPSYS>,
C<$cond-E<gt>string> will return C<"COND1 and !COND2">.

=cut

sub _to_human ($ )
{
  my ($s) = @_;
  if ($s =~ /^(.*)_(TRUE|FALSE)$/)
    {
      return (($2 eq 'FALSE') ? '!' : '') . $1;
    }
  else
    {
      return $s;
    }
}

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
      $res = join (' and ', map { _to_human $_ } $self->conds);
    }
  $self->{'human'} = $res;
  return $res;
}

=item C<$cond-E<gt>subst_string>

Build a C<AC_SUBST>-style string for output in F<Makefile.in>.

For instance using the C<$cond> definition from L<SYNOPSYS>,
C<$cond-E<gt>subst_string> will return C<"@COND1_TRUE@@COND2_FALSE@">.

=cut

sub subst_string ($ )
{
  my ($self) = @_;

  return $self->{'subst_string'} if defined $self->{'subst_string'};

  my $res = '';
  if ($self->false)
    {
      $res = '#';
    }
  elsif (! $self->true)
    {
      $res = '@' . join ('@@', sort $self->conds) . '@';
    }
  $self->{'subst_string'} = $res;
  return $res;
}

=item C<$cond-E<gt>true_when ($when)>

Return 1 iff C<$cond> is true when C<$when> is true.
Return 0 otherwise.

Using the definitions from L<SYNOPSYS>, C<$cond> is true
when C<$both> is true, but the converse is wrong.

=cut

sub true_when ($$)
{
  my ($self, $when) = @_;

  # Nothing is true when FALSE (not even FALSE itself, but it
  # shouldn't hurt if you decide to change that).
  return 0 if $self->false || $when->false;

  # If we are true, we stay true when $when is true :)
  return 1 if $self->true;

  # $SELF is true under $WHEN if each conditional component of $SELF
  # exists in $WHEN.
  foreach my $cond ($self->conds)
    {
      return 0 unless $when->has ($cond);
    }
  return 1;
}

=item C<$cond-E<gt>redundant_wrt (@conds)>

Return 1 iff C<$cond> is true for any condition in C<@conds>.
If @conds is empty, return 1 iff C<$cond> is C<FALSE>.
Return 0 otherwise.

=cut

sub redundant_wrt ($@)
{
  my ($self, @conds) = @_;

  foreach my $cond (@conds)
    {
      return 1 if $self->true_when ($cond);
    }
  return $self->false;
}

=item C<$cond-E<gt>implies_any (@conds)>

Return 1 iff C<$cond> implies any of the conditions in C<@conds>.
Return 0 otherwise.

=cut

sub implies_any ($@)
{
  my ($self, @conds) = @_;

  foreach my $cond (@conds)
    {
      return 1 if $cond->true_when ($self);
    }
  return 0;
}

=item C<$cond-E<gt>not>

Return a negation of C<$cond> as a list of C<Condition>s.
This list should be used to construct a C<DisjConditions>
(we cannot return a C<DisjConditions> from C<Automake::Condition>,
because that would make these two packages interdependent).

=cut

sub not ($ )
{
  my ($self) = @_;
  return @{$self->{'not'}} if defined $self->{'not'};
  my @res =
    map { new Automake::Condition &conditional_negate ($_) } $self->conds;
  $self->{'not'} = [@res];
  return @res;
}

=item C<$cond-E<gt>multiply (@conds)>

Assumption: C<@conds> represent a disjunction of conditions.

Return the result of multiplying C<$cond> with that disjunction.
The result will be a list of conditions suitable to construct a
C<DisjConditions>.

=cut

sub multiply ($@)
{
  my ($self, @set) = @_;
  my %res = ();
  for my $cond (@set)
    {
      my $ans = $self->merge ($cond);
      $res{$ans} = $ans;
    }

  # FALSE can always be removed from a disjunction.
  delete $res{FALSE};

  # Now, $self is a common factor of the remaining conditions.
  # If one of the conditions is $self, we can discard the rest.
  return ($self, ())
    if exists $res{$self};

  return (values %res);
}

=head2 Other helper functions

=over 4

=item C<TRUE>

The C<"TRUE"> conditional.

=item C<FALSE>

The C<"FALSE"> conditional.

=cut

use constant TRUE => new Automake::Condition "TRUE";
use constant FALSE => new Automake::Condition "FALSE";

=item C<reduce_and (@conds)>

Return a subset of @conds with the property that the conjunction of
the subset is the same as the conjunction of @conds.  For example, if
both C<COND1_TRUE COND2_TRUE> and C<COND1_TRUE> are in the list,
discard the latter.  If the input list is empty, return C<(TRUE)>.

=cut

sub reduce_and (@)
{
  my (@conds) = @_;
  my @ret = ();
  my $cond;
  while (@conds > 0)
    {
      $cond = shift @conds;

      # FALSE is absorbent.
      return FALSE
	if $cond == FALSE;

      if (! $cond->redundant_wrt (@ret, @conds))
	{
	  push (@ret, $cond);
	}
    }

  return TRUE if @ret == 0;
  return @ret;
}

=item C<reduce_or (@conds)>

Return a subset of @conds with the property that the disjunction of
the subset is equivalent to the disjunction of @conds.  For example,
if both C<COND1_TRUE COND2_TRUE> and C<COND1_TRUE> are in the list,
discard the former.  If the input list is empty, return C<(FALSE)>.

=cut

sub reduce_or (@)
{
  my (@conds) = @_;
  my @ret = ();
  my $cond;
  while (@conds > 0)
    {
      $cond = shift @conds;

      next
       if $cond == FALSE;
      return TRUE
       if $cond == TRUE;

      push (@ret, $cond)
       unless $cond->implies_any (@ret, @conds);
    }

  return FALSE if @ret == 0;
  return @ret;
}

=item C<conditional_negate ($condstr)>

Negate a conditional string.

=cut

sub conditional_negate ($)
{
  my ($cond) = @_;

  $cond =~ s/TRUE$/TRUEO/;
  $cond =~ s/FALSE$/TRUE/;
  $cond =~ s/TRUEO$/FALSE/;

  return $cond;
}

=head1 SEE ALSO

L<Automake::DisjConditions>.

=head1 HISTORY

C<AM_CONDITIONAL>s and supporting code were added to Automake 1.1o by
Ian Lance Taylor <ian@cygnus.org> in 1997.  Since then it has been
improved by Tom Tromey <tromey@redhat.com>, Richard Boulton
<richard@tartarus.org>, Raja R Harinath <harinath@cs.umn.edu>,
Akim Demaille <akim@epita.fr>, and  Alexandre Duret-Lutz <adl@gnu.org>.

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
