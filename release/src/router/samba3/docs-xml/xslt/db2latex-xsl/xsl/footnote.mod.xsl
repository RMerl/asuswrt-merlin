<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: footnote.mod.xsl,v 1.10 2004/01/02 06:45:25 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
+ ############################################################################## -->
<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="footnote" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: footnote.mod.xsl,v 1.10 2004/01/02 06:45:25 j-devenish Exp $
			</releaseinfo>
			<authorgroup>
				&ramon;
				&james;
			</authorgroup>
			<copyright>
				<year>2000</year><year>2001</year><year>2002</year><year>2003</year><year>2004</year>
				<holder>Ramon Casellas</holder>
			</copyright>
			<revhistory>
				<doc:revision rcasver="1.10">&rev_2003_05;</doc:revision>
			</revhistory>
		</referenceinfo>
		<title>Footnotes <filename>footnote.mod.xsl</filename></title>
		<partintro>
			<para>
			
			
			
			</para>
		</partintro>
	</doc:reference>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>footnote</doc:db> elements</refpurpose>
		<doc:description>
			<para>
			
			Format a footnote.
			
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				Applies templates within a &LaTeX; <function
				condition="latex">footnote</function> command. Note that this
				may not work within some tables. Also, <doc:db
				basename="indexterm">indexterms</doc:db> may fail.
			</para>
			&essential_preamble;
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_book;
				&test_links;
				&test_tables;
			</simplelist>
		</doc:samples>
	</doc:template>
	<xsl:template match="footnote">
		<xsl:call-template name="label.id"/>
		<xsl:text>\begingroup\catcode`\#=12\footnote{</xsl:text>
		<xsl:apply-templates/>
		<xsl:text>}\endgroup\docbooktolatexmakefootnoteref{</xsl:text>
		<xsl:call-template name="generate.label.id"/>
		<xsl:text>}</xsl:text>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose> Essential preamble for <filename>footnote.mod.xsl</filename> support </refpurpose>
		<doc:description>
			<para>

				Defines <function
				condition="latex">docbooktolatexusefootnoteref</function> and
				<function
				condition="latex">docbooktolatexmakefootnoteref</function>.
				These functions help to integrate
				<doc:db>footnote</doc:db>/@<sgmltag
				class="attribute">id</sgmltag> cross-references with &LaTeX;.

			</para>
		</doc:description>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara>&preamble;</simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template name="latex.preamble.essential.footnote">
		<xsl:text>
			<![CDATA[
% --------------------------------------------
% A way to honour <footnoteref>s
% Blame j-devenish (at) users.sourceforge.net
% In any other LaTeX context, this would probably go into a style file.
\newcommand{\docbooktolatexusefootnoteref}[1]{\@ifundefined{@fn@label@#1}%
  {\hbox{\@textsuperscript{\normalfont ?}}%
    \@latex@warning{Footnote label `#1' was not defined}}%
  {\@nameuse{@fn@label@#1}}}
\newcommand{\docbooktolatexmakefootnoteref}[1]{%
  \protected@write\@auxout{}%
    {\global\string\@namedef{@fn@label@#1}{\@makefnmark}}%
  \@namedef{@fn@label@#1}{\hbox{\@textsuperscript{\normalfont ?}}}%
  }
]]>
		</xsl:text>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>ulink</doc:db> elements within <doc:db>footnote</doc:db>s</refpurpose>
		<doc:description>
			<para>
			Format a <doc:db>ulink</doc:db>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				Footnotes are <quote>tricky</quote> and require special handling for
				a number of link-type commands.
			</para>
			<para>This template exists in this file so that all
			the footnote-generating templates are close to each other. However,
			it is actually a part of the <literal>ulink</literal> template in <filename>xref.mod.xsl</filename></para>
		</doc:notes>
	</doc:template>
	<xsl:template name="generate.ulink.in.footnote">
		<xsl:param name="hyphenation"/>
		<xsl:param name="url"/>
		<xsl:call-template name="label.id"/>
		<xsl:text>\begingroup\catcode`\#=12\footnote{</xsl:text>
		<xsl:call-template name="generate.typeset.url">
			<xsl:with-param name="hyphenation" select="$hyphenation"/>
			<xsl:with-param name="url" select="$url"/>
		</xsl:call-template>
		<xsl:text>}\endgroup\docbooktolatexmakefootnoteref{</xsl:text>
		<xsl:call-template name="generate.label.id"/>
		<xsl:text>}</xsl:text>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>footnote</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Make a link to a <doc:db>footnote</doc:db>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			&essential_preamble;
		</doc:notes>
	</doc:template>
	<xsl:template match="footnoteref">
		<xsl:variable name="footnote" select="key('id',@linkend)"/>
		<xsl:text>\docbooktolatexusefootnoteref{</xsl:text>
		<xsl:value-of select="@linkend"/>
		<xsl:text>}</xsl:text>
	</xsl:template>

</xsl:stylesheet>
