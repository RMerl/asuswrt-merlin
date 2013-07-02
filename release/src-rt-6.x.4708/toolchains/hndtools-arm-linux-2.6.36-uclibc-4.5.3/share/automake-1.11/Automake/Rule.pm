# Copyright (C) 2003, 2004, 2006, 2007  Free Software Foundation, Inc.

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

package Automake::Rule;
use strict;
use Carp;

use Automake::Item;
use Automake::RuleDef;
use Automake::ChannelDefs;
use Automake::Channels;
use Automake::Options;
use Automake::Condition qw (TRUE FALSE);
use Automake::DisjConditions;
require Exporter;
use vars '@ISA', '@EXPORT', '@EXPORT_OK';
@ISA = qw/Automake::Item Exporter/;
@EXPORT = qw (reset register_suffix_rule suffix_rules_count
	      suffixes rules $suffix_rules $KNOWN_EXTENSIONS_PATTERN
	      depend %dependencies %actions register_action
	      accept_extensions
	      reject_rule msg_rule msg_cond_rule err_rule err_cond_rule
	      rule rrule ruledef rruledef);

=head1 NAME

Automake::Rule - support for rules definitions

=head1 SYNOPSIS

  use Automake::Rule;
  use Automake::RuleDef;


=head1 DESCRIPTION

This package provides support for Makefile rule definitions.

An C<Automake::Rule> is a rule name associated to possibly
many conditional definitions.  These definitions are instances
of C<Automake::RuleDef>.

Therefore obtaining the value of a rule under a given
condition involves two lookups.  One to look up the rule,
and one to look up the conditional definition:

  my $rule = rule $name;
  if ($rule)
    {
      my $def = $rule->def ($cond);
      if ($def)
	{
	  return $def->location;
	}
      ...
    }
  ...

when it is known that the rule and the definition
being looked up exist, the above can be simplified to

  return rule ($name)->def ($cond)->location; # do not write this.

but is better written

  return rrule ($name)->rdef ($cond)->location;

or even

  return rruledef ($name, $cond)->location;

The I<r> variants of the C<rule>, C<def>, and C<ruledef> methods add
an extra test to ensure that the lookup succeeded, and will diagnose
failures as internal errors (with a message which is much more
informative than Perl's warning about calling a method on a
non-object).

=head2 Global variables

=over 4

=cut

my $_SUFFIX_RULE_PATTERN =
  '^(\.[a-zA-Z0-9_(){}$+@\-]+)(\.[a-zA-Z0-9_(){}$+@\-]+)' . "\$";

# Suffixes found during a run.
use vars '@_suffixes';

# Same as $suffix_rules (declared below), but records only the
# default rules supplied by the languages Automake supports.
use vars '$_suffix_rules_default';

=item C<%dependencies>

Holds the dependencies of targets which dependencies are factored.
Typically, C<.PHONY> will appear in plenty of F<*.am> files, but must
be output once.  Arguably all pure dependencies could be subject to
this factoring, but it is not unpleasant to have paragraphs in
Makefile: keeping related stuff altogether.

=cut

use vars '%dependencies';

=item <%actions>

Holds the factored actions.  Tied to C<%dependencies>, i.e., filled
only when keys exists in C<%dependencies>.

=cut

use vars '%actions';

=item <$suffix_rules>

This maps the source extension for all suffix rule seen to
a C<hash> whose keys are the possible output extensions.

Note that this is transitively closed by construction:
if we have
      exists $suffix_rules{$ext1}{$ext2}
   && exists $suffix_rules{$ext2}{$ext3}
then we also have
      exists $suffix_rules{$ext1}{$ext3}

So it's easy to check whether C<.foo> can be transformed to
C<.$(OBJEXT)> by checking whether
C<$suffix_rules{'.foo'}{'.$(OBJEXT)'}> exists.  This will work even if
transforming C<.foo> to C<.$(OBJEXT)> involves a chain of several
suffix rules.

The value of C<$suffix_rules{$ext1}{$ext2}> is a pair
C<[ $next_sfx, $dist ]> where C<$next_sfx> is target suffix
for the next rule to use to reach C<$ext2>, and C<$dist> the
distance to C<$ext2'>.

The content of this variable should be updated via the
C<register_suffix_rule> function.

=cut

use vars '$suffix_rules';

=item C<$KNOWN_EXTENSIONS_PATTERN>

Pattern that matches all know input extensions (i.e. extensions used
by the languages supported by Automake).  Using this pattern (instead
of `\..*$') to match extensions allows Automake to support dot-less
extensions.

New extensions should be registered with C<accept_extensions>.

=cut

use vars qw ($KNOWN_EXTENSIONS_PATTERN @_known_extensions_list);
$KNOWN_EXTENSIONS_PATTERN = "";
@_known_extensions_list = ();

=back

=head2 Error reporting functions

In these functions, C<$rule> can be either a rule name, or
an instance of C<Automake::Rule>.

=over 4

=item C<err_rule ($rule, $message, [%options])>

Uncategorized errors about rules.

=cut

sub err_rule ($$;%)
{
  msg_rule ('error', @_);
}

=item C<err_cond_rule ($cond, $rule, $message, [%options])>

Uncategorized errors about conditional rules.

=cut

sub err_cond_rule ($$$;%)
{
  msg_cond_rule ('error', @_);
}

=item C<msg_cond_rule ($channel, $cond, $rule, $message, [%options])>

Messages about conditional rules.

=cut

sub msg_cond_rule ($$$$;%)
{
  my ($channel, $cond, $rule, $msg, %opts) = @_;
  my $r = ref ($rule) ? $rule : rrule ($rule);
  msg $channel, $r->rdef ($cond)->location, $msg, %opts;
}

=item C<msg_rule ($channel, $targetname, $message, [%options])>

Messages about rules.

=cut

sub msg_rule ($$$;%)
{
  my ($channel, $rule, $msg, %opts) = @_;
  my $r = ref ($rule) ? $rule : rrule ($rule);
  # Don't know which condition is concerned.  Pick any.
  my $cond = $r->conditions->one_cond;
  msg_cond_rule ($channel, $cond, $r, $msg, %opts);
}


=item C<$bool = reject_rule ($rule, $error_msg)>

Bail out with C<$error_msg> if a rule with name C<$rule> has been
defined.

Return true iff C<$rule> is defined.

=cut

sub reject_rule ($$)
{
  my ($rule, $msg) = @_;
  if (rule ($rule))
    {
      err_rule $rule, $msg;
      return 1;
    }
  return 0;
}

=back

=head2 Administrative functions

=over 4

=item C<accept_extensions (@exts)>

Update C<$KNOWN_EXTENSIONS_PATTERN> to recognize the extensions
listed C<@exts>.  Extensions should contain a dot if needed.

=cut

sub accept_extensions (@)
{
    push @_known_extensions_list, @_;
    $KNOWN_EXTENSIONS_PATTERN =
	'(?:' . join ('|', map (quotemeta, @_known_extensions_list)) . ')';
}

=item C<rules>

Return the list of all L<Automake::Rule> instances.  (I.e., all
rules defined so far.)

=cut

use vars '%_rule_dict';
sub rules ()
{
  return values %_rule_dict;
}


=item C<register_action($target, $action)>

Append the C<$action> to C<$actions{$target}> taking care of special
cases.

=cut

sub register_action ($$)
{
  my ($target, $action) = @_;
  if ($actions{$target})
    {
      $actions{$target} .= "\n$action" if $action;
    }
  else
    {
      $actions{$target} = $action;
    }
}


=item C<Automake::Rule::reset>

The I<forget all> function.  Clears all know rules and reset some
other internal data.

=cut

sub reset()
{
  %_rule_dict = ();
  @_suffixes = ();
  # The first time we initialize the variables,
  # we save the value of $suffix_rules.
  if (defined $_suffix_rules_default)
    {
      $suffix_rules = $_suffix_rules_default;
    }
  else
    {
      $_suffix_rules_default = $suffix_rules;
    }

  %dependencies =
    (
     # Texinfoing.
     'dvi'      => [],
     'dvi-am'   => [],
     'pdf'      => [],
     'pdf-am'   => [],
     'ps'       => [],
     'ps-am'    => [],
     'info'     => [],
     'info-am'  => [],
     'html'     => [],
     'html-am'  => [],

     # Installing/uninstalling.
     'install-data-am'      => [],
     'install-exec-am'      => [],
     'uninstall-am'         => [],

     'install-man'	    => [],
     'uninstall-man'	    => [],

     'install-dvi'          => [],
     'install-dvi-am'       => [],
     'install-html'         => [],
     'install-html-am'      => [],
     'install-info'         => [],
     'install-info-am'      => [],
     'install-pdf'          => [],
     'install-pdf-am'       => [],
     'install-ps'           => [],
     'install-ps-am'        => [],

     'installcheck-am'      => [],

     # Cleaning.
     'clean-am'             => [],
     'mostlyclean-am'       => [],
     'maintainer-clean-am'  => [],
     'distclean-am'         => [],
     'clean'                => [],
     'mostlyclean'          => [],
     'maintainer-clean'     => [],
     'distclean'            => [],

     # Tarballing.
     'dist-all'             => [],

     # Phoning.
     '.PHONY'               => [],
     # Recursive install targets (so `make -n install' works for BSD Make).
     '.MAKE'		    => [],
     );
  %actions = ();
}

=item C<register_suffix_rule ($where, $src, $dest)>

Register a suffix rules defined on C<$where> that transform
files ending in C<$src> into files ending in C<$dest>.

This upgrades the C<$suffix_rules> variables.

=cut

sub register_suffix_rule ($$$)
{
  my ($where, $src, $dest) = @_;

  verb "Sources ending in $src become $dest";
  push @_suffixes, $src, $dest;

  # When transforming sources to objects, Automake uses the
  # %suffix_rules to move from each source extension to
  # `.$(OBJEXT)', not to `.o' or `.obj'.  However some people
  # define suffix rules for `.o' or `.obj', so internally we will
  # consider these extensions equivalent to `.$(OBJEXT)'.  We
  # CANNOT rewrite the target (i.e., automagically replace `.o'
  # and `.obj' by `.$(OBJEXT)' in the output), or warn the user
  # that (s)he'd better use `.$(OBJEXT)', because Automake itself
  # output suffix rules for `.o' or `.obj'...
  $dest = '.$(OBJEXT)' if ($dest eq '.o' || $dest eq '.obj');

  # Reading the comments near the declaration of $suffix_rules might
  # help to understand the update of $suffix_rules that follows...

  # Register $dest as a possible destination from $src.
  # We might have the create the \hash.
  if (exists $suffix_rules->{$src})
    {
      $suffix_rules->{$src}{$dest} = [ $dest, 1 ];
    }
  else
    {
      $suffix_rules->{$src} = { $dest => [ $dest, 1 ] };
    }

  # If we know how to transform $dest in something else, then
  # we know how to transform $src in that "something else".
  if (exists $suffix_rules->{$dest})
    {
      for my $dest2 (keys %{$suffix_rules->{$dest}})
	{
	  my $dist = $suffix_rules->{$dest}{$dest2}[1] + 1;
	  # Overwrite an existing $src->$dest2 path only if
	  # the path via $dest which is shorter.
	  if (! exists $suffix_rules->{$src}{$dest2}
	      || $suffix_rules->{$src}{$dest2}[1] > $dist)
	    {
	      $suffix_rules->{$src}{$dest2} = [ $dest, $dist ];
	    }
	}
    }

  # Similarly, any extension that can be derived into $src
  # can be derived into the same extensions as $src can.
  my @dest2 = keys %{$suffix_rules->{$src}};
  for my $src2 (keys %$suffix_rules)
    {
      if (exists $suffix_rules->{$src2}{$src})
	{
	  for my $dest2 (@dest2)
	    {
	      my $dist = $suffix_rules->{$src}{$dest2} + 1;
	      # Overwrite an existing $src2->$dest2 path only if
	      # the path via $src is shorter.
	      if (! exists $suffix_rules->{$src2}{$dest2}
		  || $suffix_rules->{$src2}{$dest2}[1] > $dist)
		{
		  $suffix_rules->{$src2}{$dest2} = [ $src, $dist ];
		}
	    }
	}
    }
}

=item C<$count = suffix_rules_count>

Return the number of suffix rules added while processing the current
F<Makefile> (excluding predefined suffix rules).

=cut

sub suffix_rules_count ()
{
  return (scalar keys %$suffix_rules) - (scalar keys %$_suffix_rules_default);
}

=item C<@list = suffixes>

Return the list of known suffixes.

=cut

sub suffixes ()
{
  return @_suffixes;
}

=item C<rule ($rulename)>

Return the C<Automake::Rule> object for the rule
named C<$rulename> if defined.   Return 0 otherwise.

=cut

sub rule ($)
{
  my ($name) = @_;
  # Strip $(EXEEXT) from $name, so we can diagnose
  # a clash if `ctags$(EXEEXT):' is redefined after `ctags:'.
  $name =~ s,\$\(EXEEXT\)$,,;
  return $_rule_dict{$name} || 0;
}

=item C<ruledef ($rulename, $cond)>

Return the C<Automake::RuleDef> object for the rule named
C<$rulename> if defined in condition C<$cond>.  Return false
if the condition or the rule does not exist.

=cut

sub ruledef ($$)
{
  my ($name, $cond) = @_;
  my $rule = rule $name;
  return $rule && $rule->def ($cond);
}

=item C<rrule ($rulename)

Return the C<Automake::Rule> object for the variable named
C<$rulename>.  Abort with an internal error if the variable was not
defined.

The I<r> in front of C<var> stands for I<required>.  One
should call C<rvar> to assert the rule's existence.

=cut

sub rrule ($)
{
  my ($name) = @_;
  my $r = rule $name;
  prog_error ("undefined rule $name\n" . &rules_dump)
    unless $r;
  return $r;
}

=item C<rruledef ($varname, $cond)>

Return the C<Automake::RuleDef> object for the rule named
C<$rulename> if defined in condition C<$cond>.  Abort with an internal
error if the condition or the rule does not exist.

=cut

sub rruledef ($$)
{
  my ($name, $cond) = @_;
  return rrule ($name)->rdef ($cond);
}

# Create the variable if it does not exist.
# This is used only by other functions in this package.
sub _crule ($)
{
  my ($name) = @_;
  my $r = rule $name;
  return $r if $r;
  return _new Automake::Rule $name;
}

sub _new ($$)
{
  my ($class, $name) = @_;

  # Strip $(EXEEXT) from $name, so we can diagnose
  # a clash if `ctags$(EXEEXT):' is redefined after `ctags:'.
  (my $keyname = $name) =~ s,\$\(EXEEXT\)$,,;

  my $self = Automake::Item::new ($class, $name);
  $_rule_dict{$keyname} = $self;
  return $self;
}


=item C<@conds = define ($rulename, $source, $owner, $cond, $where)>

Define a new rule.  C<$rulename> is the list of targets.  C<$source>
is the filename the rule comes from.  C<$owner> is the owner of the
rule (C<RULE_AUTOMAKE> or C<RULE_USER>).  C<$cond> is the
C<Automake::Condition> under which the rule is defined.  C<$where> is
the C<Automake::Location> where the rule is defined.

Returns a (possibly empty) list of C<Automake::Condition>s where the
rule's definition should be output.

=cut

sub define ($$$$$)
{
  my ($target, $source, $owner, $cond, $where) = @_;

  prog_error "$where is not a reference"
    unless ref $where;
  prog_error "$cond is not a reference"
    unless ref $cond;

  # Don't even think about defining a rule in condition FALSE.
  return () if $cond == FALSE;

  # For now `foo:' will override `foo$(EXEEXT):'.  This is temporary,
  # though, so we emit a warning.
  (my $noexe = $target) =~ s,\$\(EXEEXT\)$,,;
  my $noexerule = rule $noexe;
  my $tdef = $noexerule ? $noexerule->def ($cond) : undef;

  if ($noexe ne $target
      && $tdef
      && $noexerule->name ne $target)
    {
      # The no-exeext option enables this feature.
      if (! option 'no-exeext')
	{
	  msg ('obsolete', $tdef->location,
	       "deprecated feature: target `$noexe' overrides "
	       . "`$noexe\$(EXEEXT)'\n"
	       . "change your target to read `$noexe\$(EXEEXT)'");
	  msg ('obsolete', $where, "target `$target' was defined here");
	}
      # Don't `return ()' now, as this might hide target clashes
      # detected below.
    }


  # A GNU make-style pattern rule has a single "%" in the target name.
  msg ('portability', $where,
       "`%'-style pattern rules are a GNU make extension")
    if $target =~ /^[^%]*%[^%]*$/;

  # Diagnose target redefinitions.
  if ($tdef)
    {
      my $oldowner  = $tdef->owner;
      # Ok, it's the name target, but the name maybe different because
      # `foo$(EXEEXT)' and `foo' have the same key in our table.
      my $oldname = $tdef->name;

      # Don't mention true conditions in diagnostics.
      my $condmsg =
	$cond == TRUE ? '' : " in condition `" . $cond->human . "'";

      if ($owner == RULE_USER)
	{
	  if ($oldowner == RULE_USER)
	    {
	      # Ignore `%'-style pattern rules.  We'd need the
	      # dependencies to detect duplicates, and they are
	      # already diagnosed as unportable by -Wportability.
	      if ($target !~ /^[^%]*%[^%]*$/)
		{
		  ## FIXME: Presently we can't diagnose duplicate user rules
		  ## because we don't distinguish rules with commands
		  ## from rules that only add dependencies.  E.g.,
		  ##   .PHONY: foo
		  ##   .PHONY: bar
		  ## is legitimate. (This is phony.test.)

		  # msg ('syntax', $where,
		  #      "redefinition of `$target'$condmsg...", partial => 1);
		  # msg_cond_rule ('syntax', $cond, $target,
		  #		   "... `$target' previously defined here");
		}
	      # Return so we don't redefine the rule in our tables,
	      # don't check for ambiguous condition, etc.  The rule
	      # will be output anyway because &read_am_file ignore the
	      # return code.
	      return ();
	    }
	  else
	    {
	      # Since we parse the user Makefile.am before reading
	      # the Automake fragments, this condition should never happen.
	      prog_error ("user target `$target'$condmsg seen after Automake's"
			  . " definition\nfrom " . $tdef->source);
	    }
	}
      else # $owner == RULE_AUTOMAKE
	{
	  if ($oldowner == RULE_USER)
	    {
	      # -am targets listed in %dependencies support a -local
	      # variant.  If the user tries to override TARGET or
	      # TARGET-am for which there exists a -local variant,
	      # just tell the user to use it.
	      my $hint = 0;
	      my $noam = $target;
	      $noam =~ s/-am$//;
	      if (exists $dependencies{"$noam-am"})
		{
		  $hint = "consider using $noam-local instead of $target";
		}

	      msg_cond_rule ('override', $cond, $target,
			     "user target `$target' defined here"
			     . "$condmsg...", partial => 1);
	      msg ('override', $where,
		   "... overrides Automake target `$oldname' defined here",
		   partial => $hint);
	      msg_cond_rule ('override', $cond, $target, $hint)
		if $hint;

	      # Don't overwrite the user definition of TARGET.
	      return ();
	    }
	  else # $oldowner == RULE_AUTOMAKE
	    {
	      # Automake should ignore redefinitions of its own
	      # rules if they came from the same file.  This makes
	      # it easier to process a Makefile fragment several times.
	      # However it's an error if the target is defined in many
	      # files.  E.g., the user might be using bin_PROGRAMS = ctags
	      # which clashes with our `ctags' rule.
	      # (It would be more accurate if we had a way to compare
	      # the *content* of both rules.  Then $targets_source would
	      # be useless.)
	      my $oldsource = $tdef->source;
	      return () if $source eq $oldsource && $target eq $oldname;

	      msg ('syntax', $where, "redefinition of `$target'$condmsg...",
		   partial => 1);
	      msg_cond_rule ('syntax', $cond, $target,
			     "... `$oldname' previously defined here");
	      return ();
	    }
	}
      # Never reached.
      prog_error ("Unreachable place reached.");
    }

  # Conditions for which the rule should be defined.
  my @conds = $cond;

  # Check ambiguous conditional definitions.
  my $rule = _crule $target;
  my ($message, $ambig_cond) = $rule->conditions->ambiguous_p ($target, $cond);
  if ($message)			# We have an ambiguity.
    {
      if ($owner == RULE_USER)
	{
	  # For user rules, just diagnose the ambiguity.
	  msg 'syntax', $where, "$message ...", partial => 1;
	  msg_cond_rule ('syntax', $ambig_cond, $target,
			 "... `$target' previously defined here");
	  return ();
	}
      else
	{
	  # FIXME: for Automake rules, we can't diagnose ambiguities yet.
	  # The point is that Automake doesn't propagate conditions
	  # everywhere.  For instance &handle_PROGRAMS doesn't care if
	  # bin_PROGRAMS was defined conditionally or not.
	  # On the following input
	  #   if COND1
	  #   foo:
	  #           ...
	  #   else
	  #   bin_PROGRAMS = foo
	  #   endif
	  # &handle_PROGRAMS will attempt to define a `foo:' rule
	  # in condition TRUE (which conflicts with COND1).  Fixing
	  # this in &handle_PROGRAMS and siblings seems hard: you'd
	  # have to explain &file_contents what to do with a
	  # condition.  So for now we do our best *here*.  If `foo:'
	  # was already defined in condition COND1 and we want to define
	  # it in condition TRUE, then define it only in condition !COND1.
	  # (See cond14.test and cond15.test for some test cases.)
	  @conds = $rule->not_always_defined_in_cond ($cond)->conds;

	  # No conditions left to define the rule.
	  # Warn, because our workaround is meaningless in this case.
	  if (scalar @conds == 0)
	    {
	      msg 'syntax', $where, "$message ...", partial => 1;
	      msg_cond_rule ('syntax', $ambig_cond, $target,
			     "... `$target' previously defined here");
	      return ();
	    }
	}
    }

  # Finally define this rule.
  for my $c (@conds)
    {
      my $def = new Automake::RuleDef ($target, '', $where->clone,
				       $owner, $source);
      $rule->set ($c, $def);
    }

  # We honor inference rules with multiple targets because many
  # make support this and people use it.  However this is disallowed
  # by POSIX.  We'll print a warning later.
  my $target_count = 0;
  my $inference_rule_count = 0;

  for my $t (split (' ', $target))
    {
      ++$target_count;
      # Check if the rule is a suffix rule: either it's a rule for
      # two known extensions...
      if ($t =~ /^($KNOWN_EXTENSIONS_PATTERN)($KNOWN_EXTENSIONS_PATTERN)$/
	  # ...or it's a rule with unknown extensions (i.e., the rule
	  # looks like `.foo.bar:' but `.foo' or `.bar' are not
	  # declared in SUFFIXES and are not known language
	  # extensions).  Automake will complete SUFFIXES from
	  # @suffixes automatically (see handle_footer).


	  || ($t =~ /$_SUFFIX_RULE_PATTERN/o && accept_extensions($1)))
	{
	  ++$inference_rule_count;
	  register_suffix_rule ($where, $1, $2);
	}
    }

  # POSIX allows multiple targets before the colon, but disallows
  # definitions of multiple inference rules.  It's also
  # disallowed to mix plain targets with inference rules.
  msg ('portability', $where,
       "Inference rules can have only one target before the colon (POSIX).")
    if $inference_rule_count > 0 && $target_count > 1;

  return @conds;
}

=item C<depend ($target, @deps)>

Adds C<@deps> to the dependencies of target C<$target>.  This should
be used only with factored targets (those appearing in
C<%dependees>).

=cut

sub depend ($@)
{
  my ($category, @dependees) = @_;
  push (@{$dependencies{$category}}, @dependees);
}

=back

=head1 SEE ALSO

L<Automake::RuleDef>, L<Automake::Condition>,
L<Automake::DisjConditions>, L<Automake::Location>.

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
