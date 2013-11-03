=head1 NAME

docbook2man-spec - convert DocBook RefEntries to Unix manpages

=head1 SYNOPSIS

The SGMLSpm package from CPAN.  This contains the sgmlspl script which
is used to grok this file.  Use it like this:

nsgmls some-docbook-document.sgml | sgmlspl docbook2man-spec.pl

=head1 DESCRIPTION

This is a sgmlspl spec file that produces Unix-style
manpages from RefEntry markup.

See the accompanying RefEntry man page for 'plain new' documentation. :)

=head1 LIMITATIONS

Trying docbook2man on non-DocBook or non-conformant SGML results in
undefined behavior. :-)

This program is a slow, dodgy Perl script.

This program does not come close to supporting all the possible markup
in DocBook, and will produce wrong output in some cases with supported
markup.

=head1 TODO

Add new element handling and fix existing handling.  Be robust.
Produce cleanest, readable man output as possible (unlike some
other converters).  Follow Linux man(7) convention.
If this results in added logic in this script,
that's okay.  The code should still be reasonably organized.

Make it faster.  If Perl sucks port it to another language.

=head1 COPYRIGHT

Copyright (C) 1998-1999 Steve Cheng <steve@ggi-project.org>

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

You should have received a copy of the GNU General Public License along with
this program; see the file COPYING.  If not, please write to the Free
Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

=cut

# $Id: docbook2man-spec.pl,v 1.1 2000/07/21 20:22:30 rosalia Exp $

use SGMLS;			# Use the SGMLS package.
use SGMLS::Output;		# Use stack-based output.
use SGMLS::Refs;

########################################################################
# SGMLSPL script produced automatically by the script sgmlspl.pl
#
# Document Type: any, but processes only RefEntries
# Edited by: me :)
########################################################################

$write_manpages = 0;
$blank_xrefs = 0;

sgml('start', sub { 
	push_output('nul');
	$raw_cdata = 1;			# Makes it a bit faster.
	
	# Links file
	open(LINKSFILE, ">manpage.links");

	$Refs = new SGMLS::Refs("manpage.refs");
});
sgml('end', sub {
	close(LINKSFILE);
	if($blank_xrefs) {
		print STDERR "Warning: output contains unresolved XRefs\n";
	}
});




########################################################################
#
# Output helpers 
#
########################################################################

# Our own version of sgml() and output() to allow simple string output
# to play well with roff's stupid whitespace rules. 

sub man_sgml
{
	if(ref($_[1]) eq 'CODE') {
		return &sgml;
	}
	
	my $s = $_[1];

	$s =~ s/\\/\\\\/g;
	$s =~ s/'/\\'/g;

	# \n at the beginning means start at beginning of line
	if($s =~ s/^\n//) {
		$sub = 'sub { output "\n" unless $newline_last++; ';
		if($s eq '') { 
			sgml($_[0], eval('sub { output "\n" unless $newline_last++; }'));
		} elsif($s =~ /\n$/) {
			sgml($_[0], eval("sub { output \"\\n\" unless \$newline_last++; output '$s'; }"));
		} else {
			sgml($_[0], eval("sub { output \"\\n\" unless \$newline_last; output '$s'; \$newline_last = 0; }"));
		}
	} else {
		if($s =~ /\n$/) {
			sgml($_[0], eval("sub { output '$s'; \$newline_last = 1; }"));
		} else {
			sgml($_[0], eval("sub { output '$s'; \$newline_last = 0; }"));
		}
	}
}

sub man_output
{
	$_ = shift;
	if(s/^\n//) {
		output "\n" unless $newline_last++;
	}
	return if $_ eq '';
	
	output $_;

	if(@_) {
		output @_;
		$newline_last = (pop(@_) =~ /\n$/);
	} else {
		$newline_last = ($_ =~ /\n$/)
	}
}

# Fold lines into one, quote some characters
sub fold_string
{
	$_ = shift;
	
	s/\\/\\\\/g;
	s/"/\\\&"/g;

	# Change tabs to spaces
	tr/\t\n/  /;

	# Trim whitespace from beginning and end.
	s/^ +//;
	s/ +$//;

	return $_;
}
	
sub save_cdata()
{
	$raw_cdata++;
	push_output('string');
}

sub bold_on()
{
	# If the last font is also bold, don't change anything.
	# Basically this is to just get more readable man output.
	if($fontstack[$#fontstack] ne 'bold') {
		if(!$raw_cdata) {
			output '\fB';
			$newline_last = 0;
		}
	}
	push(@fontstack, 'bold');
}

sub italic_on()
{
	# If the last font is also italic, don't change anything.
	if($fontstack[$#fontstack] ne 'italic') {
		if(!$raw_cdata) {
			output '\fI';
			$newline_last = 0;
		}
	}
	push(@fontstack, 'italic');
}

sub font_off()
{
	my $thisfont = pop(@fontstack);
	my $lastfont = $fontstack[$#fontstack];
	
	# Only output font change if it is different
	if($thisfont ne $lastfont) {
		if($raw_cdata)			{ return; }
		elsif($lastfont eq 'bold') 	{ output '\fB'; }
		elsif($lastfont eq 'italic')	{ output '\fI'; }
		else				{ output '\fR'; }
	
		$newline_last = 0;
	}
}






########################################################################
#
# Manpage management
#
########################################################################

sgml('<REFENTRY>', sub { 
	# This will be overwritten at end of REFMETA, when we know the name of the page.
	pop_output();
	
	$write_manpages = 1;		# Currently writing manpage.
	
	$nocollapse_whitespace = 0;	# Current whitespace collapse counter.
	$newline_last = 1;		# At beginning of line?
		# Just a bit of warning, you will see this variable manipulated
		# manually a lot.  It makes the code harder to follow but it
		# saves you from having to worry about collapsing at the end of
		# parse, stopping at verbatims, etc.
	$raw_cdata = 0;                 # Instructs certain output functions to
					# leave CDATA alone, so we can assign
					# it to a string and process it, etc.
	@fontstack = ();		# Fonts being activated.
	
	$manpage_title = '';		# Needed for indexing.
	$manpage_sect = '';
	@manpage_names = ();
	
	$manpage_misc = '';
	
	$list_nestlevel = 0;		# Indent certain nested content.
});
sgml('</REFENTRY>', sub {
	if(!$newline_last) {
		output "\n";
	}
	
	$write_manpages = 0;
	$raw_cdata = 1;
	push_output('nul');
});

sgml('</REFMETA>', sub {
	push_output('file', "$manpage_title.$manpage_sect");

	output <<_END_BANNER;
.\\" This manpage has been automatically generated by docbook2man 
.\\" from a DocBook document.  This tool can be found at:
.\\" <http://shell.ipoline.com/~elmert/comp/docbook2X/> 
.\\" Please send any bug reports, improvements, comments, patches, 
.\\" etc. to Steve Cheng <steve\@ggi-project.org>.
_END_BANNER

	my $manpage_date = `date "+%d %B %Y"`;
		
	output '.TH "';
	
	# If the title is not mixed-case, convention says to
	# uppercase the whole title.  (The canonical title is
	# lowercase.)
	if($manpage_title =~ /[A-Z]/) {
		output fold_string($manpage_title);
	} else {
		output uc(fold_string($manpage_title));
	}
	
	output  '" "', fold_string($manpage_sect), 
		'" "', fold_string(`date "+%d %B %Y"`), 
		'" "', $manpage_misc, 
		'" "', $manpage_manual, 
		"\"\n";

	$newline_last = 1;

	# References to this RefEntry.
	my $id = $_[0]->parent->attribute('ID')->value;
	if($id ne '') {
		# The 'package name' part of the section should
		# not be used when citing it.
		my ($sectnum) = ($manpage_sect =~ /([0-9]*)/);
		
		if($_[0]->parent->attribute('XREFLABEL')->value eq '') {
			$Refs->put("refentry:$id", "$manpage_title($sectnum)");
		} else {
			$Refs->put("refentry:$id",
				$_[0]->parent->attribute('XREFLABEL')->value . 
				"($sectnum)");
		}
	}
});

sgml('<REFENTRYTITLE>', sub { 
	if($_[0]->in('REFMETA')) { 
		save_cdata();
	} else { 
		# Manpage citations are in bold.
		bold_on();
	}
});
sgml('</REFENTRYTITLE>', sub { 
	if($_[0]->in('REFMETA')) {
		$raw_cdata--;
		$manpage_title = pop_output();
	}
	else { font_off(); }
});

sgml('<MANVOLNUM>', sub { 
	if($_[0]->in('REFMETA')) { 
		save_cdata();	
	} else {
		# Manpage citations use ().
		output '(';
	}
});
sgml('</MANVOLNUM>', sub { 
	if($_[0]->in('REFMETA')) {
		$raw_cdata--;
		$manpage_sect = pop_output();
	}
	else { output ')' }
});

sgml('<REFMISCINFO>', \&save_cdata);
sgml('</REFMISCINFO>', sub { 
	$raw_cdata--;
	$manpage_misc = fold_string(pop_output());
});


# NAME section
man_sgml('<REFNAMEDIV>', "\n.SH NAME\n");

sgml('<REFNAME>', \&save_cdata);
sgml('</REFNAME>', sub { 
	$raw_cdata--;
	push(@manpage_names, pop_output());
});

sgml('<REFPURPOSE>', \&save_cdata);
sgml('</REFPURPOSE>', sub { 
	$raw_cdata--;
	my $manpage_purpose = fold_string(pop_output());
	
	for(my $i = 0; $i < $#manpage_names; $i++) {
		output fold_string($manpage_names[$i]), ', ';
	}

	output fold_string($manpage_names[$#manpage_names]);
	output " \\- $manpage_purpose\n";

	$newline_last = 1;

	foreach(@manpage_names) {
		# Don't link to itself
		if($_ ne $manpage_title) {
			print LINKSFILE "$manpage_title.$manpage_sect	$_.$manpage_sect\n";
		}
	}
});
	
man_sgml('<REFCLASS>', "\n.sp\n");

#RefDescriptor





########################################################################
#
# SYNOPSIS section and synopses
#
########################################################################

man_sgml('<REFSYNOPSISDIV>', "\n.SH SYNOPSIS\n");
man_sgml('</REFSYNOPSISDIV>', "\n");

## FIXME! Must be made into block elements!!
#sgml('<FUNCSYNOPSIS>', \&bold_on);
#sgml('</FUNCSYNOPSIS>', \&font_off);
#sgml('<CMDSYNOPSIS>', \&bold_on);
#sgml('</CMDSYNOPSIS>', \&font_off);

man_sgml('<FUNCSYNOPSIS>', sub {
	man_output("\n.sp\n");
	bold_on();
});
man_sgml('</FUNCSYNOPSIS>', sub {
	font_off();
	man_output("\n");
});

man_sgml('<CMDSYNOPSIS>', "\n\n");
man_sgml('</CMDSYNOPSIS>', "\n\n");

man_sgml('<FUNCPROTOTYPE>', "\n.sp\n");

# Arguments to functions.  This is C convention.
man_sgml('<PARAMDEF>', '(');
man_sgml('</PARAMDEF>', ");\n");
man_sgml('<VOID>', "(void);\n");



sub arg_start
{
	# my $choice = $_[0]->attribute('CHOICE')->value;

	# The content model for CmdSynopsis doesn't include #PCDATA,
	# so we won't see any of the whitespace in the source file,
	# so we have to add it after each component.
	output ' ';

	if($_[0]->attribute('CHOICE')->value =~ /opt/i) {
		output '[ ';
	}
	bold_on();
}
sub arg_end
{
	font_off();
	if($_[0]->attribute('REP')->value =~ /^Repeat/i) {
		italic_on();
		output ' ...';
		font_off();
	}
	if($_[0]->attribute('CHOICE')->value =~ /opt/i) {
		output '] ';
	}
}

sgml('<ARG>', \&arg_start);
sgml('</ARG>', \&arg_end);
sgml('<GROUP>', \&arg_start);
sgml('</GROUP>', \&arg_end);

sgml('<OPTION>', \&bold_on);
sgml('</OPTION>', \&font_off);

# FIXME: This is one _blank_ line.
man_sgml('<SBR>', "\n\n");


########################################################################
#
# General sections
#
########################################################################

# The name of the section is handled by TITLE.  This just sets
# up the roff markup.
man_sgml('<REFSECT1>', "\n.SH ");
man_sgml('<REFSECT2>', "\n.SS ");
man_sgml('<REFSECT3>', "\n.SS ");


########################################################################
#
# Titles, metadata.
#
########################################################################

sgml('<TITLE>', sub {
	if($_[0]->in('REFERENCE') or $_[0]->in('BOOK')) {
		$write_manpages = 1;
	}
	save_cdata();
});
sgml('</TITLE>', sub { 
	my $title = fold_string(pop_output());
	$raw_cdata--;
	
	if($_[0]->in('REFERENCE') or $_[0]->in('BOOK')) {
		# We use TITLE of enclosing Reference or Book as manual name
		$manpage_manual = $title;
		$write_manpages = 0;
	}
	elsif(exists $_[0]->parent->ext->{'title'}) {
		# By far the easiest case.  Just fold the string as
		# above, and then set the parent element's variable.
		$_[0]->parent->ext->{'title'} = $title;
	}
	else {
		# If the parent element's handlers are lazy, 
		# output the folded string for them :)
		# We assume they want uppercase and a newline.
		output '"', uc($title), "\"\n";
		$newline_last = 1;
	}
});

sgml('<ATTRIBUTION>', sub { push_output('string') });
sgml('</ATTRIBUTION>', sub { $_[0]->parent->ext->{'attribution'} = pop_output(); });


# IGNORE.
sgml('<DOCINFO>', sub { push_output('nul'); });
sgml('</DOCINFO>', sub { pop_output(); });
sgml('<REFSECT1INFO>', sub { push_output('nul'); });
sgml('</REFSECT1INFO>', sub { pop_output(); });
sgml('<REFSECT2INFO>', sub { push_output('nul'); });
sgml('</REFSECT2INFO>', sub { pop_output(); });
sgml('<REFSECT3INFO>', sub { push_output('nul'); });
sgml('</REFSECT3INFO>', sub { pop_output(); });

sgml('<INDEXTERM>', sub { push_output('nul'); });
sgml('</INDEXTERM>', sub { pop_output(); });


########################################################################
#
# Set bold on enclosed content 
#
########################################################################

sgml('<APPLICATION>', \&bold_on);	sgml('</APPLICATION>', \&font_off);

sgml('<CLASSNAME>', \&bold_on);		sgml('</CLASSNAME>', \&font_off);
sgml('<STRUCTNANE>', \&bold_on);	sgml('</STRUCTNAME>', \&font_off);
sgml('<STRUCTFIELD>', \&bold_on);	sgml('</STRUCTFIELD>', \&font_off);
sgml('<SYMBOL>', \&bold_on);		sgml('</SYMBOL>', \&font_off);
sgml('<TYPE>', \&bold_on);		sgml('</TYPE>', \&font_off);

sgml('<ENVAR>', \&bold_on);	sgml('</ENVAR>', \&font_off);

sgml('<FUNCTION>', \&bold_on);	sgml('</FUNCTION>', \&font_off);

sgml('<EMPHASIS>', \&bold_on);	sgml('</EMPHASIS>', \&font_off);

sgml('<ERRORNAME>', \&bold_on);	sgml('</ERRORNAME>', \&font_off);
# ERRORTYPE

sgml('<COMMAND>', \&bold_on);	sgml('</COMMAND>', \&font_off);

sgml('<GUIBUTTON>', \&bold_on);	sgml('</GUIBUTTON>', \&font_off);
sgml('<GUIICON>', \&bold_on);	sgml('</GUIICON>', \&font_off);
# GUILABEL
# GUIMENU
# GUIMENUITEM
# GUISUBMENU
# MENUCHOICE
# MOUSEBUTTON

sgml('<ACCEL>', \&bold_on);	sgml('</ACCEL>', \&font_off);
sgml('<KEYCAP>', \&bold_on);	sgml('</KEYCAP>', \&font_off);
sgml('<KEYSYM>', \&bold_on);	sgml('</KEYSYM>', \&font_off);
# KEYCODE
# KEYCOMBO
# SHORTCUT

sgml('<USERINPUT>', \&bold_on);	sgml('</USERINPUT>', \&font_off);

sgml('<INTERFACEDEFINITION>', \&bold_on);
sgml('</INTERFACEDEFINITION>', \&font_off);

# May need to look at the CLASS
sgml('<SYSTEMITEM>', \&bold_on);
sgml('</SYSTEMITEM>', \&font_off);





########################################################################
#
# Set italic on enclosed content 
#
########################################################################

sgml('<FIRSTTERM>', \&italic_on);	sgml('</FIRSTTERM>', \&font_off);

sgml('<FILENAME>', \&italic_on);	sgml('</FILENAME>', \&font_off);
sgml('<PARAMETER>', \&italic_on);	sgml('</PARAMETER>', \&font_off);
sgml('<PROPERTY>', \&italic_on);	sgml('</PROPERTY>', \&font_off);

sgml('<REPLACEABLE>', sub {
	italic_on();
	if($_[0]->in('TOKEN')) {
		# When tokenizing, follow more 'intuitive' convention
		output "<";
	}
});
sgml('</REPLACEABLE>', sub {
	if($_[0]->in('TOKEN')) {
		output ">";
	}
	font_off();
});

sgml('<CITETITLE>', \&italic_on);	sgml('</CITETITLE>', \&font_off);
sgml('<FOREIGNPHRASE>', \&italic_on);	sgml('</FOREIGNPHRASE>', \&font_off);

sgml('<LINEANNOTATION>', \&italic_on);	sgml('</LINEANNOTATION>', \&font_off);






########################################################################
#
# Other 'inline' elements 
#
########################################################################

man_sgml('<EMAIL>', '<');
man_sgml('</EMAIL>', '>');
man_sgml('<OPTIONAL>', '[');
man_sgml('</OPTIONAL>', ']');

man_sgml('</TRADEMARK>', "\\u\\s-2TM\\s+2\\d");

man_sgml('<COMMENT>', "[Comment: ");
man_sgml('</COMMENT>', "]");

man_sgml('<QUOTE>', "``");
man_sgml('</QUOTE>', "''");

#man_sgml('<LITERAL>', '"');
#man_sgml('</LITERAL>', '"');

# No special presentation:

# AUTHOR
# AUTHORINITIALS

# ABBREV
# ACTION
# ACRONYM
# ALT
# CITATION
# PHRASE
# QUOTE
# WORDASWORD

# COMPUTEROUTPUT
# MARKUP
# PROMPT
# RETURNVALUE
# SGMLTAG
# TOKEN

# DATABASE
# HARDWARE
# INTERFACE
# MEDIALABEL

# There doesn't seem to be a good way to represent LITERAL in -man



########################################################################
#
# Paragraph and paragraph-like elements 
#
########################################################################

sub para_start {
	output "\n" unless $newline_last++;

	# In lists, etc., don't start paragraph with .PP since
	# the indentation will be gone.

	if($_[0]->parent->ext->{'nobreak'}==1) {
		# Usually this is the FIRST element of
		# a hanging tag, so we MUST not do a full
		# paragraph break.
		$_[0]->parent->ext->{'nobreak'} = 2;
	} elsif($_[0]->parent->ext->{'nobreak'}==2) {
		# Usually these are the NEXT elements of
		# a hanging tag.  If we break using a blank
		# line, we're okay.
		output "\n";
	} else {
		# Normal case. (For indented blocks too, at least
		# -man isn't so braindead in this area.)
		output ".PP\n";
	}
}
# Actually applies to a few other block elements as well
sub para_end {
	output "\n" unless $newline_last++; 
}

sgml('<PARA>', \&para_start);
sgml('</PARA>', \&para_end);
sgml('<SIMPARA>', \&para_start);
sgml('</SIMPARA>', \&para_end);

# Nothing special, except maybe FIXME set nobreak.
sgml('<INFORMALEXAMPLE>', \&para_start);
sgml('</INFORMALEXAMPLE>', \&para_end);





########################################################################
#
# Blocks using SS sections
#
########################################################################

# FIXME: We need to consider the effects of SS
# in a hanging tag :(

# Complete with the optional-title dilemma (again).
sgml('<ABSTRACT>', sub {
	$_[0]->ext->{'title'} = 'ABSTRACT';
	output "\n" unless $newline_last++;
	push_output('string');
});
sgml('</ABSTRACT>', sub {
	my $content = pop_output();
	
	# As ABSTRACT is never on the same level as RefSect1,
	# this leaves us with only .SS in terms of -man macros.
	output ".SS \"", uc($_[0]->ext->{'title'}), "\"\n";

	output $content;
	output "\n" unless $newline_last++;
});

# Ah, I needed a break.  Example always has a title.
man_sgml('<EXAMPLE>', "\n.SS ");
sgml('</EXAMPLE>', \&para_end);

# Same with sidebar.
man_sgml('<SIDEBAR>', "\n.SS ");
sgml('</SIDEBAR>', \&para_end);

# NO title.
man_sgml('<HIGHLIGHTS>', "\n.SS HIGHLIGHTS\n");
sgml('</HIGHLIGHTS>', \&para_end);




########################################################################
#
# Indented 'Block' elements 
#
########################################################################

sub indent_block_start
{
	output "\n" unless $newline_last++;
	output ".sp\n.RS\n";
}
sub indent_block_end
{
	output "\n" unless $newline_last++;
	output ".RE\n";
}

# This element is almost like an admonition (below),
# only the default title is blank :)

sgml('<BLOCKQUOTE>', sub { 
	$_[0]->ext->{'title'} = ''; 
	output "\n" unless $newline_last++;
	push_output('string');
});
sgml('</BLOCKQUOTE>', sub {
	my $content = pop_output();

	indent_block_start();
	
	if($_[0]->ext->{'title'}) {
		output ".B \"", $_[0]->ext->{'title'}, ":\"\n";
	}
	
	output $content;

	if($_[0]->ext->{'attribution'}) {
		output "\n" unless $newline_last++;
		# One place where roff's space-sensitivity makes sense :)
		output "\n                -- ";
		output $_[0]->ext->{'attribution'} . "\n";
	}
	
	indent_block_end();
});

# Set off admonitions from the rest of the text by indenting.
# FIXME: Need to check if this works inside paragraphs, not enclosing them.
sub admonition_end {
	my $content = pop_output();

	indent_block_start();
	
	# When the admonition is only one paragraph,
	# it looks nicer if the title was inline.
	my $num_para;
	while ($content =~ /^\.PP/gm) { $num_para++ }
	if($num_para==1) {
		$content =~ s/^\.PP\n//;
	}
	
	output ".B \"" . $_[0]->ext->{'title'} . ":\"\n";
	output $content;
	
	indent_block_end();
}

sgml('<NOTE>', sub {
	# We can't see right now whether or not there is a TITLE
	# element, so we have to save the output now and add it back
	# at the end of this admonition.
	$_[0]->ext->{'title'} = 'Note';

	# Although admonition_end's indent_block_start will do this,
	# we need to synchronize the output _now_
	output "\n" unless $newline_last++;

	push_output('string');
});
sgml('</NOTE>', \&admonition_end);

# Same as above.
sgml('<WARNING>', sub { 
	$_[0]->ext->{'title'} = 'Warning'; 
	output "\n" unless $newline_last++;
	push_output('string');
});
sgml('</WARNING>', \&admonition_end);

sgml('<TIP>', sub {
	$_[0]->ext->{'title'} = 'Tip';
	output "\n" unless $newline_last++;
	push_output('string');
});
sgml('</TIP>', \&admonition_end);
sgml('<CAUTION>', sub {
	$_[0]->ext->{'title'} = 'Caution';
	output "\n" unless $newline_last++;
	push_output('string');
});
sgml('</CAUTION>', \&admonition_end);

sgml('<IMPORTANT>', sub {
	$_[0]->ext->{'title'} = 'Important';
	output "\n" unless $newline_last++;
	push_output('string');
});
sgml('</IMPORTANT>', \&admonition_end);












########################################################################
#
# Verbatim displays. 
#
########################################################################

sub verbatim_start {
	output "\n" unless $newline_last++;
	
	if($_[0]->parent->ext->{'nobreak'}==1) {
		# Usually this is the FIRST element of
		# a hanging tag, so we MUST not do a full
		# paragraph break.
		$_[0]->parent->ext->{'nobreak'} = 2;
	} else {
		output "\n";
	}
	
	output(".nf\n") unless $nocollapse_whitespace++;
}

sub verbatim_end {
	output "\n" unless $newline_last++;
	output(".fi\n") unless --$nocollapse_whitespace;
}

sgml('<PROGRAMLISTING>', \&verbatim_start); 
sgml('</PROGRAMLISTING>', \&verbatim_end);

sgml('<SCREEN>', \&verbatim_start); 
sgml('</SCREEN>', \&verbatim_end);

sgml('<LITERALLAYOUT>', \&verbatim_start); 
sgml('</LITERALLAYOUT>', \&verbatim_end);

#sgml('<SYNOPSIS>', sub {
#	if($_[0]->attribute('FORMAT')->value =~ /linespecific/i) {
#		&verbatim_start;
#	} else {
#		roffcmd("");
#	}
#});
#
#sgml('</SYNOPSIS>', sub {
#	if($_[0]->attribute('FORMAT')->value =~ /linespecific/i) {
#		&verbatim_end;
#	}
#	else {
#		roffcmd("");# not sure about this.
#	}
#});
sgml('<SYNOPSIS>', \&verbatim_start);
sgml('</SYNOPSIS>', \&verbatim_end);









########################################################################
#
# Lists
#
########################################################################

# Indent nested lists.
sub indent_list_start {
	if($list_nestlevel++) {
		output "\n" unless $newline_last++;
		output ".RS\n";
	}
}
sub indent_list_end {
	if(--$list_nestlevel) {
		output "\n" unless $newline_last++;
		output ".RE\n";
	}
}

sgml('<VARIABLELIST>', \&indent_list_start);
sgml('</VARIABLELIST>', \&indent_list_end);
sgml('<ITEMIZEDLIST>', \&indent_list_start);
sgml('</ITEMIZEDLIST>', \&indent_list_end);
sgml('<ORDEREDLIST>', sub { 
	indent_list_start();
	$_[0]->ext->{'count'} = 1;
});
sgml('</ORDEREDLIST>', \&indent_list_end);
		
# Output content on one line, bolded.
sgml('<TERM>', sub { 
	output "\n" unless $newline_last++;
	output ".TP\n";
	bold_on();
	push_output('string');
});
sgml('</TERM>', sub { 
	my $term = pop_output();
	$term =~ tr/\n/ /;
	output $term;
	font_off();
	output "\n";
	$newline_last = 1;
});
	
sgml('<LISTITEM>', sub {
	# A bulleted list.
	if($_[0]->in('ITEMIZEDLIST')) {
		output "\n" unless $newline_last++;
		output ".TP 0.2i\n\\(bu\n";
	}

	# Need numbers.
	# Assume Arabic numeration for now.
	elsif($_[0]->in('ORDEREDLIST')) {
		output "\n" unless $newline_last++;
		output ".TP ", $_[0]->parent->ext->{'count'}++, ". \n";
	}
	
	$_[0]->ext->{'nobreak'} = 1;
});

sgml('<SIMPLELIST>', sub {
	$_[0]->ext->{'first_member'} = 1;
});

sgml('<MEMBER>', sub {
	my $parent = $_[0]->parent;
	
	if($parent->attribute('TYPE')->value =~ /Inline/i) {
		if($parent->ext->{'first_member'}) { 
			# If this is the first member don't put any commas
			$parent->ext->{'first_member'} = 0;
		} else {
			output ", ";
		}
	} elsif($parent->attribute('TYPE')->value =~ /Vert/i) {
		output "\n" unless $newline_last++;
		output "\n";
	}
});





########################################################################
#
# Stuff we don't know how to handle (yet) 
#
########################################################################

# Address blocks:

# Credit stuff:
# ACKNO
# ADDRESS
# AFFILIATION
# ARTPAGENUMS
# ATTRIBUTION
# AUTHORBLURB
# AUTHORGROUP
# OTHERCREDIT
# HONORIFIC

# Areas:
# AREA
# AREASET
# AREASPEC





########################################################################
#
# Linkage, cross references
#
########################################################################

# Print the URL
sgml('</ULINK>', sub {
#	output ' <URL:', $_[0]->attribute('URL')->value, '>';
	$newline_last = 0;
});

# If cross reference target is a RefEntry, 
# output CiteRefEntry-style references.
sgml('<XREF>', sub {
	my $id = $_[0]->attribute('LINKEND')->value;
	my $manref = $Refs->get("refentry:$id");

	if($manref) {
		my ($title, $sect) = ($manref =~ /(.*)(\(.*\))/);
		bold_on();
		output $title;
		font_off();
		output $sect;
	} else {
		$blank_xrefs++ if $write_manpages;
		output "[XRef to $id]";
	}

	$newline_last = 0;
});

# Anchor




########################################################################
#
# Other handlers 
#
########################################################################

man_sgml('|[lt    ]|', '<');
man_sgml('|[gt    ]|', '>');
man_sgml('|[amp   ]|', '&');

#
# Default handlers (uncomment these if needed).  Right now, these are set
# up to gag on any unrecognised elements, sdata, processing-instructions,
# or entities.
#
# sgml('start_element',sub { die "Unknown element: " . $_[0]->name; });
# sgml('end_element','');

# This is for weeding out and escaping certain characters.
# This looks like it's inefficient since it's done on every line, but
# in reality, SGMLSpm and sgmlspl parsing ESIS takes _much_ longer.

sgml('cdata', sub
{ 
	if(!$write_manpages) { return; }
	elsif($raw_cdata) { output $_[0]; return; }
	
	# Escape backslashes
	$_[0] =~ s/\\/\\\\/g;

	# In non-'pre'-type elements:
	if(!$nocollapse_whitespace) {
		# Change tabs to spaces
		$_[0] =~ tr/\t/ /;

		# Do not allow indents at beginning of line
		# groff chokes on that.
		if($newline_last) { 
			$_[0] =~ s/^ +//;

			# If the line is all blank, don't do anything.
			if($_[0] eq '') { return; }
			
			$_[0] =~ s/^\./\\\&\./;

			# Argh... roff doesn't like ' either...
			$_[0] =~ s/^\'/\\\&\'/;
		}
	}

	$newline_last = 0;

	output $_[0];
});


# When in whitespace-collapsing mode, we disallow consecutive newlines.

sgml('re', sub
{
	if($nocollapse_whitespace || !$newline_last) {
		output "\n";
	}

	$newline_last = 1;
});

sgml('sdata',sub { die "Unknown SDATA: " . $_[0]; });
sgml('pi',sub { die "Unknown processing instruction: " . $_[0]; });
sgml('entity',sub { die "Unknown external entity: " . $_[0]->name; });
sgml('start_subdoc',sub { die "Unknown subdoc entity: " . $_[0]->name; });
sgml('end_subdoc','');
sgml('conforming','');

1;

