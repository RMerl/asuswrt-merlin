<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################
|	$Id: param-switch.mod.xsl,v 1.17 2004/01/31 11:52:31 j-devenish Exp $
+ ############################################################################## -->

<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="param-switch" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: param-switch.mod.xsl,v 1.17 2004/01/31 11:52:31 j-devenish Exp $
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
				<doc:revision rcasver="1.1">&rev_2003_05;</doc:revision>
			</revhistory>
		</referenceinfo>
		<title>Parameters: &DB2LaTeX; Switches <filename>param-switch.mod.xsl</filename></title>
		<partintro>
			<para>
			
			The values of parameters in this file are used to influence
			the behaviour of &DB2LaTeX; templates through the selection
			of pre-defined options.
			All parameter names begin with <quote>latex.</quote>.
			
			</para>
			<note>
				<para>
				
				In some stylesheets, tests of parameter values use
				<quote>!=0</quote> logic. However, &DB2LaTeX; uses
				<quote>=1</quote> logic. This means that empty parameters are
				<quote>off</quote> in &DB2LaTeX; but <quote>on</quote> in those
				other stylesheets. The <ulink
				url="http://www.w3.org">XPath</ulink> values
				<quote>true()</quote> and <quote>false()</quote> work as
				expected.
				
				</para>
			</note>
		</partintro>
	</doc:reference>

	<doc:param xmlns="">
	<refpurpose> Control the display of <doc:db basename="caption">captions</doc:db> in lists of figures.  </refpurpose>
	<doc:description>
		<para>
		By default, lists of figures (see <xref linkend="template.lot"/>)
		will include both figure <doc:db basename="title">titles</doc:db> and mediaobject <doc:db basename="caption">captions</doc:db>.
		This is useful for readers, as they have a full description of each figure.
		However, it is impractical for large documents because the list
		of figures will occupy too many pages.
		</para>
		<para>
		This variable, when set to '1', authorises DB2LaTeX to include
		mediaobject captions in lists of figures. It is enabled by default.
		When set to '0', lists of figures will contain only the
		titles of figures.
		</para>
	</doc:description>
	</doc:param>
	<xsl:param name="latex.caption.lot.titles.only">1</xsl:param>

	<doc:param xmlns="">
	<refpurpose>
		Control the output of the \bibliography{.bib}.
	</refpurpose>
	<doc:description>
		<para>The value of this parameter is output.</para>
		<para>An example is <quote><filename>citations.bib</filename></quote>,
		if your BibTeX file has that name.</para>
	</doc:description>
	</doc:param>
	<xsl:param name="latex.bibfiles"></xsl:param>

    <doc:param xmlns="">
	<refpurpose> Controls the output of LaTeX packages and commands to support 
	    documents with math commands and environments..</refpurpose>
	<doc:description>
	    <para>If this parameter is set to 1, the stylesheets generate code to 
		<emphasis>DEFAULT: 1</emphasis> Only more code is generated. 
	    </para>
	</doc:description>
    </doc:param>
	<xsl:param name="latex.math.support">1</xsl:param>

    <doc:param xmlns="">
	<refpurpose> Controls  if the revision history table is generated as the first document 
	    table.
	</refpurpose>
	<doc:description>
	    <para>If this parameter is set to 1, the stylesheets generate code to 
		<emphasis>DEFAULT: 1</emphasis> Only more code is generated. 
	    </para>
	</doc:description>
    </doc:param>
	<xsl:param name="latex.output.revhistory">1</xsl:param>

    <doc:template name="latex.fancybox.options" xmlns="">
	<refpurpose> Options for fancybox </refpurpose>
	<doc:description>
			<!--
			<xsl:if test="@role">
				<xsl:choose>
					<xsl:when test="@role='small'">
						<xsl:text>,fontsize=\small</xsl:text>
					</xsl:when>
					<xsl:when test="@role='large'">
						<xsl:text>,fontsize=\large</xsl:text>
					</xsl:when>
				</xsl:choose>
			</xsl:if>
			-->
			<para>
				<doc:todo>Undocumented.</doc:todo>
			</para>
	</doc:description>
    </doc:template>
    <xsl:template name="latex.fancybox.options">
	</xsl:template>

    <doc:param xmlns="">
	<refpurpose> Controls the output of LaTeX commands to support the generation 
	    of PDF files.</refpurpose>
	<doc:description>
	    <para>If this parameter is set to 1, the stylesheets generate code to 
		detect if it is either <literal>latex</literal> or <literal>pdflatex</literal>
		the shell command that is being used to compile the LaTeX text file. Some
		packages (<literal>graphicx</literal>, <literal>hyperref</literal>) are used
		with the right parameters. Finally, the graphic extensions declared, to use in
		<literal>\includegraphics</literal> commands depends also on which command is
		being used. If this parameter is set to zero, such code is not generated (which 
		does not mean that the file cannot compile with pdflatex, but some strange issues 
		may appear). <emphasis>DEFAULT: 1</emphasis> Only more code is generated. 
	    </para>
	</doc:description>
    </doc:param>
	<xsl:param name="latex.pdf.support">1</xsl:param>

	<doc:param xmlns="">
		<refpurpose> Enable the generation of indexterms </refpurpose>
		<doc:description><para>Support index generation.</para></doc:description>
	</doc:param>
	<xsl:param name="latex.generate.indexterm">1</xsl:param>

	<doc:param xmlns="">
	<refpurpose> DB2LaTeX hyphenation linebreak tricks </refpurpose>
	<doc:description>
		<para>
		Usually, LaTeX does not perform hyphenation in <quote>teletype</quote> (monospace)
		text. This can lead to formatting problems. But certain monospace texts, such as
		URLs and filenames, have <quote>natural</quote> breakpoints such as full stops
		and slashes. DB2LaTeX's <quote>tttricks</quote> exploit a hyphenation trick in
		order to provide line wrapping in the middle of monospace text. Set this to '1'
		to enable these tricks (they are not enabled by default). See also the FAQ.
		</para>
	</doc:description>
	</doc:param>
	<xsl:param name="latex.hyphenation.tttricks">0</xsl:param>

	<doc:param xmlns="">
		<refpurpose> Decimal point for &LaTeX; tables </refpurpose>
		<doc:description>
			<para>
			
			This is a non-localisable character that may be used
			for decimal alignment of &LaTeX; tables.
			
			</para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.decimal.point"/>

	<doc:param xmlns="">
		<refpurpose>Toggle the trimming of leading and trailing whitespace in verbatim environments </refpurpose>
		<doc:description><para>
			In verbatim environments such as <doc:db>programlisting</doc:db> and <doc:db>screen</doc:db>,
			it can be useful to trim leading and trailing whitespace. However, this is not compliant
			with The Definitive Guide.
		</para></doc:description>
	</doc:param>
	<xsl:param name="latex.trim.verbatim">0</xsl:param>

	<doc:param xmlns="">
		<refpurpose>Toggle the use of the <productname>ltxtable</productname> LaTeX package</refpurpose>
		<doc:description>
		<note><para>
		This is not implemented as true ltxtable support, yet.
		It uses longtable until we can integrate proper ltxtable support.
		One the feature is supported, it should probably be enabled by
		default!
		</para></note>
		<para>If this package is used then tables will be have the capability
		to run over multiple pages when necessary.</para>
		<warning><para>
		Cells spanning multiple columns may require extra passes with LaTeX
		in order for column widths to 'converge'.
		</para></warning>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.use.ltxtable">0</xsl:param>

	<doc:param xmlns="">
		<refpurpose>Toggle the use of the <productname>longtable</productname> LaTeX package</refpurpose>
		<doc:description>
			<para>
				Enabling this option allows <doc:db
				basename="simplelist">simplelists</doc:db> to run over multiple
				pages. In the future, it will be superseded by <xref
				linkend="param.latex.use.ltxtable"/>.
			</para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.use.longtable">0</xsl:param>

	<doc:param xmlns="">
		<refpurpose>Toggle the use of the <productname>overpic</productname> LaTeX package</refpurpose>
		<doc:description><para>Facilitates overlays (for callouts).</para></doc:description>
	</doc:param>
	<xsl:param name="latex.use.overpic">1</xsl:param>

	<doc:param xmlns="">
		<refpurpose>Toggle the use of the <productname>umoline</productname> LaTeX package</refpurpose>
		<doc:description><para>Provide underlining.</para></doc:description>
	</doc:param>
	<xsl:param name="latex.use.umoline">0</xsl:param>

	<doc:param xmlns="">
		<refpurpose>Toggle the use of the <productname>url</productname> LaTeX package</refpurpose>
		<doc:description><para>Provide partial support for hyperlinks.</para></doc:description>
	</doc:param>
	<xsl:param name="latex.use.url">1</xsl:param>

	<doc:param xmlns="">
		<refpurpose>Toggle the use of the the <quote>draft</quote> preamble</refpurpose>
		<doc:description><para>&DB2LaTeX; provides a number of draft-mode features to aid
		the refinement of documents. Normally, this mode is enabled or disabled according
		to the <sgmltag class='attribute'>status</sgmltag> attribute of the top-level
		<doc:db>book</doc:db> or <doc:db>article</doc:db> element. However, this variable
		will take precedence when it is not empty. It is empty by default.</para>
		</doc:description>
		<doc:samples>
			<simplelist type='inline'>
				&test_index_draft;
				&test_draft;
			</simplelist>
		</doc:samples>
	</doc:param>
	<xsl:param name="latex.is.draft"/>

	<doc:param xmlns="">
		<refpurpose>Toggle the use of the <productname>varioref</productname> LaTeX package</refpurpose>
		<doc:description><para><productname>varioref</productname> seemed like a good idea at first,
		but we not realise it does understand &DocBook; gentext localisations. By default, it is enabled
		when <xref linkend="param.insert.xref.page.number"/> is enabled.</para></doc:description>
	</doc:param>
	<xsl:param name="latex.use.varioref">
		<xsl:if test="$insert.xref.page.number='1'">1</xsl:if>
	</xsl:param>

	<doc:param xmlns="">
		<refpurpose>Toggle the use of the <productname>fancyhdr</productname> LaTeX package</refpurpose>
		<doc:description><para>Provides page headers and footers. Disabling support for
		this package will make headers and footer go away.</para></doc:description>
	</doc:param>
	<xsl:param name="latex.use.fancyhdr">1</xsl:param>

	<doc:param xmlns="">
		<refpurpose> Control the inclusion of chapter titles in <doc:db basename="lot">lots</doc:db> </refpurpose>
		<doc:description>
			<para>
				When this variable is set, lists of tables and lists of figures
				will be grouped and labeled by chapter.
			</para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.bridgehead.in.lot">1</xsl:param>

	<doc:param xmlns="">
		<refpurpose> Configure the application of truncation partitions </refpurpose>
		<doc:description>
			<para>

				For <xref linkend="param.latex.fancyhdr.truncation.partition"/>, the
				partition can be modulated in a left-right fashion or an
				inside-outside fashion. Use <quote>lr</quote> for left-right,
				all other values are inside-outside (<quote>io</quote> is
				suggested). The default is <quote>io</quote>, because this
				matches <xref linkend="param.latex.documentclass.book"/>.

			</para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.fancyhdr.truncation.style">io</xsl:param>

	<doc:param xmlns="">
		<refpurpose> Configure the width of header portions on each page </refpurpose>
		<doc:description>
			<para>

				It is possible that the titles of chapter or sections will
				occupy more than the width of a single line. When this variable
				is empty, headers will be allowed to occupy multiple lines.
				However, it is possible that the left-hand portion of a header
				will collide with the right-hand portion of header. To prevent
				this, headers can be truncated if they exceed an allowable
				width. When this variables is set to a number from zero to 100,
				the left-hand (or inside) side of each header will have that
				width reserved. The right-hand (or outside) side will have the
				remainder. A common option is to set this variable to zero and
				set <xref linkend="param.latex.fancyhdr.truncation.style"/> to
				<quote>io</quote>.

			</para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.fancyhdr.truncation.partition">50</xsl:param>

	<doc:param xmlns="">
		<refpurpose> Section/chapter style for fancy headers </refpurpose>
		<doc:description>
			<para>

				&DB2LaTeX; comes with some pre-configured styles for page
				headers. These include <quote></quote>, the default, which is
				determined by the <productname>fancyhdr</productname> package.
				Another option is <quote>natural</quote>, in which both the
				chapter and section are shown on each page with their numbers.
				An alternative is to provide your own <xref
				linkend="template.generate.latex.pagestyle"/> template with a
				value such as <literal>\pagestyle{headings}</literal> (the
				<quote>headings</quote> page style, which is not a
				<productname>fancyhdr</productname> style, shows page numbers
				in the headers whereas the others show page numbers in the
				footers).

			</para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.fancyhdr.style"/>

	<doc:param xmlns="">
		<refpurpose>Toggle the use of the <productname>parskip</productname> &latex; package</refpurpose>
		<doc:description>
			<para>Use <quote>block</quote> paragraph style instead of indentation.</para>
		</doc:description>
		<doc:notes>
			<para><productname>parskip</productname> introduces vertical whitespace between
			paragraphs and list items. However, &db2latex;'s <doc:db>toc</doc:db> and
			<doc:db>lot</doc:db> templates attempt to suppress this whitespace.</para>
			<para>When this option is off, you may wish to investigate <xref linkend="param.latex.use.noindent"/>.</para>
		</doc:notes>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="template.para"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:param>
	<xsl:param name="latex.use.parskip">0</xsl:param>

	<doc:param xmlns="">
		<refpurpose>Toggle the use of <function condition="latex">noindent</function> commands</refpurpose>
		<doc:description>
			<para>
			
			When this parameter is 1, &DB2LaTeX; will deliberately insert
			noindents at particular locations within your document.

			</para>
		</doc:description>
		<doc:notes>
			<para>

			When traditional &LaTeX; paragraph indentation and spacing is used,
			it is often necessary to use <function
			condition="latex">noindent</function> after certain block-formatted
			elements (e.g. <doc:db>itemizedlist</doc:db>). Let us know if we
			need to insert more <function
			condition="latex">noindent</function>---so far, there are few
			places where we make use of it.

			</para>
			<para>
				
			By default, this option will be turned on when <xref
			linkend="param.latex.use.parskip"/> is <emphasis>off</emphasis> and
			will be turned off when <xref linkend="param.latex.use.parskip"/>
			is <emphasis>on</emphasis>.
				
			</para>
		</doc:notes>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.use.parskip"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:param>
	<xsl:param name="latex.use.noindent">
		<xsl:choose>
			<xsl:when test="$latex.use.parskip=1">
				<xsl:value-of select="0"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="1"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:param>

	<doc:param xmlns="">
		<refpurpose>Toggle the use of the <productname>subfigure</productname> LaTeX package</refpurpose>
		<doc:description><para>Used to provide nice layout of multiple mediaobjects in figures.</para></doc:description>
	</doc:param>
	<xsl:param name="latex.use.subfigure">1</xsl:param>

	<doc:param xmlns="">
		<refpurpose>Toggle the use of the <productname>rotating</productname> LaTeX package</refpurpose>
		<doc:description><para>Undocumented.</para></doc:description>
	</doc:param>
	<xsl:param name="latex.use.rotating">1</xsl:param>

	<doc:param xmlns="">
		<refpurpose>Toggle the use of the <productname>tabularx</productname> LaTeX package</refpurpose>
		<doc:description><para>Used to provide certain table features. Has some incompatabilities
		with packages, but also solves some conflicts that the regular tabular
		environment has.</para></doc:description>
	</doc:param>
	<xsl:param name="latex.use.tabularx">1</xsl:param>

	<doc:param xmlns="">
		<refpurpose>Toggle the use of the <productname>dcolumn</productname> LaTeX package</refpurpose>
		<doc:description>
		<warning><para>
			Currently, <productname>dcolumn</productname> support does not function
			correctly.
		</para></warning>
		<para>
			<productname>dcolumn</productname> provides support for the <literal>char</literal>
			alignment of table cells.
		</para>
		</doc:description>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.decimal.point"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:param>
	<xsl:param name="latex.use.dcolumn">0</xsl:param>

	<doc:param xmlns="">
		<refpurpose>Toggle the use of the <productname>hyperref</productname> LaTeX package</refpurpose>
		<doc:description><para>This is used extensively for hyperlinking within documents.</para></doc:description>
	</doc:param>
	<xsl:param name="latex.use.hyperref">1</xsl:param>

	<doc:param xmlns="">
		<refpurpose>Toggle the use of the <productname>fancybox</productname> LaTeX package</refpurpose>
		<doc:description><para>This is essential for admonitions.</para></doc:description>
	</doc:param>
	<xsl:param name="latex.use.fancybox">1</xsl:param>

	<doc:param xmlns="">
		<refpurpose>Toggle the use of the <productname>fancyvrb</productname> LaTeX package</refpurpose>
		<doc:description><para>Provides support for tabbed whitespace in verbatim environments.
		See also <xref linkend="param.latex.fancyvrb.tabsize"/>.</para></doc:description>
	</doc:param>
	<xsl:param name="latex.use.fancyvrb">1</xsl:param>

	<doc:param xmlns="">
		<refpurpose>Toggle the use of the <productname>isolatin1</productname> LaTeX package</refpurpose>
		<doc:description>
			<warning><para>
			This option is deprecated. See <xref linkend="param.latex.inputenc"/>.
			</para></warning>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.use.isolatin1">0</xsl:param>

	<doc:param xmlns="">
		<refpurpose> Choose whether to use the <productname>unicode</productname> LaTeX package</refpurpose>
		<doc:description><para>See the <productname>unicode</productname> documentation for details.</para></doc:description>
	</doc:param>
	<xsl:param name="latex.use.ucs">0</xsl:param>

	<doc:param xmlns="">
	<refpurpose> Control which references are cited in the bibliography </refpurpose>
	<doc:description>
		<para>
		The DB2LaTeX generated bibliography (bibitems) may either
		include all biblioentries found in the document, or only thee ones explicitly
		cited with <sgmltag class="element">citation</sgmltag>.
		</para>
	    <para>Two values are possible: <quote>all</quote> or <quote>cited</quote>.</para>
	</doc:description>
	</doc:param>
	<xsl:param name="latex.biblio.output">all</xsl:param>

	<doc:param xmlns="">
	<refpurpose> Control bibliographic citation style </refpurpose>
	<doc:description>
		<para>By default, this value is empty. Alternatively, a special value
		is recognised: <quote>ieee</quote> (or <quote>IEEE</quote>).</para>
	</doc:description>
	</doc:param>
	<xsl:param name="latex.biblioentry.style"/>

	<doc:param xmlns="">
	<refpurpose> Improved typesetting of captions  </refpurpose>
	<doc:description>
		<para>
		DB2LaTeX supports <link linkend="param.formal.title.placement">$formal.title.placement</link>
		as a mechanism for choosing whether captions will appear above or below the objects they describe.
		<!--
		($formal.title.placement is described in the <ulink
		url="http://docbook.sourceforge.net/release/xsl/current/doc/html/formal.title.placement.html">DocBook
		XSL Stylesheet HTML Parameter Reference</ulink>.)
		-->
		However, LaTeX will often produce an ugly result when captions occur
		above their corresponding content. This usually arises because of
		unsuitable \abovecaptionskip and \belowcaptionskip.
		</para>
		<para>
		This variable, when set to '1', authorises DB2LaTeX to swap the caption
		'skip' lengths when a caption is placed <emphasis>above</emphasis> its
		corresponding content. This is enabled by default.
		</para>
	</doc:description>
	</doc:param>
	<xsl:param name="latex.caption.swapskip">1</xsl:param>

	<doc:param xmlns="">
	<refpurpose> Control <sgmltag class="element">imagedata</sgmltag> selection. </refpurpose>
	<doc:description>
		<para>This controls how DB2LaTeX behaves when a <sgmltag class="element">mediaobject</sgmltag> contains
		multiple <sgmltag class="element">imagedata</sgmltag>. When non-empty, DB2LaTeX will exclude
		imagedata that have a format no listed within this variable.</para>
	</doc:description>
	</doc:param>
	<xsl:param name="latex.graphics.formats"></xsl:param>

	<doc:param xmlns="">
	<refpurpose> Control Unicode character handling. </refpurpose>
	<doc:description>
		<para>
		Normally, XSLT processors will convert SGML character entities into
		Unicode characters and DB2LaTeX doesn't have much chance to do anything
		toward converting them to LaTeX equivalents. We do not yet know how we
		can solve this problem best.
		</para>
		<para>
		Proposed values: 'catcode', 'unicode', 'extension'.
		Currently only 'catcode' is supported. All other values will
		cause no special handling except for certain mappings in MathML.
		In future, perhaps the 'unicode' LaTeX package could be of assistance.
		'Extension' could be an XSLT extension that handles the characters
		using a mapping table.
		</para>
	</doc:description>
	</doc:param>
	<xsl:param name="latex.entities"></xsl:param>

	<doc:param xmlns="">
	<refpurpose> Control the use of <sgmltag class="attribute">otherterm</sgmltag> attributes </refpurpose>
		<doc:description>
			<para>
				When a <doc:db>glosssee</doc:db> or <doc:db>glossseealso</doc:db> element contains
				both an <quote>otherterm</quote> attribute <emphasis>and</emphasis> content templates,
				this variable elects which will be the source of the displayed text. By default,
				this variable is enabled and a cross-reference to the otherwterm will be
				generated (i.e. content templates will be ignored).
			</para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.otherterm.is.preferred">1</xsl:param>

	<doc:param xmlns="">
	<refpurpose> Control the use of <sgmltag class="element">alt</sgmltag> text </refpurpose>
	<doc:description>
		<para>
		By default, DB2LaTeX assumes that <sgmltag class="element">alt</sgmltag>
		text should be typeset in preference to any 
		<sgmltag class="element">mediaobject</sgmltag>s.
		</para>
	</doc:description>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.tex.math.in.alt"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:param>
	<xsl:param name="latex.alt.is.preferred">1</xsl:param>

	<doc:param xmlns="">
	<refpurpose> Whether to apply templates for component titles. </refpurpose>
	<doc:description>
		<para>
		Controls whether component titles will be generated by
		applying templates or by conversion to string values.
		When enabled, templates will be applied. This enables template
		expression in titles but may have problematic side-effects such
		as nested links.
		</para>
		<note>
			<para>
				This variable does not influence all <sgmltag class="element">title</sgmltag>
				elements. Some may have their own configuration variables or be non-configurable.
			</para>
		</note>
	</doc:description>
	</doc:param>
	<xsl:param name="latex.apply.title.templates">1</xsl:param>

	<doc:param xmlns="">
	<refpurpose> Whether to apply templates for admonition titles. </refpurpose>
	<doc:description>
		<para>
		Controls whether admonition titles will be generated by
		applying templates or by conversion to string values.
		When enabled, templates will be applied.
		</para>
	</doc:description>
	</doc:param>
	<xsl:param name="latex.apply.title.templates.admonitions">1</xsl:param>

	<doc:param xmlns="">
	<refpurpose> Whether to delimit URLs with quotation characters </refpurpose>
	<doc:description>
		<para>
		When this option is enabled, gentext quotation characters
		(urlstartquote and urlendquote) are used to delimit the
		URLs when they are displayed as part of <doc:db>ulink</doc:db>
		formatting. The delimiters do not form part of the URL or
		hyperlink per se.
		</para>
	</doc:description>
	</doc:param>
	<xsl:param name="latex.url.quotation">1</xsl:param>

	<doc:param xmlns="">
		<refpurpose> Control string comparison for <doc:db basename="ulink">ulinks</doc:db> </refpurpose>
		<doc:description>
			<para>
			
			The formatting of a <doc:db>ulink</doc:db> element varies according
			to whether its <sgmltag class="attribute">url</sgmltag> attribute
			differs from its content. When this option is enabled, the
			comparison between these two values ignores the
			<quote>protocol</quote> portion of the URL (that which occurs
			before ':' or '://', as a concession to HTTP URLs). For example,
			when this option is enabled, <quote>a.b.c/d</quote> would be
			considered equivalent to <quote>http://a.b.c/d</quote> and
			<quote>file:a.b.c/d</quote>.
			
			</para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.ulink.protocols.relaxed">
      <xsl:choose>
         <xsl:when test="$ulink.protocols.relaxed!=''">
            <xsl:message>Warning: $ulink.protocols.relaxed was a misnomer: use $latex.ulink.protocols.relaxed instead</xsl:message>
            <xsl:value-of select="$ulink.protocols.relaxed"/>
         </xsl:when>
			<xsl:otherwise>
            <xsl:value-of select="0"/>
			</xsl:otherwise>
      </xsl:choose>
   </xsl:param>
   <xsl:param name="ulink.protocols.relaxed"/>

	<doc:param xmlns="">
		<refpurpose> Control the suppression of headers/footers on blank pages in double-side documents </refpurpose>
		<doc:description>
			<para>
			
			When this option is enabled, &DB2LaTeX; will attempt to
			suppress headers and footers on pages that contain no
			other content (i.e. left-handed pages in a double-sided
			document). When this option is disabled, &DB2LaTeX; does
			not interfere with the default appearance of headers and
			footers.
			
			</para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.suppress.blank.page.headers">1</xsl:param>

</xsl:stylesheet>
