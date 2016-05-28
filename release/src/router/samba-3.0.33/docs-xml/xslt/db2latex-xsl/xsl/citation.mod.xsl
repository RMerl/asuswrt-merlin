<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: citation.mod.xsl,v 1.6 2003/12/29 01:30:32 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
+ ############################################################################## -->

<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>


	<doc:reference id="citation" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: citation.mod.xsl,v 1.6 2003/12/29 01:30:32 j-devenish Exp $
			</releaseinfo>
			<authorgroup>
				&ramon;
				&james;
			</authorgroup>
			<copyright>
				<year>2000</year><year>2001</year><year>2002</year><year>2003</year>
				<holder>Ramon Casellas</holder>
			</copyright>
			<revhistory>
				<doc:revision rcasver="1.6">&rev_2003_05;</doc:revision>
			</revhistory>
		</referenceinfo>
		<title>Citations <filename>citation.mod.xsl</filename></title>
		<partintro>

			<para>This file contains a single XSL template that maps <doc:db
			basename="citation">citations</doc:db> to the &LaTeX;
			<function condition="latex">cite{}</function>. command.</para>

		</partintro>
	</doc:reference>

	<doc:template basename="citation" xmlns="">
		<refpurpose> Process <doc:db>citation</doc:db> elements  </refpurpose>
		<doc:description>

			<para>

			Outputs a <function condition="latex">cite{...}</function> command
			using the text value of the <doc:db>citation</doc:db>.

			</para>

		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			&essential_preamble;
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_book;
				&test_cited;
			</simplelist>
		</doc:samples>
	</doc:template>

	<xsl:template match="citation">
		<!-- todo: biblio-citation-check -->
		<xsl:text>\docbooktolatexcite{</xsl:text>
			<xsl:value-of select="."/>
		<xsl:text>}{}</xsl:text>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose> Essential preamble for <filename>citation.mod.xsl</filename> support </refpurpose>
		<doc:description>
			<para>

				Defines <function
				condition="latex">docbooktolatexcite</function>. This function
				helps to integrate <doc:db>bibioentry</doc:db>/@<sgmltag
				class="attribute">id</sgmltag> and
				<doc:db>bibioentry</doc:db>/<doc:db>abbrev</doc:db> with
				&LaTeX;.
				Also defines <function condition="latex">docbooktolatexbackcite</function>
				for compatability with <productname>hyperref</productname>'s
				<productname>backref</productname> functionality.

			</para>
		</doc:description>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara>&preamble;</simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>

	<xsl:template name="latex.preamble.essential.citation">
		<xsl:text>
			<![CDATA[
%\usepackage{cite}
%\renewcommand\citeleft{(}  % parentheses around list
%\renewcommand\citeright{)} % parentheses around list
\newcommand{\docbooktolatexcite}[2]{%
  \@ifundefined{docbooktolatexcite@#1}%
  {\cite{#1}}%
  {\def\@docbooktolatextemp{#2}\ifx\@docbooktolatextemp\@empty%
   \cite{\@nameuse{docbooktolatexcite@#1}}%
   \else\cite[#2]{\@nameuse{docbooktolatexcite@#1}}%
   \fi%
  }%
}
\newcommand{\docbooktolatexbackcite}[1]{%
  \ifx\Hy@backout\@undefined\else%
    \@ifundefined{docbooktolatexcite@#1}{%
      % emit warning?
    }{%
      \ifBR@verbose%
        \PackageInfo{backref}{back cite \string`#1\string' as \string`\@nameuse{docbooktolatexcite@#1}\string'}%
      \fi%
      \Hy@backout{\@nameuse{docbooktolatexcite@#1}}%
    }%
  \fi%
}
]]>
		</xsl:text>
	</xsl:template>

</xsl:stylesheet>


