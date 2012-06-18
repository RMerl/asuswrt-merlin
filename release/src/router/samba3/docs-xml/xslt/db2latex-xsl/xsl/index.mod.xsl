<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: index.mod.xsl,v 1.17 2004/01/27 05:59:51 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
+ ############################################################################## -->
<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="index" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: index.mod.xsl,v 1.17 2004/01/27 05:59:51 j-devenish Exp $
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
				<doc:revision rcasver="1.11">&rev_2003_05;</doc:revision>
			</revhistory>
		</referenceinfo>
		<title>Indices <filename>index.mod.xsl</filename></title>
		<partintro>
			<para>The file <filename>index.mod.xsl</filename> contains the
			XSL template for <doc:db>index</doc:db>.</para>
			<!-- TODO: -->
			<para>Describe indexterm-range key.</para>
			<para>
				
				An <literal>indexterm-range</literal> XSLT key is defined for
				all <quote>startofrange</quote> <doc:db>indexterm</doc:db>
				elements, matching their <sgmltag
				class="attribute">id</sgmltag> attribute.
				
			</para>
		</partintro>
	</doc:reference>

	<!-- Our key for ranges -->
	<xsl:key name="indexterm-range" match="indexterm[@class='startofrange']" use="@id"/>

	<doc:template basename="index" xmlns="">
		<refpurpose>Process <doc:db>index</doc:db> and <doc:db>setindex</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Produce a chapter-level index in &LaTeX;.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>

			<para>

				You will need to run your typesetter at least twice, and
				possibly three times, to have the index generated normally (you
				will also need to run the <command>makeidx</command> command).

			</para>

			&essential_preamble;
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_book;
				&test_index;
				&test_draft;
			</simplelist>
		</doc:samples>
	</doc:template>
	<xsl:template match="index|setindex">
		<xsl:variable name="preamble" select="node()[not(self::indexinfo or self::setindexinfo or self::title or self::subtitle or self::titleabbrev or self::indexdiv or self::indexentry)]"/>
      <xsl:text>\setlength\saveparskip\parskip&#10;</xsl:text>
      <xsl:text>\setlength\saveparindent\parindent&#10;</xsl:text>
		<xsl:text>\begin{dbtolatexindex}{</xsl:text>
		<xsl:call-template name="generate.label.id"/>
		<xsl:text>}&#10;</xsl:text>
      <xsl:text>\setlength\tempparskip\parskip \setlength\tempparindent\parindent&#10;</xsl:text>
      <xsl:text>\parskip\saveparskip \parindent\saveparindent&#10;</xsl:text>
      <xsl:text>\noindent </xsl:text><!-- &#10; -->
		<xsl:apply-templates select="$preamble"/>
		<xsl:call-template name="map.begin"/>
      <xsl:text>\parskip\tempparskip&#10;</xsl:text>
      <xsl:text>\parindent\tempparindent&#10;</xsl:text>
		<xsl:text>\makeatletter\@input@{\jobname.ind}\makeatother&#10;</xsl:text>
		<xsl:call-template name="map.end"/>
		<xsl:text>\end{dbtolatexindex}&#10;</xsl:text>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose> Essential preamble for <filename>index.mod.xsl</filename> support </refpurpose>
		<doc:description>
			<para>

				This preamble aids the following:
				<itemizedlist>
					<listitem>
						<simpara>
							Allow user to override the &LaTeX; default index name
							with a &DocBook; localisation.
						</simpara>
						<simpara>
							Integrate <doc:db>index</doc:db>/@<sgmltag
							class="attribute">id</sgmltag> cross-references
							with &LaTeX; and tables of contents (makes indices
							behave a bit like chapters).
						</simpara>
						<simpara>
							Allow <quote>preamble</quote> templates or
							mappings to be applied for indices.
						</simpara>
					</listitem>
				</itemizedlist>

			</para>
		</doc:description>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara>&preamble;</simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template name="latex.preamble.essential.index">
		<xsl:text>
			<![CDATA[
% index labeling helper
\newif\ifdocbooktolatexprintindex\docbooktolatexprintindextrue
\let\dbtolatex@@theindex\theindex
\let\dbtolatex@@endtheindex\endtheindex
\def\theindex{\relax}
\def\endtheindex{\relax}
\newenvironment{dbtolatexindex}[1]
   {
\if@openright\cleardoublepage\else\clearpage\fi
\let\dbtolatex@@indexname\indexname
\def\dbtolatex@indexlabel{%
 \ifnum \c@secnumdepth >\m@ne \refstepcounter{chapter}\fi%
 \label{#1}\hypertarget{#1}{\dbtolatex@@indexname}%
 \global\docbooktolatexprintindexfalse}
\def\indexname{\ifdocbooktolatexprintindex\dbtolatex@indexlabel\else\dbtolatex@@indexname\fi}
\dbtolatex@@theindex
   }
   {
\dbtolatex@@endtheindex\let\indexname\dbtolatex@@indexname
   }

\newlength\saveparskip \newlength\saveparindent
\newlength\tempparskip \newlength\tempparindent
]]>
		</xsl:text>
	</xsl:template>

<!--
	<xsl:template match="index/title">
		<xsl:call-template name="label.id"> <xsl:with-param name="object" select=".."/> </xsl:call-template>
	</xsl:template>

<xsl:template match="indexdiv">
	<xsl:apply-templates/>
</xsl:template>

<xsl:template match="indexdiv/title">
	<xsl:call-template name="label.id"> <xsl:with-param name="object" select=".."/> </xsl:call-template>
</xsl:template>

	<xsl:template match="primary|secondary|tertiary|see|seealso"/>

-->

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>indexterm</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Identify an instance of an indexed term.
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.generate.indexterm"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:params>
			<variablelist>
				<varlistentry>
					<term>style</term>
					<listitem><simpara>
						&LaTeX; code used to format the displayed entry.
						By default, this is drawn from any <quote>latex-index-style</quote>
						processing instructions (and is therefore empty most of the time).
					</simpara></listitem>
				</varlistentry>
			</variablelist>
		</doc:params>
		<doc:notes>
			<para>
				A &LaTeX; <function condition="latex">index</function> command
				is issued. When an <doc:db>index</doc:db> element is included
				in your document and indexing is enabled, this indexterm will
				be indexed.
			</para>
			<para>
				When <link linkend="param.latex.is.draft">draft mode</link> is
				enabled, the physical location of <doc:db
				basename="indexterm">indexterms</doc:db> will be highlighted
				within the body of the text as well as appearing in the index
				proper.
			</para>
			<para>
				&DB2LaTeX; includes some logic to handle the
				<quote>startofrange</quote> and <quote>endofrange</quote>
				classes.
			</para>
			<para>
				It is possible to format an entry (e.g.
				make it bold or italic) by inserting a processing instruction
				named <quote>latex-index-style</quote> in the appropriate
				subterm.
			</para>
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_book;
				&test_ddh;
				&test_draft1;
				&test_draft2;
				&test_index;
			</simplelist>
		</doc:samples>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.is.draft"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<!-- INDEX TERM CONTENT MODEL
	IndexTerm ::=
	(Primary,
	((Secondary,
	((Tertiary,
	(See|SeeAlso+)?)|
	See|SeeAlso+)?)|
	See|SeeAlso+)?)
	-->
	<xsl:template match="indexterm">
		<xsl:if test="$latex.generate.indexterm='1'">
			<xsl:variable name="idxterm">
				<xsl:apply-templates mode="indexterm"/>
			</xsl:variable>

			<xsl:if test="@class and @zone">
				<xsl:message terminate="yes">Error: Only one attribute (@class or @zone) is in indexterm possible!</xsl:message>
			</xsl:if>

			<xsl:choose>
				<xsl:when test="@class='startofrange'">
					<xsl:text>\index{</xsl:text>
					<xsl:value-of select="$idxterm"/>
					<xsl:text>|(}</xsl:text>
				</xsl:when>
				<xsl:when test="@class='endofrange'">
					<xsl:choose>
						<xsl:when test="count(key('indexterm-range',@startref)) = 0">
							<xsl:message terminate="yes"><xsl:text>Error: No indexterm with </xsl:text>
							<xsl:text>id='</xsl:text><xsl:value-of select="@startref"/>
							<xsl:text>' found!</xsl:text>
							<xsl:text>  Check your attributs id/startref in your indexterms!</xsl:text>
							</xsl:message>
						</xsl:when>
						<xsl:otherwise>
							<xsl:variable name="thekey" select="key('indexterm-range',@startref)"/>
							<xsl:for-each select="$thekey[1]">
								<xsl:text>\index{</xsl:text>
								<xsl:apply-templates mode="indexterm"/>
								<xsl:text>|)}</xsl:text>
							</xsl:for-each>
						</xsl:otherwise>
					</xsl:choose>
				</xsl:when>
				<xsl:otherwise>
					<xsl:text>\index{</xsl:text>
					<xsl:value-of select="$idxterm"/>
					<xsl:text>}</xsl:text>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:if>
	</xsl:template>

	<xsl:template match="*" mode="indexterm">
		<xsl:message>WARNING: Element '<xsl:value-of select="local-name()"/>' in indexterm not supported and skipped!</xsl:message>
	</xsl:template>

	<!--
	<xsl:template match="acronym|foreignphrase" mode="indexterm">
		<xsl:apply-templates mode="indexterm"/>
	</xsl:template>
	-->

	<doc:template xmlns="">
		<refpurpose>Process the contents of <doc:db basename="indexterm">indexterms</doc:db></refpurpose>
		<doc:description>
			<para>
				Register a primary index term.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				Calls <xref linkend="template.index.subterm"/>.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="primary" mode="indexterm">
		<xsl:call-template name="index.subterm"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process the contents of <doc:db basename="indexterm">indexterms</doc:db></refpurpose>
		<doc:description>
			<para>
				Register a secondary or tertiary index term.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				Calls <xref linkend="template.index.subterm"/>.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="secondary|tertiary" mode="indexterm">
		<xsl:text>!</xsl:text>
		<xsl:call-template name="index.subterm"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process the contents of <doc:db basename="indexterm">indexterms</doc:db></refpurpose>
		<doc:description>
			<para>
				Register a primary, secondary or tertiary index term.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>

				If no <sgmltag class="attribute">sortas</sgmltag> attribute is
				present, the contents of <doc:db>primary</doc:db>,
				<doc:db>secondary</doc:db> and <doc:db>tertiary</doc:db>
				elements are converted to text-only and no templates are
				applied. If the <sgmltag class="attribute">sortas</sgmltag>
				attribute is present and non-empty, its value is used for indexing
				and sorting (but not for display)<!-- and templates
				<emphasis>are</emphasis> applied for display purposes-->.

			</para>
			<para>

				If a <quote>latex-index-style</quote> processing instruction is
				present, the displayed indexterm will be formatted by treating
				the content of the PI as a &LaTeX; command.

			</para>
		</doc:notes>
	</doc:template>
	<xsl:template name="index.subterm">
		<xsl:variable name="style" select="processing-instruction('latex-index-style')"/>
		<xsl:choose>
			<xsl:when test="@sortas!=''">
				<xsl:variable name="string">
					<xsl:call-template name="scape-indexterm">
						<xsl:with-param name="string" select="@sortas"/>
					</xsl:call-template>
				</xsl:variable>
				<xsl:variable name="content">
					<xsl:call-template name="scape-indexterm">
						<xsl:with-param name="string" select="."/>
					</xsl:call-template>
				</xsl:variable>
				<xsl:value-of select="normalize-space($string)"/>
				<xsl:text>@{</xsl:text>
				<xsl:value-of select="$style"/>
				<xsl:text>{</xsl:text>
				<xsl:value-of select="normalize-space($content)"/>
				<xsl:text>}}</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<xsl:variable name="string">
					<xsl:call-template name="scape-indexterm">
						<xsl:with-param name="string" select="."/>
					</xsl:call-template>
				</xsl:variable>
				<xsl:value-of select="normalize-space($string)"/>
				<xsl:if test="$style!=''">
					<xsl:text>@{</xsl:text>
					<xsl:value-of select="$style"/>
					<xsl:text>{</xsl:text>
					<xsl:value-of select="normalize-space($string)"/>
					<xsl:text>}}</xsl:text>
				</xsl:if>
			</xsl:otherwise>
		</xsl:choose>
		<!--
		<xsl:apply-templates mode="indexterm"/>
		-->
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process the contents of <doc:db>see</doc:db> and <doc:db>seealso</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Annotate an index entry with a <quote>See</quote> or <quote>See also</quote> cross-reference.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				Templates are applied.
				<quote>See</quote> or <quote>see also</quote> text
				is generated by <literal>gentext.element.name</literal>
				and formatted in italics.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="see|seealso" mode="indexterm">
		<xsl:text>|textit{</xsl:text>
		<xsl:call-template name="gentext.element.name"/>
		<xsl:text>} {</xsl:text>
		<xsl:apply-templates/>
		<!--
		<xsl:apply-templates mode="indexterm"/>
		-->
		<xsl:text>} </xsl:text>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose> Skip <doc:db>indexentry</doc:db>-related elements </refpurpose>
		<doc:description>
			<para>
				Ignores the elements.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				&DB2LaTeX; only supports indices that are generated by &LaTeX; itself.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="indexentry|primaryie|secondaryie|tertiaryie|seeie|seealsoie"/>

</xsl:stylesheet>
