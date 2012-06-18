<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: glossary.mod.xsl,v 1.16 2004/01/26 08:58:10 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
+ ############################################################################## -->

<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="glossary" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: glossary.mod.xsl,v 1.16 2004/01/26 08:58:10 j-devenish Exp $
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
				<doc:revision rcasver="1.12">&rev_2003_05;</doc:revision>
			</revhistory>
		</referenceinfo>
		<title>Glossaries <filename>glossary.mod.xsl</filename></title>
		<partintro>
			<para>
			
			Although &LaTeX; provides some glossary support, the better glossary
			management support motivates the bypass of the &LaTeX;
			<function condition="latex">makeglossary</function> command.
			
			</para>
		</partintro>
	</doc:reference>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>glossary</doc:db> elements</refpurpose>
		<doc:description>
			<para>
			
			The <doc:db>glossary</doc:db> element is the entry point to a
			&DocBook; glossary. The &DB2LaTeX; processing of the element is
			quite straight- forward. First thing is to check whether the
			document is a <doc:db>book</doc:db> or <doc:db>article</doc:db>. In
			both cases, two new &LaTeX; commands are defined: <function
			condition="latex">dbglossary</function> and <function
			condition="latex">dbglossdiv</function>. In the former case, they
			are mapped to <function condition="latex">chapter*</function> and
			<function condition="latex">section*</function>. In the second case
			to <function condition="latex">section*</function> and <function
			condition="latex">subsection*</function>. The <function
			condition="env">description</function> environment is used for
			<doc:db basename="glossentry">glossentries</doc:db>.
			
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<itemizedlist>
				<listitem><para>Call template map.begin.</para></listitem>
				<listitem><para>Apply Templates for Preamble, GlossDivs and GlossEntries (serial).</para></listitem>
				<listitem><para>Call template map.end.</para></listitem>
			</itemizedlist>
			&essential_preamble;
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_book;
				&test_gloss;
				&test_ieeebiblio;
				&test_mapping;
			</simplelist>
		</doc:samples>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara>&mapping;</simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="glossary">
		<xsl:variable name="divs" select="glossdiv"/>
		<xsl:variable name="entries" select="glossentry"/>
		<xsl:variable name="preamble" select="node()[not(self::glossaryinfo or self::title or self::subtitle or self::titleabbrev or self::glossdiv or self::glossentry or self::bibliography)]"/>
		<xsl:call-template name="map.begin"/>
		<!--
		<xsl:if test="./subtitle"><xsl:apply-templates select="./subtitle" mode="component.title.mode"/> </xsl:if>
		-->
		<xsl:if test="$preamble"> <xsl:apply-templates select="$preamble"/> </xsl:if>
		<xsl:if test="$divs"> <xsl:apply-templates select="$divs"/> </xsl:if>
		<xsl:if test="$entries">
			<xsl:text>\noindent%&#10;</xsl:text>
			<xsl:text>\begin{description}&#10;</xsl:text>
			<xsl:apply-templates select="$entries"/>
			<xsl:text>\end{description}&#10;</xsl:text>
		</xsl:if>
		<xsl:call-template name="map.end"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>glossdiv</doc:db> and <doc:db>glosslist</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes the mapping templates and applies content templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara>&mapping;</simpara></listitem>
				<listitem><simpara><xref linkend="template.content-templates"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="glossdiv|glosslist">
		<xsl:call-template name="map.begin"/>
		<xsl:call-template name="content-templates"/>
		<xsl:call-template name="map.end"/>
	</xsl:template>

	<!--
    <doc:template match="glossentry" xmlns="">
	<refpurpose> Glossary Entry XSL template / entry point  </refpurpose>
	<doc:description>
	    <para>T.B.D.</para>
	</doc:description>
	<itemizedlist>
	    <listitem><para>Apply Templates.</para></listitem>
	</itemizedlist>
	<formalpara><title>Remarks and Bugs</title>
	    <itemizedlist>
		<listitem><para>Explicit Templates for <literal>glossentry/glossterm</literal></para></listitem>
		<listitem><para>Explicit Templates for <literal>glossentry/acronym</literal></para></listitem>
		<listitem><para>Explicit Templates for <literal>glossentry/abbrev</literal></para></listitem>
		<listitem><para>Explicit Templates for <literal>glossentry/glossdef</literal></para></listitem>
		<listitem><para>Explicit Templates for <literal>glossentry/glosssee</literal></para></listitem>
		<listitem><para>Explicit Templates for <literal>glossentry/glossseealso</literal></para></listitem>
		<listitem><para>Template for glossentry/revhistory is EMPTY.</para></listitem>
	    </itemizedlist>
	</formalpara>
    </doc:template>
	-->
	<doc:template xmlns="">
		<refpurpose>Process <doc:db>glossentry</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Simply applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="glossentry">
		<xsl:apply-templates/>
		<xsl:text>&#10;&#10;</xsl:text>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process a <doc:db>glossentry</doc:db>'s <doc:db>glossterm</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Creates a &LaTeX; <function condition="latex">item</function> and
				a <function condition="latex">hypertarget</function>, then applies
				templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="glossentry/glossterm">
		<xsl:text>\item[</xsl:text>
		<xsl:if test="../@id!=''">
			<xsl:text>\hypertarget{</xsl:text>
			<xsl:value-of select="../@id"/>
			<xsl:text>}</xsl:text>
		</xsl:if>
		<xsl:text>{</xsl:text>
		<xsl:apply-templates/>
		<xsl:text>}] </xsl:text>
	</xsl:template>

	<doc:template basename="acronym" xmlns="">
		<refpurpose>Process a <doc:db>glossentry</doc:db>'s <doc:db>acronym</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Format an acronym as part of a glossentry.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				The <doc:db>acronym</doc:db> is formatted as monospaced text
				and delimited by round brackets. It appears in the <quote>body</quote>
				(e.g. <doc:db>glossdef</doc:db>) region of the glossary entry, not
				as part of the <doc:db>glossterm</doc:db>.
			</para>
			<para>
				The delimiters should probably be localised.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="glossentry/acronym">
	<xsl:text> ( </xsl:text> <xsl:call-template name="inline.monoseq"/> <xsl:text> ) </xsl:text>
	</xsl:template>

	<doc:template basename="abbrev" xmlns="">
		<refpurpose>Process a <doc:db>glossentry</doc:db>'s <doc:db>abbrev</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Format an abbrev as part of a glossentry.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				The <doc:db>abbrev</doc:db> is delimited by square brackets. It
				appears in the <quote>body</quote> (e.g.
				<doc:db>glossdef</doc:db>) region of the glossary entry, not as
				part of the <doc:db>glossterm</doc:db>.
			</para>
			<para>
				The delimiters should probably be localised.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="glossentry/abbrev">
	<xsl:text> [ </xsl:text> <xsl:apply-templates/> <xsl:text> ] </xsl:text> 
	</xsl:template>

	<doc:template basename="revhistory" xmlns="">
		<refpurpose>Process a <doc:db>glossentry</doc:db>'s <doc:db>revhistory</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Currently, <doc:db basename="revhistory">revhistories</doc:db>
				are suppressed within <doc:db basename="glossentry">glossentries</doc:db>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="glossentry/revhistory"/>

	<doc:template basename="glossdef" xmlns="">
		<refpurpose>Process a <doc:db>glossentry</doc:db>'s <doc:db>glossdef</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="glossentry/glossdef">
		<xsl:text>&#10;</xsl:text>
		<xsl:apply-templates/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>glossseealso</doc:db> and <doc:db>glosssee</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Format a glossary cross-reference.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
			
			Will call the following gentet templates:
			<literal>gentext.element.name</literal>,
			<literal>gentext.space</literal>,
			<literal>gentext.startquote</literal>,
			<literal>gentext.endquote</literal>.
			It will then output a full stop (<quote>period</quote>).
			
			</para>
			<para>
			
			If the <quote>otherterm</quote> was successfully found, and this
			element is empty, then the appropriate cross-reference will be
			generated. This this element is not empty but the otherterm was
			also found, the behaviour will depend on <xref
			linkend="param.latex.otherterm.is.preferred"/>.
			
			</para>
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_book;
				&test_gloss;
				&test_ieeebiblio;
			</simplelist>
		</doc:samples>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.otherterm.is.preferred"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="glossseealso|glosssee">
		<xsl:variable name="otherterm" select="@otherterm"/>
		<xsl:variable name="targets" select="key('id',$otherterm)"/>
		<xsl:variable name="target" select="$targets[1]"/>
		<xsl:call-template name="gentext.element.name"/>
		<xsl:call-template name="gentext.space"/>
		<xsl:call-template name="gentext.startquote"/>
		<xsl:choose>
			<xsl:when test="$otherterm">
				<xsl:text>\hyperlink{</xsl:text><xsl:value-of select="$otherterm"/>
				<xsl:text>}{</xsl:text>
				<xsl:choose>
					<xsl:when test="$latex.otherterm.is.preferred=1">
						<xsl:apply-templates select="$target" mode="xref"/>
					</xsl:when>
					<xsl:otherwise>
						<xsl:apply-templates/>
					</xsl:otherwise>
				</xsl:choose>
				<xsl:text>}</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<xsl:apply-templates/>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:call-template name="gentext.endquote"/>
		<xsl:text>. </xsl:text>
	</xsl:template>

	<xsl:template match="glossentry" mode="xref">
		<xsl:apply-templates select="./glossterm" mode="xref"/>
	</xsl:template>

	<xsl:template match="glossterm" mode="xref">
		<xsl:apply-templates/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose> Essential preamble for <filename>glossary.mod.xsl</filename> support </refpurpose>
		<doc:description>
			<para>

				Defines the <function condition="env">dbglossary</function>
				command.

			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara>&preamble;</simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template name="latex.preamble.essential.glossary">
		<xsl:if test="//glossary">
			<xsl:choose>
				<xsl:when test="/book or /part">
					<xsl:text>\newcommand{\dbglossary}[1]{\chapter*{#1}%&#10;</xsl:text>
					<xsl:text>\markboth{\MakeUppercase{#1}}{\MakeUppercase{#1}}}%&#10;</xsl:text>
					<xsl:text>\newcommand{\dbglossdiv}[1]{\section*{#1}}%&#10;</xsl:text>
				</xsl:when>
				<xsl:otherwise>
					<xsl:text>\newcommand{\dbglossary}[1]{\section*{#1}}%&#10;</xsl:text>
					<xsl:text>\newcommand{\dbglossdiv}[1]{\subsection*{#1}}%&#10;</xsl:text>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:if>
	</xsl:template>

</xsl:stylesheet>
