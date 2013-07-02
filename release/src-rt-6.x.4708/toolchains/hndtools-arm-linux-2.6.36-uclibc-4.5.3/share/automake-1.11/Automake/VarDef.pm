# Copyright (C) 2003, 2004, 2006  Free Software Foundation, Inc.

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

package Automake::VarDef;
use strict;
use Carp;
use Automake::ChannelDefs;
use Automake::ItemDef;

require Exporter;
use vars '@ISA', '@EXPORT';
@ISA = qw/Automake::ItemDef Exporter/;
@EXPORT = qw (&VAR_AUTOMAKE &VAR_CONFIGURE &VAR_MAKEFILE
	      &VAR_ASIS &VAR_PRETTY &VAR_SILENT &VAR_SORTED);

=head1 NAME

Automake::VarDef - a class for variable definitions

=head1 SYNOPSIS

  use Automake::VarDef;
  use Automake::Location;

  # Create a VarDef for a definition such as
  # | # any comment
  # | foo = bar # more comment
  # in Makefile.am
  my $loc = new Automake::Location 'Makefile.am:2';
  my $def = new Automake::VarDef ('foo', 'bar # more comment',
                                  '# any comment',
                                  $loc, '', VAR_MAKEFILE, VAR_ASIS);

  # Appending to a definition.
  $def->append ('value to append', 'comment to append');

  # Accessors.
  my $value    = $def->value;  # with trailing `#' comments and
                               # continuation ("\\\n") omitted.
  my $value    = $def->raw_value; # the real value, as passed to new().
  my $comment  = $def->comment;
  my $location = $def->location;
  my $type     = $def->type;
  my $owner    = $def->owner;
  my $pretty   = $def->pretty;

  # Changing owner.
  $def->set_owner (VAR_CONFIGURE,
                   new Automake::Location 'configure.ac:15');

  # Marking examined definitions.
  $def->set_seen;
  my $seen_p = $def->seen;

  # Printing a variable for debugging.
  print STDERR $def->dump;

=head1 DESCRIPTION

This class gathers data related to one Makefile-variable definition.

=head2 Constants

=over 4

=item C<VAR_AUTOMAKE>, C<VAR_CONFIGURE>, C<VAR_MAKEFILE>

Possible owners for variables.  A variable can be defined
by Automake, in F<configure.ac> (using C<AC_SUBST>), or in
the user's F<Makefile.am>.

=cut

# Defined so that the owner of a variable can only be increased (e.g
# Automake should not override a configure or Makefile variable).
use constant VAR_AUTOMAKE => 0; # Variable defined by Automake.
use constant VAR_CONFIGURE => 1;# Variable defined in configure.ac.
use constant VAR_MAKEFILE => 2; # Variable defined in Makefile.am.

=item C<VAR_ASIS>, C<VAR_PRETTY>, C<VAR_SILENT>, C<VAR_SORTED>

Possible print styles.  C<VAR_ASIS> variables should be output as-is.
C<VAR_PRETTY> variables are wrapped on multiple lines if they cannot
fit on one.  C<VAR_SILENT> variables are not output at all.  Finally,
C<VAR_SORTED> variables should be sorted and then handled as
C<VAR_PRETTY> variables.

C<VAR_SILENT> variables can also be overridden silently (unlike the
other kinds of variables whose overriding may sometimes produce
warnings).

=cut

# Possible values for pretty.
use constant VAR_ASIS => 0;	# Output as-is.
use constant VAR_PRETTY => 1;	# Pretty printed on output.
use constant VAR_SILENT => 2;	# Not output.  (Can also be
				# overridden silently.)
use constant VAR_SORTED => 3;	# Sorted and pretty-printed.

=back

=head2 Methods

C<VarDef> defines the following methods in addition to those inherited
from L<Automake::ItemDef>.

=over 4

=item C<my $def = new Automake::VarDef ($varname, $value, $comment, $location, $type, $owner, $pretty)>

Create a new Makefile-variable definition.  C<$varname> is the name of
the variable being defined and C<$value> its value.

C<$comment> is any comment preceding the definition.  (Because
Automake reorders variable definitions in the output, it also tries to
carry comments around.)

C<$location> is the place where the definition occurred, it should be
an instance of L<Automake::Location>.

C<$type> should be C<''> for definitions made with C<=>, and C<':'>
for those made with C<:=>.

C<$owner> specifies who owns the variables, it can be one of
C<VAR_AUTOMAKE>, C<VAR_CONFIGURE>, or C<VAR_MAKEFILE> (see these
definitions).

Finally, C<$pretty> tells how the variable should be output, and can
be one of C<VAR_ASIS>, C<VAR_PRETTY>, or C<VAR_SILENT>, or
C<VAR_SORTED> (see these definitions).

=cut

sub new ($$$$$$$$)
{
  my ($class, $var, $value, $comment, $location, $type, $owner, $pretty) = @_;

  # A user variable must be set by either `=' or `:=', and later
  # promoted to `+='.
  if ($owner != VAR_AUTOMAKE && $type eq '+')
    {
      error $location, "$var must be set with `=' before using `+='";
    }

  my $self = Automake::ItemDef::new ($class, $comment, $location, $owner);
  $self->{'value'} = $value;
  $self->{'type'} = $type;
  $self->{'pretty'} = $pretty;
  $self->{'seen'} = 0;
  return $self;
}

=item C<$def-E<gt>append ($value, $comment)>

Append C<$value> and <$comment> to the existing value and comment of
C<$def>.  This is normally called on C<+=> definitions.

=cut

sub append ($$$)
{
  my ($self, $value, $comment) = @_;
  $self->{'comment'} .= $comment;

  my $val = $self->{'value'};

  # Strip comments from augmented variables.  This is so that
  #   VAR = foo # com
  #   VAR += bar
  # does not become
  #   VAR = foo # com bar
  # Furthermore keeping `#' would not be portable if the variable is
  # output on multiple lines.
  $val =~ s/ ?#.*//;

  if (chomp $val)
    {
      # Insert a backslash before a trailing newline.
      $val .= "\\\n";
    }
  elsif ($val)
    {
      # Insert a separator.
      $val .= ' ';
    }
  $self->{'value'} = $val . $value;
  # Turn ASIS appended variables into PRETTY variables.  This is to
  # cope with `make' implementation that cannot read very long lines.
  $self->{'pretty'} = VAR_PRETTY if $self->{'pretty'} == VAR_ASIS;
}

=item C<$def-E<gt>value>

=item C<$def-E<gt>type>

=item C<$def-E<gt>pretty>

Accessors to the various constituents of a C<VarDef>.  See the
documentation of C<new>'s arguments for a description of these.

=cut

sub value ($)
{
  my ($self) = @_;
  my $val = $self->raw_value;
  # Strip anything past `#'.  `#' characters cannot be escaped
  # in Makefiles, so we don't have to be smart.
  $val =~ s/#.*$//s;
  # Strip backslashes.
  $val =~ s/\\$/ /mg;
  return $val;
}

sub raw_value ($)
{
  my ($self) = @_;
  return $self->{'value'};
}

sub type ($)
{
  my ($self) = @_;
  return $self->{'type'};
}

sub pretty ($)
{
  my ($self) = @_;
  return $self->{'pretty'};
}

=item C<$def-E<gt>set_owner ($owner, $location)>

Change the owner of a definition.  This usually happens because
the user used C<+=> on an Automake variable, so (s)he now owns
the content.  C<$location> should be an instance of L<Automake::Location>
indicating where the change took place.

=cut

sub set_owner ($$$)
{
  my ($self, $owner, $location) = @_;
  # We always adjust the location when the owner changes (even for
  # `+=' statements).  The risk otherwise is to warn about
  # a VAR_MAKEFILE variable and locate it in configure.ac...
  $self->{'owner'} = $owner;
  $self->{'location'} = $location;
}

=item C<$def-E<gt>set_seen>

=item C<$bool = $def-E<gt>seen>

These function allows Automake to mark (C<set_seen>) variable that
it has examined in some way, and latter check (using C<seen>) for
unused variables.  Unused variables usually indicate typos.

=cut

sub set_seen ($)
{
  my ($self) = @_;
  $self->{'seen'} = 1;
}

sub seen ($)
{
  my ($self) = @_;
  return $self->{'seen'};
}

=item C<$str = $def-E<gt>dump>

Format the contents of C<$def> as a human-readable string,
for debugging.

=cut

sub dump ($)
{
  my ($self) = @_;
  my $owner = $self->owner;

  if ($owner == VAR_AUTOMAKE)
    {
      $owner = 'Automake';
    }
  elsif ($owner == VAR_CONFIGURE)
    {
      $owner = 'Configure';
    }
  elsif ($owner == VAR_MAKEFILE)
    {
      $owner = 'Makefile';
    }
  else
    {
      prog_error ("unexpected owner");
    }

  my $where = $self->location->dump;
  my $comment = $self->comment;
  my $value = $self->raw_value;
  my $type = $self->type;

  return "{
      type: $type=
      where: $where      comment: $comment
      value: $value
      owner: $owner
    }\n";
}

=back

=head1 SEE ALSO

L<Automake::Variable>, L<Automake::ItemDef>.

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
