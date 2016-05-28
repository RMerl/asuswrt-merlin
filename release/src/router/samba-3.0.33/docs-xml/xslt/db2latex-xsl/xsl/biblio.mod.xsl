<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: biblio.mod.xsl,v 1.21 2004/01/26 08:57:46 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
+ ############################################################################## -->

<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

    <doc:reference id="biblio" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: biblio.mod.xsl,v 1.21 2004/01/26 08:57:46 j-devenish Exp $
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
				<doc:revision rcasver="1.16">&rev_2003_05;</doc:revision>
			</revhistory>
		</referenceinfo>
		<title>Bibliographies <filename>biblio.mod.xsl</filename></title>
		<partintro>

			<para>The file <filename>biblio.mod.xsl</filename> contains the XSL
			template for <doc:db>bibliography</doc:db> and associated
			elements.</para>

			<!--
		<para>This reference describes the templates and parameters relevant
		    to formatting DocBook bibliography markup.</para>
			-->

			<bridgehead><quote>All</quote> versus <quote>cited</quote> mode</bridgehead>
				<note>
					<para>These modes are superseded by &BibTeX;
					when using &BibTeX;.</para>
				</note>
				<para>

				The <xref linkend="param.latex.biblio.output"/> option can be used
				to select <quote>all</quote> mode or <quote>cited</quote> mode.
				With the <quote>cited</quote> mode, only the <doc:db
				basename="biblioentry">biblioentries</doc:db> that have been
				cited somewhere in the document are output. Otherwise (in
				<quote>all</quote> mode) all biblioentries found are output (as
				the HTML stylesheets do).

				</para>

			<bridgehead>&DocBook; versus &BibTeX;</bridgehead>
				<para>
					
					&DB2LaTeX; supports &BibTeX;. When this mode is enabled,
					you can use a &BibTeX; citations file. Your <doc:db
					basename="citation">citations</doc:db> can then refer to
					your &BibTeX; keys (<doc:db basename="xref">xrefs</doc:db>
					are unlikely to work, though). You will need to provide a
					&LaTeX; command to select a bibliographic citation style as
					is normal for &BibTeX; (see <xref
					linkend="bibtex.example"/>). You must also run the
					<command>bibtex</command> command when typesetting your
					document with &LaTeX;.
					
				</para>
				<note>
					<para>Although &DocBook; allows <doc:db basename="bibliography">bibliographies</doc:db>
					within a number of components, such as <doc:db>section</doc:db>, the use of &BibTeX;
					is only useful for a single, chapter-level bibliography.</para>
				</note>
				<para>

					To enable &BibTeX; mode, insert an empty
					<doc:db>bibliography</doc:db> element in your &DocBook;
					document. However, note that this is not valid &DocBook;,
					though it will lead to the desired results with most
					&DocBook; XSL stylesheets (including those for HTML).
					You will also need to set the <xref linkend="param.latex.bibfiles"/>
					variable to the correct path of your &BibTeX; citations file.

					Alternatively, you may choose to use a processing
					instruction named <quote>bibtex-bibliography</quote>
					instead of an empty element. The name of the citations file
					may be specified within the processing instruction or via
					<xref linkend="param.latex.bibfiles"/> (see
					<xref linkend="bibtex.example"/>).

				</para>
				<example id="bibtex.example">
					<title>Using &BibTeX; with &DB2LaTeX;</title>
					<para>

						A &BibTeX; bibliography may be enabled by providing the
						name of your citations file, the name of a &BibTeX; style,
						and the insertion of an appropriate node in your
						&DocBook; document. In your customisation layer:

					<programlisting><![CDATA[
<xsl:variable name="latex.book.preamble.post">
% Your LaTeX customisation commands
\bibliographystyle{ieeetr}
</xsl:variable>
<xsl:variable name="latex.bibfiles" select="'../citations.bib'"/>
]]></programlisting>

						Then, in your document, type this:

					<programlisting><![CDATA[
<bibliography/>
]]></programlisting>

						Although this is not valid according to the &DocBook;
						DTD, it will work with most stylesheets (not just
						&DB2LaTeX;). Alternatively, you may instead use a
						processing instruction in compliance with the DTD,
						though this will work only with &DB2LaTeX; and you will
						not be able to specify a custom <doc:db>title</doc:db>
						for your bibliography:

					<programlisting><![CDATA[
<?bibtex-bibliography?>
]]></programlisting>

						You may optionally specify the citations file directly
						(you will not need to set
						<literal>latex.bibfiles</literal>):

					<programlisting><![CDATA[
<?bibtex-bibliography ../citations.bib?>
]]></programlisting>

					</para>
				</example>

			<doc:variables>
				<itemizedlist>
					<listitem><simpara><xref linkend="param.latex.bibfiles"/></simpara></listitem>
					<listitem><simpara><xref linkend="param.latex.biblio.output"/></simpara></listitem>
				</itemizedlist>
			</doc:variables>
		</partintro>
	</doc:reference>

	<doc:template xmlns="">
		<refpurpose> Essential preamble for <filename>biblio.mod.xsl</filename> support </refpurpose>
		<doc:description>
			<para>

				Defines <function condition="latex">docbooktolatexbibname</function>,
				<function condition="latex">docbooktolatexbibaux</function>
				and <function condition="env">docbooktolatexbibliography</function>.

			</para>
		</doc:description>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara>&preamble;</simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>

	<xsl:template name="latex.preamble.essential.biblio">
		<xsl:text>
			<![CDATA[
\AtBeginDocument{\ifx\refname\@undefined\let\docbooktolatexbibname\bibname\def\docbooktolatexbibnamex{\bibname}\else\let\docbooktolatexbibname\refname\def\docbooktolatexbibnamex{\refname}\fi}
% Facilitate use of \cite with \label
\newcommand{\docbooktolatexbibaux}[2]{%
  \protected@write\@auxout{}{\string\global\string\@namedef{docbooktolatexcite@#1}{#2}}
}
% Provide support for bibliography `subsection' environments with titles
\newenvironment{docbooktolatexbibliography}[3]{
   \begingroup
   \let\save@@chapter\chapter
   \let\save@@section\section
   \let\save@@@mkboth\@mkboth
   \let\save@@bibname\bibname
   \let\save@@refname\refname
   \let\@mkboth\@gobbletwo
   \def\@tempa{#3}
   \def\@tempb{}
   \ifx\@tempa\@tempb
      \let\chapter\@gobbletwo
      \let\section\@gobbletwo
      \let\bibname\relax
   \else
      \let\chapter#2
      \let\section#2
      \let\bibname\@tempa
   \fi
   \let\refname\bibname
   \begin{thebibliography}{#1}
}{
   \end{thebibliography}
   \let\chapter\save@@chapter
   \let\section\save@@section
   \let\@mkboth\save@@@mkboth
   \let\bibname\save@@bibname
   \let\refname\save@@refname
   \endgroup
}
]]>
		</xsl:text>
	</xsl:template>

	<doc:template basename="bibliography" xmlns="">
		<refpurpose>Process <doc:db>bibliography</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				This is a complex template that can format bibliographies as
				chapter-level or section-level components.
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.biblio.output"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.bibwidelabel"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:params>
			<variablelist>
				<varlistentry>
					<term>makechapter</term>
					<listitem>
						<para>

							Whether to make a chapter-level bibliography or
							not. This is normally determined by examining
							whether the <doc:db>bibliography</doc:db> element
							occurs as a child of a <doc:db>book</doc:db> or
							<doc:db>part</doc:db>. If so, the bibliography will
							be formatted as an unnumbered chapter. If not, the
							bibliography will be formatted as an unnumbered
							section. This effect does not hold for &BibTeX;
							bibliographies, which will be formatted by the
							&LaTeX; <function
							condition="latex">bibliography</function> command.

						</para>
					</listitem>
				</varlistentry>
			</variablelist>
		</doc:params>
		<doc:notes>
			<para>This template probably contains many bugs.</para>
			&essential_preamble;
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_article;
				&test_biblio;
				&test_bind;
				&test_book;
				&test_cited;
				&test_ieeebiblio;
			</simplelist>
		</doc:samples>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara>&mapping;: this template will use the <literal>bibliography-chapter</literal>
				and <literal>bibliography-section</literal> mappings.</simpara></listitem>
				<listitem><simpara><xref linkend="citation"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="bibliography">
		<xsl:param name="makechapter" select="local-name(..)='book' or local-name(..)='part'"/>
		<xsl:variable name="keyword">
			<xsl:choose>
				<xsl:when test="$makechapter">
					<xsl:text>bibliography-chapter</xsl:text>
				</xsl:when>
				<xsl:otherwise>
					<xsl:text>bibliography-section</xsl:text>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:variable>
		<xsl:variable name="environment">
			<xsl:choose>
				<xsl:when test="$makechapter">thebibliography</xsl:when>
				<xsl:otherwise>docbooktolatexbibliography</xsl:otherwise>
			</xsl:choose>
		</xsl:variable>
		<xsl:variable name="title">
			<xsl:apply-templates select="title|subtitle"/>
		</xsl:variable>
		<!--
		<xsl:message>DB2LaTeX: Processing BIBLIOGRAPHY</xsl:message>
		-->
		<xsl:if test="$title!=''">
			<xsl:text>\let\oldbibname\bibname&#10;</xsl:text>
			<xsl:text>\let\oldrefname\refname&#10;</xsl:text>
			<xsl:text>\def\bibname{</xsl:text>
			<xsl:value-of select="$title"/>
			<xsl:text>}&#10;</xsl:text>
			<xsl:text>\let\refname\bibname&#10;</xsl:text>
		</xsl:if>
		<xsl:call-template name="map.begin">
			<xsl:with-param name="keyword" select="$keyword"/>
		</xsl:call-template>
		<xsl:choose>
			<xsl:when test="biblioentry or bibliodiv">
				<xsl:variable name="separatetitle" select="not(biblioentry or bibliodiv[1]/@title)"/>
				<xsl:message>DB2LaTeX: Bibliographic Output Mode :  <xsl:value-of select="$latex.biblio.output"/></xsl:message>
				<xsl:choose>
					<xsl:when test="$separatetitle and $makechapter">
						<xsl:text>\chapter*{\docbooktolatexbibnamex}\hypertarget{</xsl:text>
						<xsl:call-template name="generate.label.id"/>
						<xsl:text>}{}&#10;</xsl:text>
					</xsl:when>
					<xsl:when test="$separatetitle and not($makechapter)">
						<xsl:text>\section*{\docbooktolatexbibnamex}\hypertarget{</xsl:text>
						<xsl:call-template name="generate.label.id"/>
						<xsl:text>}{}&#10;</xsl:text>
					</xsl:when>
					<xsl:when test="biblioentry"><!-- implies not($separatetitle) -->
						<xsl:text>\begin{</xsl:text>
						<xsl:value-of select="$environment"/>
						<xsl:text>}{</xsl:text>
						<xsl:value-of select="$latex.bibwidelabel"/>
						<xsl:if test="$environment='docbooktolatexbibliography'">
							<xsl:text>}{\</xsl:text>
							<!-- TODO choose the correct nesting, rather than assuming something -->
							<xsl:choose>
								<xsl:when test="$makechapter">chapter</xsl:when>
								<xsl:otherwise>section</xsl:otherwise>
							</xsl:choose>
							<xsl:text>}{</xsl:text>
							<xsl:choose>
								<xsl:when test="$title!=''">
									<xsl:value-of select="$title"/>
								</xsl:when>
								<xsl:otherwise>
									<xsl:text>\docbooktolatexbibname</xsl:text>
								</xsl:otherwise>
							</xsl:choose>
						</xsl:if>
						<xsl:text>}\hypertarget{</xsl:text>
						<xsl:call-template name="generate.label.id"/>
						<xsl:text>}{}&#10;</xsl:text>
						<xsl:choose>
							<xsl:when test="$latex.biblio.output ='cited'">
								<xsl:apply-templates select="biblioentry" mode="bibliography.cited">
									<xsl:sort select="./abbrev"/>
									<xsl:sort select="./@xreflabel"/>
									<xsl:sort select="./@id"/>
								</xsl:apply-templates>
							</xsl:when>
							<xsl:when test="$latex.biblio.output ='all'">
								<xsl:apply-templates select="biblioentry" mode="bibliography.all">
									<xsl:sort select="./abbrev"/>
									<xsl:sort select="./@xreflabel"/>
									<xsl:sort select="./@id"/>
								</xsl:apply-templates>
							</xsl:when>
							<xsl:otherwise>
								<xsl:apply-templates select="biblioentry">
									<xsl:sort select="./abbrev"/>
									<xsl:sort select="./@xreflabel"/>
									<xsl:sort select="./@id"/>
								</xsl:apply-templates>
							</xsl:otherwise>
						</xsl:choose>
						<!-- <xsl:apply-templates select="child::*[name(.)!='biblioentry']"/>  -->
						<xsl:text>&#10;\end{</xsl:text>
						<xsl:value-of select="$environment"/>
						<xsl:text>}&#10;</xsl:text>
					</xsl:when>
					<xsl:otherwise>
						<xsl:text>\hypertarget{</xsl:text>
						<xsl:call-template name="generate.label.id"/>
						<xsl:text>}{}&#10;</xsl:text>
					</xsl:otherwise>
				</xsl:choose>
				<xsl:apply-templates select="bibliodiv"/>
			</xsl:when>
			<xsl:when test="child::*">
				<xsl:choose>
					<xsl:when test="$makechapter">
						<xsl:text>\chapter*</xsl:text>
					</xsl:when>
					<xsl:otherwise>
						<xsl:text>\section*</xsl:text>
					</xsl:otherwise>
				</xsl:choose>
				<xsl:text>{\docbooktolatexbibnamex}\hypertarget{</xsl:text>
				<xsl:call-template name="generate.label.id"/>
				<xsl:text>}{}&#10;</xsl:text>
				<xsl:call-template name="content-templates"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text>% Assume that an empty &lt;bibliography&gt; means ``use BibTeX'' or similar.&#10;</xsl:text>
				<xsl:text>\bibliography{</xsl:text><xsl:value-of select="$latex.bibfiles"/><xsl:text>}&#10;</xsl:text>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:call-template name="map.end">
			<xsl:with-param name="keyword" select="$keyword"/>
		</xsl:call-template>
		<xsl:if test="$title!=''">
			<xsl:text>\let\bibname\oldbibname&#10;</xsl:text>
			<xsl:text>\let\refname\oldrefname&#10;</xsl:text>
		</xsl:if>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <sgmltag class="xmlpi">bibtex-bibliography</sgmltag> nodes</refpurpose>
		<doc:description>
			<para>
				Output a &BibTeX; bibliography.
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.bibfiles"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:params>
			<variablelist>
				<varlistentry>
					<term>makechapter</term>
					<listitem>
						<para>

							Choose which &LaTeX; mapping to use. This is
							normally determined by examining whether the PI
							occurs as a child of a <doc:db>book</doc:db> or
							<doc:db>part</doc:db>. Regardless of this
							parameter, the bibliography will be formatted as a
							chapter via the &LaTeX; <function
							condition="latex">bibliography</function> command.

						</para>
					</listitem>
				</varlistentry>
				<varlistentry>
					<term>filename</term>
					<listitem>
						<para>

							The filename of the &BibTeX; citations source file.
							By default, this will be obtained from the content
							of the process instruction, if present, or
							otherwise from <xref
							linkend="param.latex.bibfiles"/>.

						</para>
					</listitem>
				</varlistentry>
			</variablelist>
		</doc:params>
		<doc:notes>
			<para>This PI is not part of &DocBook; and is only supported by &DB2LaTeX;
			The formatting of the bibliography is performed by &LaTeX; and is not
			configurable by &DB2LaTeX;.</para>
		</doc:notes>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara>&mapping;: This template will use the <literal>bibliography-chapter</literal>
				or <literal>bibliography-section</literal> mapping.</simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="processing-instruction('bibtex-bibliography')">
		<xsl:param name="makechapter" select="local-name(..)='book' or local-name(..)='part'"/>
		<xsl:param name="filename">
			<xsl:choose>
				<xsl:when test="normalize-space(.)!=''">
					<xsl:value-of select="."/>
				</xsl:when>
				<xsl:otherwise>
					<xsl:value-of select="$latex.bibfiles"/>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:param>
		<xsl:variable name="keyword">
			<xsl:choose>
				<xsl:when test="$makechapter">
					<xsl:text>bibliography-chapter</xsl:text>
				</xsl:when>
				<xsl:otherwise>
					<xsl:text>bibliography-section</xsl:text>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:variable>
		<xsl:call-template name="map.begin">
			<xsl:with-param name="keyword" select="$keyword"/>
		</xsl:call-template>
		<xsl:text>\bibliography{</xsl:text><xsl:value-of select="$filename"/><xsl:text>}&#10;</xsl:text>
		<xsl:call-template name="map.end">
			<xsl:with-param name="keyword" select="$keyword"/>
		</xsl:call-template>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>bibliodiv</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Formats subdivisions of bibliographies.
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.biblio.output"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:notes>
			&essential_preamble;
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_article;
				&test_biblio;
				&test_bind;
				&test_book;
				&test_cited;
				&test_ieeebiblio;
			</simplelist>
		</doc:samples>
	</doc:template>
	<xsl:template match="bibliodiv">
		<xsl:param name="environment">
			<xsl:variable name="parent" select="local-name(..)"/>
			<xsl:choose>
				<xsl:when test="starts-with($parent,'sect')">docbooktolatexbibliography</xsl:when>
				<xsl:otherwise>thebibliography</xsl:otherwise>
			</xsl:choose>
		</xsl:param>
		<!--
		<xsl:message>DB2LaTeX: Processing BIBLIOGRAPHY - BIBLIODIV</xsl:message>
		-->
		<xsl:text>&#10;\begin{docbooktolatexbibliography}{</xsl:text>
		<xsl:value-of select="$latex.bibwidelabel"/>
		<xsl:text>}{\</xsl:text>
		<!-- TODO choose the correct nesting, rather than assuming subsection -->
		<xsl:text>subsection</xsl:text>
		<xsl:text>}{</xsl:text>
		<xsl:apply-templates select="title|subtitle"/>
		<xsl:text>}\hypertarget{</xsl:text>
		<xsl:call-template name="generate.label.id"/>
		<xsl:text>}{}&#10;</xsl:text>
		<xsl:choose>
			<xsl:when test="$latex.biblio.output ='cited'">
				<xsl:apply-templates select="biblioentry" mode="bibliography.cited">
					<xsl:sort select="./abbrev"/>
					<xsl:sort select="./@xreflabel"/>
					<xsl:sort select="./@id"/>
				</xsl:apply-templates>
			</xsl:when>
			<xsl:when test="$latex.biblio.output ='all'">
				<xsl:apply-templates select="biblioentry">
					<xsl:sort select="./abbrev"/>
					<xsl:sort select="./@xreflabel"/>
					<xsl:sort select="./@id"/>
				</xsl:apply-templates>
			</xsl:when>
		</xsl:choose>
		<xsl:text>&#10;\end{docbooktolatexbibliography}&#10;</xsl:text>
	</xsl:template>

	<doc:template basename="biblioentry" xmlns="">
		<refpurpose>Process <doc:db>biblioentry</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Emits a bibiographic entry if the reference was cited in the document.
			</para>
		</doc:description>
		<doc:seealso>
			<para>This template calls <xref linkend="template.biblioentry.output"/> to format the entry.</para>
		</doc:seealso>
	</doc:template>
	<xsl:template match="biblioentry" mode="bibliography.cited">
		<xsl:param name="bibid" select="@id"/>
		<xsl:param name="ab" select="abbrev"/>
		<xsl:variable name="nx" select="//xref[@linkend=$bibid]"/>
		<xsl:variable name="nc" select="//citation[text()=$ab]"/>
		<xsl:if test="count($nx) &gt; 0 or count($nc) &gt; 0">
			<xsl:call-template name="biblioentry.output"/>
		</xsl:if>
	</xsl:template>

	<doc:template basename="biblioentry" xmlns="">
		<refpurpose>Process <doc:db>biblioentry</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Emits a bibiographic entry unconditionally.
			</para>
		</doc:description>
		<doc:seealso>
			<para>This template calls <xref linkend="template.biblioentry.output"/> to format the entry.</para>
		</doc:seealso>
	</doc:template>
	<xsl:template match="biblioentry" mode="bibliography.all">
		<xsl:call-template name="biblioentry.output"/>
	</xsl:template>

	<doc:template basename="biblioentry" xmlns="">
		<refpurpose>Process <doc:db>biblioentry</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Emits a bibiographic entry unconditionally.
			</para>
		</doc:description>
		<doc:seealso>
			<para>This template calls <xref linkend="template.biblioentry.output"/> to format the entry.</para>
		</doc:seealso>
	</doc:template>
	<xsl:template match="biblioentry">
		<xsl:call-template name="biblioentry.output"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Format <doc:db basename="biblioentry">biblioentries</doc:db></refpurpose>
		<doc:description>
			<para>
				Formats a bibiographic entry.
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.biblioentry.style"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:notes>
			<note>
				<para>
					This template does not apply to &BibTeX; bibliographies.
				</para>
			</note>
			<para>
				A <function condition="latex">bibitem</function> is emitted.
				If can be linked via <doc:db>citation</doc:db> or <doc:db>xref</doc:db>.
				The <doc:db>title</doc:db> will be formatted in italics, followed by
				<xref linkend="param.biblioentry.item.separator"/>, the <doc:db>author</doc:db>
				or <doc:db>authorgroup</doc:db>, and then the following elements separated
				by <literal>biblioentry.item.separator</literal>:
				<doc:db>copyright</doc:db>, <doc:db>publisher</doc:db>,
				<doc:db>pubdate</doc:db>, <doc:db>pagenums</doc:db>,
				<doc:db>isbn</doc:db>, <doc:db>editor</doc:db>,
				<doc:db>releaseinfo</doc:db>.
			</para>
			<note>
				<para>
					All templates for all &DocBook; elements will be applied
					with the <quote>bibliography.mode</quote> XSLT mode.
				</para>
			</note>
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_article;
				&test_biblio;
				&test_bind;
				&test_book;
				&test_cited;
				&test_ieeebiblio;
			</simplelist>
		</doc:samples>
	</doc:template>
	<xsl:template name="biblioentry.output">
		<xsl:variable name="biblioentry.label">
			<xsl:choose>
				<xsl:when test="@xreflabel">
					<xsl:value-of select="normalize-space(@xreflabel)"/> 
				</xsl:when>
				<xsl:when test="abbrev">
					<xsl:apply-templates select="abbrev" mode="bibliography.mode"/> 
				</xsl:when>
				<xsl:when test="@id">
					<xsl:value-of select="normalize-space(@id)"/> 
				</xsl:when>
				<xsl:otherwise>
					<!-- TODO is there any need for a warning? -->
				</xsl:otherwise>
			</xsl:choose>
		</xsl:variable>
		<xsl:variable name="biblioentry.id">
			<xsl:choose>
				<xsl:when test="abbrev">
					<xsl:apply-templates select="abbrev" mode="bibliography.mode"/>
				</xsl:when>
				<xsl:otherwise>
					<xsl:call-template name="generate.label.id"/>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:variable>
		<xsl:text>&#10;</xsl:text>
		<xsl:call-template name="biblioentry.output.format">
			<xsl:with-param name="biblioentry.label" select="$biblioentry.label"/>
			<xsl:with-param name="biblioentry.id" select="$biblioentry.id"/>
		</xsl:call-template>
	</xsl:template>

	<xsl:template name="biblioentry.output.format">
		<xsl:param name="biblioentry.label"/>
		<xsl:param name="biblioentry.id"/>
		<xsl:choose>
			<xsl:when test="$latex.biblioentry.style='ieee' or $latex.biblioentry.style='IEEE'">
				<xsl:text>% -------------- biblioentry &#10;</xsl:text>
				<xsl:text>\bibitem</xsl:text>
				<xsl:text>{</xsl:text>
				<xsl:value-of select="$biblioentry.id"/>
				<xsl:text>}\docbooktolatexbibaux{</xsl:text>
				<xsl:call-template name="generate.label.id"/>
				<xsl:text>}{</xsl:text>
				<xsl:value-of select="$biblioentry.id"/>
				<xsl:text>}&#10;\hypertarget{</xsl:text>
				<xsl:call-template name="generate.label.id"/>
				<xsl:text>}&#10;</xsl:text>
				<xsl:apply-templates select="author|authorgroup" mode="bibliography.mode"/>
				<xsl:value-of select="$biblioentry.item.separator"/>
				<xsl:text>\emph{</xsl:text> <xsl:apply-templates select="title" mode="bibliography.mode"/><xsl:text>}</xsl:text>
				<xsl:for-each select="child::copyright|child::publisher|child::pubdate|child::pagenums|child::isbn">
					<xsl:value-of select="$biblioentry.item.separator"/>
					<xsl:apply-templates select="." mode="bibliography.mode"/>
				</xsl:for-each>
				<xsl:text>. </xsl:text>
				<xsl:text>&#10;&#10;</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text>% -------------- biblioentry &#10;</xsl:text>
				<xsl:choose>
					<xsl:when test="$biblioentry.label=''">
						<xsl:text>\bibitem</xsl:text> 
					</xsl:when>
					<xsl:otherwise>
						<xsl:text>\bibitem[{</xsl:text>
						<xsl:call-template name="normalize-scape">
							<xsl:with-param name="string" select="$biblioentry.label"/>
						</xsl:call-template>
						<xsl:text>}]</xsl:text>
					</xsl:otherwise>
				</xsl:choose>
				<xsl:text>{</xsl:text>
				<xsl:value-of select="$biblioentry.id"/>
				<xsl:text>}\docbooktolatexbibaux{</xsl:text>
				<xsl:call-template name="generate.label.id"/>
				<xsl:text>}{</xsl:text>
				<xsl:value-of select="$biblioentry.id"/>
				<xsl:text>}&#10;\hypertarget{</xsl:text>
				<xsl:call-template name="generate.label.id"/>
				<xsl:text>}{\emph{</xsl:text> <xsl:apply-templates select="title" mode="bibliography.mode"/> <xsl:text>}}</xsl:text>
				<xsl:value-of select="$biblioentry.item.separator"/>
				<xsl:apply-templates select="author|authorgroup" mode="bibliography.mode"/>
				<xsl:for-each select="child::copyright|child::publisher|child::pubdate|child::pagenums|child::isbn|child::editor|child::releaseinfo">
					<xsl:value-of select="$biblioentry.item.separator"/>
					<xsl:apply-templates select="." mode="bibliography.mode"/>
				</xsl:for-each>
				<xsl:text>.</xsl:text>
				<xsl:text>&#10;&#10;</xsl:text>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

	<xsl:template name="biblioentry.output.format.ieee">
	</xsl:template>

    <xsl:template match="abbrev" mode="bibliography.mode">
		<xsl:apply-templates mode="bibliography.mode"/>
    </xsl:template>

	<!--
	<doc:template basename="abstract" match="abstract" mode="bibliography.mode" xmlns="">
		<refpurpose>Process <doc:db>abstract</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Currently, <doc:db basename="abstract">abstracts</doc:db> are deleted
				in <literal>bibliography.mode</literal>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>Abstracts are suppressed in &DB2LaTeX; bibliographies.</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="abstract" mode="bibliography.mode"/>
	-->

	<xsl:template match="address" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="affiliation" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="authorblurb" mode="bibliography.mode"/>

	<xsl:template match="artheader" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="artpagenums" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="author" mode="bibliography.mode">
		<xsl:apply-templates select="."/>
	</xsl:template>

	<xsl:template match="authorgroup" mode="bibliography.mode">
		<xsl:apply-templates select="."/>
	</xsl:template>

	<!-- basename="authorinitials" -->
	<xsl:template match="authorinitials" mode="bibliography.mode">
		<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="bibliomisc" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="bibliomset" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="bibliomixed" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="biblioset" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="biblioset/title|biblioset/citetitle" mode="bibliography.mode">
	<xsl:variable name="relation" select="../@relation"/>
	<xsl:choose>
		<xsl:when test="$relation='article'">
		<xsl:call-template name="gentext.startquote"/>
		<xsl:apply-templates/>
		<xsl:call-template name="gentext.endquote"/>
		</xsl:when>
		<xsl:otherwise>
		<xsl:apply-templates/>
		</xsl:otherwise>
	</xsl:choose>
	</xsl:template>

	<xsl:template match="bookbiblio" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="citetitle" mode="bibliography.mode">
	<I><xsl:apply-templates mode="bibliography.mode"/></I>
	</xsl:template>

	<xsl:template match="collab" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="collabname" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="confgroup" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="confdates" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="conftitle" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="confnum" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="confsponsor" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="contractnum" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="contractsponsor" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="contrib" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="copyright" mode="bibliography.mode">
	<xsl:call-template name="gentext.element.name"/>
	<xsl:call-template name="gentext.space"/>
	<xsl:call-template name="dingbat">
		<xsl:with-param name="dingbat">copyright</xsl:with-param>
	</xsl:call-template>
	<xsl:call-template name="gentext.space"/>
	<xsl:apply-templates select="year" mode="bibliography.mode"/>
	<xsl:call-template name="gentext.space"/>
	<xsl:apply-templates select="holder" mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="year" mode="bibliography.mode">
	<xsl:apply-templates/><xsl:text>, </xsl:text>
	</xsl:template>

	<xsl:template match="year[position()=last()]" mode="bibliography.mode">
	<xsl:apply-templates/>
	</xsl:template>

	<xsl:template match="holder" mode="bibliography.mode">
	<xsl:apply-templates/>
	</xsl:template>

	<xsl:template match="corpauthor" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="corpname" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="date" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="edition" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="editor" mode="bibliography.mode">
	<xsl:call-template name="person.name"/>
	</xsl:template>

	<xsl:template match="firstname" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="honorific" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="indexterm" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="invpartnumber" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="isbn" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="issn" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="issuenum" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="jobtitle" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="lineage" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="orgname" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="orgdiv" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="othercredit" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="othername" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="pagenums" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="printhistory" mode="bibliography.mode">
	<!-- suppressed -->
	</xsl:template>

	<xsl:template match="productname" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="productnumber" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="pubdate" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="publisher" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="publishername" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="pubsnumber" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="releaseinfo" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="revhistory" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="seriesinfo" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="seriesvolnums" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="shortaffil" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="subtitle" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="surname" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="title" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="titleabbrev" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="volumenum" mode="bibliography.mode">
	<xsl:apply-templates mode="bibliography.mode"/>
	</xsl:template>

	<xsl:template match="*" mode="bibliography.mode">
	<xsl:apply-templates select="."/>
	</xsl:template>

</xsl:stylesheet>
