<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################
|	$Id: param-direct.mod.xsl,v 1.7 2004/01/31 11:05:05 j-devenish Exp $
+ ############################################################################## -->

<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="param-direct" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: param-direct.mod.xsl,v 1.7 2004/01/31 11:05:05 j-devenish Exp $
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
		<title>Parameters: &LaTeX;-direct Strings <filename>param-direct.mod.xsl</filename></title>
		<partintro>
			<para>
			
			The values of the parameters in this file will be passed
			directly to &LaTeX;.
			All parameter names begin with <quote>latex.</quote>.
			
			</para>
		</partintro>
	</doc:reference>

	<doc:param xmlns="">
		<refpurpose> DB2LaTeX document class </refpurpose>
		<doc:description>
			<para>
			This variable is normally empty and the stylesheets will determine
			the correct document class according to whether the document is a
			book or an article. If you wish to use your own document class,
			put its non-empty value in this variable. It will apply for both
			books and articles.
			</para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.documentclass"></xsl:param>

	<doc:param xmlns="">
		<refpurpose> The <literal>\maketitle</literal> for books and articles. </refpurpose>
		<doc:description>
			<para>Some users may wish to override or eliminate <literal>\maketitle</literal>.</para>
			<note><para>Does not apply to <sgmltag class="element">article</sgmltag>s within <sgmltag class="element">book</sgmltag>s.</para></note>
			<para>By default, uses LaTeX <literal>\maketitle</literal> with the 'empty' pagestyle
			for the first page. The page style of subsequent pages is determined by
			<xref linkend="template.generate.latex.pagestyle"/>.</para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.maketitle">
		<xsl:text>{\maketitle</xsl:text>
		<xsl:call-template name="generate.latex.pagestyle"/>
		<xsl:text>\thispagestyle{empty}}&#10;</xsl:text>
	</xsl:param>

	<doc:param xmlns="">
		<refpurpose> Undocumented </refpurpose>
		<doc:description>
			<para> Undocumented. </para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.article.preamble.pre">
	</xsl:param>

	<doc:param xmlns="">
		<refpurpose> Undocumented </refpurpose>
		<doc:description>
			<para> Undocumented. </para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.article.preamble.post">
	</xsl:param>

	<doc:param xmlns="">
		<refpurpose> Controls what is output after the LaTeX preamble. </refpurpose>
		<doc:description>
			<para>Default values decrease edge margins and allow a large quantity of figures to be set on each page. </para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.article.varsets">
		<xsl:text><![CDATA[
\usepackage{anysize}
\marginsize{2cm}{2cm}{2cm}{2cm}
\renewcommand\floatpagefraction{.9}
\renewcommand\topfraction{.9}
\renewcommand\bottomfraction{.9}
\renewcommand\textfraction{.1}
]]>
		</xsl:text>
	</xsl:param>

	<doc:param xmlns="">
		<refpurpose> Undocumented </refpurpose>
		<doc:description>
			<para> Undocumented. </para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.book.preamble.pre">
	</xsl:param>

	<doc:param xmlns="">
		<refpurpose> Undocumented </refpurpose>
		<doc:description>
			<para> Undocumented. </para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.book.preamble.post">
	</xsl:param>

	<doc:param xmlns="">
		<refpurpose>
			All purpose commands to change text width, height, counters, etc.
			Defaults to a two-sided margin layout.
		</refpurpose>
		<doc:description>
			<para> Undocumented. </para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.book.varsets">
		<xsl:text>\usepackage{anysize}&#10;</xsl:text>
		<xsl:text>\marginsize{3cm}{2cm}{1.25cm}{1.25cm}&#10;</xsl:text>
	</xsl:param>

	<doc:param xmlns="">
		<refpurpose> 
			Begin document command
		</refpurpose>
		<doc:description>
			<para> Undocumented. </para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.book.begindocument">
		<xsl:text>\begin{document}&#10;</xsl:text>
	</xsl:param>

	<doc:param xmlns="">
		<refpurpose>
			LaTeX code that is output after the author (e.g. 
			<literal>\makeindex, \makeglossary</literal>
		</refpurpose>
		<doc:description>
			<para> Undocumented. </para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.book.afterauthor">
		<xsl:text>% --------------------------------------------&#10;</xsl:text>
		<xsl:text>\makeindex&#10;</xsl:text>
		<xsl:text>\makeglossary&#10;</xsl:text>
		<xsl:text>% --------------------------------------------&#10;</xsl:text>
	</xsl:param>

	<doc:template xmlns="">
		<refpurpose> Format the output of tabular headings. </refpurpose>
		<doc:description>
			<para> Undocumented. </para>
		</doc:description>
	</doc:template>
	<xsl:template name="latex.thead.row.entry">
		<xsl:apply-templates/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose> Format the output of tabular footers. </refpurpose>
		<doc:description>
			<para> Undocumented. </para>
		</doc:description>
	</doc:template>
	<xsl:template name="latex.tfoot.row.entry">
		<xsl:apply-templates/>
	</xsl:template>

	<doc:param xmlns="">
		<refpurpose> Control the style of article titles </refpurpose>
		<doc:description>
			<para>This is passed directly to &LaTeX;. Typically,
			it is either a \command or an empty string.</para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.article.title.style">\textbf</xsl:param>

	<doc:param xmlns="">
		<refpurpose> Control the style of article titles within books </refpurpose>
		<doc:description>
			<para>This is passed directly to &LaTeX;. Typically,
			it is either a \command or an empty string.</para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.book.article.title.style">\Large\textbf</xsl:param>

	<doc:param xmlns="">
		<refpurpose> Control the style of authors and dates within a book's articles </refpurpose>
		<doc:description>
			<para>This is passed directly to &LaTeX;. Typically,
			it is either a \command or an empty string.</para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.book.article.header.style">\textsf</xsl:param>

	<doc:param xmlns="">
		<refpurpose> Control the style of equation captions  </refpurpose>
		<doc:description>
			<para>Figure captions are typeset in the default typeface (usually 'roman') but if
			this option is set to a LaTeX command, such as <literal>\textit</literal>, then
			that command will precede the caption and it will be typeset accordingly.</para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.equation.caption.style"></xsl:param>

	<doc:param xmlns="">
		<refpurpose> Control the style of example captions  </refpurpose>
		<doc:description>
			<para>Figure captions are typeset in the default typeface (usually 'roman') but if
			this option is set to a LaTeX command, such as <literal>\textit</literal>, then
			that command will precede the caption and it will be typeset accordingly.</para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.example.caption.style"></xsl:param>

	<doc:param xmlns="">
		<refpurpose> Control the style of figure captions  </refpurpose>
		<doc:description>
			<para>Figure captions are typeset in the default typeface (usually 'roman') but if
			this option is set to a LaTeX command, such as <literal>\textit</literal>, then
			that command will precede the caption and it will be typeset accordingly.</para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.figure.caption.style"></xsl:param>

	<doc:param xmlns="">
		<refpurpose> Control the style of figure titles  </refpurpose>
		<doc:description>
			<para>Figure titles are typeset in the default typeface (usually 'roman') but if
			this option is set to a LaTeX command, such as <literal>\textit</literal>, then
			that command will precede the title and it will be typeset accordingly.</para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.figure.title.style"></xsl:param>

	<doc:param xmlns="">
		<refpurpose> Control the style of equation captions  </refpurpose>
		<doc:description>
			<para>The titles of <doc:db basename="formalpara">formalparas</doc:db> are typeset in the bold typeface by default.
			This parameter can be set to an alternative &latex; command, such as <function condition="latex">textit</function> (or empty).</para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.formalpara.title.style">\textbf</xsl:param>

	<doc:param xmlns="">
		<refpurpose> Control the style of list titles  </refpurpose>
		<doc:description>
			<para>List titles are typeset in small caps but if
			this option is set to a LaTeX command, such as <literal>\textit</literal>, then
			that command will precede the title and it will be typeset accordingly.</para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.list.title.style">\sc</xsl:param>

	<doc:param xmlns="">
		<refpurpose> Control the style of procedure titles  </refpurpose>
		<doc:description>
			<para>Procedure titles are typeset in small caps but if
			this option is set to a LaTeX command, such as <literal>\textit</literal>, then
			that command will precede the title and it will be typeset accordingly.</para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.procedure.title.style">\sc</xsl:param>

	<doc:param xmlns="">
		<refpurpose> Control the style of segtitles  </refpurpose>
		<doc:description>
			<para>This is passed directly to &LaTeX;. Typically,
			it is either a \command or an empty string.</para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.segtitle.style">\em</xsl:param>

	<doc:param xmlns="">
		<refpurpose> Control the style of step titles  </refpurpose>
		<doc:description>
			<para>Step titles are typeset in small caps but if
			this option is set to a LaTeX command, such as <literal>\textit</literal>, then
			that command will precede the title and it will be typeset accordingly.</para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.step.title.style">\bf</xsl:param>

	<doc:param xmlns="">
		<refpurpose> Control the style of table captions  </refpurpose>
		<doc:description>
			<para>Figure captions are typeset in the default typeface (usually 'roman') but if
			this option is set to a LaTeX command, such as <literal>\textit</literal>, then
			that command will precede the caption and it will be typeset accordingly.</para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.table.caption.style"></xsl:param>

	<xsl:param name="latex.fancyhdr.lh">Left Header</xsl:param>
	<xsl:param name="latex.fancyhdr.ch">Center Header</xsl:param>
	<xsl:param name="latex.fancyhdr.rh">Right Header</xsl:param>
	<xsl:param name="latex.fancyhdr.lf">Left Footer</xsl:param>
	<xsl:param name="latex.fancyhdr.cf">Center Footer</xsl:param>
	<xsl:param name="latex.fancyhdr.rf">Right Footer</xsl:param>

	<doc:param xmlns="">
	<refpurpose> Override DB2LaTeX's choice of LaTeX page numbering style </refpurpose>
		<doc:description>
			<para>By default, DB2LaTeX will choose the 'plain' or 'fancy' page styles,
			depending on <xref linkend="param.latex.use.fancyhdr"/>. If non-empty, this
			variable overrides the automatic selection. An example would be the literal
			string 'empty', to eliminate headers and page numbers.</para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.pagestyle"/>

	<doc:param xmlns="">
	<refpurpose> DB2LaTeX hyperref options</refpurpose>
	<doc:description>
		<para>
		In addition to this variable, you can specify additional options using
		<literal>latex.hyperref.param.pdftex</literal> or <literal>latex.hyperref.param.dvips</literal>.
		</para>
	</doc:description>
	</doc:param>
	<xsl:param name="latex.hyperref.param.common">bookmarksnumbered,colorlinks,backref,bookmarks,breaklinks,linktocpage,plainpages=false</xsl:param>

	<doc:param xmlns="">
	<refpurpose> DB2LaTeX hyperref options for pdfTeX output</refpurpose>
	<doc:description>
		<para>
		See the hyperref documentation for further information.
		</para>
	</doc:description>
	</doc:param>
	<xsl:param name="latex.hyperref.param.pdftex">pdfstartview=FitH</xsl:param>
	<!--
	what is the unicode option?
	-->

	<doc:param xmlns="">
	<refpurpose> DB2LaTeX hyperref options for dvips output</refpurpose>
	<doc:description>
		<para>
		See the hyperref documentation for further information.
		</para>
	</doc:description>
	</doc:param>
	<xsl:param name="latex.hyperref.param.dvips"></xsl:param>

	<doc:param xmlns="">
		<refpurpose>Options for the <productname>varioref</productname> LaTeX package</refpurpose>
		<doc:description><para>Support index generation.</para></doc:description>
	</doc:param>
	<xsl:param name="latex.varioref.options">
		<xsl:if test="$latex.language.option!='none'">
			<xsl:value-of select="$latex.language.option" />
		</xsl:if>
	</xsl:param>

	<doc:template name="latex.vpageref.options" xmlns="">
		<refpurpose>Toggle the use of the <productname>varioref</productname> LaTeX package</refpurpose>
		<doc:description><para>Support index generation.</para></doc:description>
	</doc:template>
	<xsl:template name="latex.vpageref.options">on this page</xsl:template>

	<doc:param xmlns="">
		<refpurpose>Choose indentation for tabs in verbatim environments</refpurpose>
		<doc:description><para>When <xref linkend="param.latex.use.fancyvrb"/> is enabled,
		this variable sets the width of a tab in terms of an equivalent number of spaces.</para></doc:description>
	</doc:param>
	<xsl:param name="latex.fancyvrb.tabsize">3</xsl:param>

	<doc:template name="latex.fancyvrb.options" xmlns="">
		<refpurpose>Insert <productname>LaTeX</productname> options for <productname>fancyvrb</productname> Verbatim environments</refpurpose>
		<doc:description>
			<para> Undocumented. </para>
		</doc:description>
	</doc:template>
	<xsl:template name="latex.fancyvrb.options"/>

	<doc:param xmlns="">
		<refpurpose>Control the use of the <productname>inputenc</productname> LaTeX package</refpurpose>
		<doc:description>
			<para>
			If this option is non-empty, the <productname>inputenc</productname> package
			will be used with the specified encoding. This should agree with the your driver
			file. For example, the default value of <literal>latin1</literal>
			is compatible with <filename>docbook.xsl</filename>, which contains
			<literal><![CDATA[<xsl:output method="text" encoding="ISO-8859-1" indent="yes"/>]]></literal>
			</para>
			<para>
			If this option is empty, the <productname>inputenc</productname> package
			will not be invoked by <productname>DB2LaTeX</productname>.
			</para>
			<segmentedlist>
				<title>Common Combinations</title>
				<segtitle>Output Encoding</segtitle><segtitle><productname>inputenc</productname> Option</segtitle>
				<seglistitem><seg>ISO-8859-1</seg><seg>latin1</seg></seglistitem>
				<seglistitem><seg>UTF-8</seg><seg>utf8<footnote><simpara>When used in conjunction with a package such as <ulink url="http://www.ctan.org/tools/cataloguesearch?action=/search/&amp;catstring=unicode"><productname>unicode</productname></ulink>.</simpara></footnote></seg></seglistitem>
			</segmentedlist>
			<para>
			<productname>inputenc</productname> is a <productname>LaTeX</productname> base package.
			</para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.inputenc">latin1</xsl:param>

	<doc:param xmlns="">
		<refpurpose> Options for the <productname>fontenc</productname> package </refpurpose>
		<doc:description>
			<para> Undocumented. </para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.fontenc"></xsl:param>

	<doc:param xmlns="">
		<refpurpose>Select the optional parameter(s) for the <productname>unicode</productname> LaTeX package</refpurpose>
		<doc:description><para>See the <productname>unicode</productname> documentation for details.</para></doc:description>
	</doc:param>
	<xsl:param name="latex.ucs.options"></xsl:param>

	<doc:param xmlns="">
		<refpurpose>Select the optional parameter for the <productname>babel</productname> LaTeX package</refpurpose>
		<doc:description>
			<para>See the <productname>babel</productname> documentation for details.</para>
			<para>Although DB2LaTeX will try to choose the correct babel options for your
			document, you may need to specify the correct choice here. The special value
			of 'none' (without the quotes) will cause DB2LaTeX to skip babel configuration.</para>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.babel.language"></xsl:param>

	<doc:param xmlns="">
	<refpurpose> Adjust bibliography formatting </refpurpose>
	<doc:description>
		<para>The environment bibliography accepts a parameter that indicates
		the widest label, which is used to correctly format the bibliography
		output. The value of this parameter is output inside the
		<literal>\begin{thebibliography[]}</literal> LaTeX command.</para>
	</doc:description>
	</doc:param>
	<xsl:param name="latex.bibwidelabel">
		<xsl:choose>
			<xsl:when test="$latex.biblioentry.style='ieee' or $latex.biblioentry.style='IEEE'">
				<xsl:text>123</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text>WIDELABEL</xsl:text>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:param>

	<doc:param xmlns="">
	<refpurpose> DB2LaTeX document class options  </refpurpose>
	<doc:description>
		<para>
		These are the first options to be passed to <literal>\documentclass</literal>
		(The common options are
		<!--
		set to <literal>french,english</literal>
		-->
		blank
		by default.)
		They will be augmented or superseded by article/book options (see $latex.documentclass.article and $latex.documentclass.book) and pdftex/dvips options (see $latex.documentclass.pdftex and $latex.documentclass.dvips).
		</para>
	</doc:description>
	</doc:param>
	<xsl:param name="latex.documentclass.common"></xsl:param>

	<doc:param xmlns="">
	<refpurpose> DB2LaTeX document class options for articles</refpurpose>
	<doc:description>
		<para>
		The article options are set to <literal>a4paper,10pt,twoside,twocolumn</literal> by default.
		These are the intermediate options to be passed to <literal>\documentclass</literal>,
		between the common options and the pdftex/dvips options.
		</para>
	</doc:description>
	</doc:param>
	<xsl:param name="latex.documentclass.article">a4paper,10pt,twoside,twocolumn</xsl:param>

	<doc:param xmlns="">
	<refpurpose> DB2LaTeX document class options for books</refpurpose>
	<doc:description>
		<para>
		The book options are set to <literal>a4paper,10pt,twoside,openright</literal> by default.
		These are the intermediate options to be passed to <literal>\documentclass</literal>,
		between the common options and the pdftex/dvips options.
		</para>
	</doc:description>
	</doc:param>
	<xsl:param name="latex.documentclass.book">a4paper,10pt,twoside,openright</xsl:param>

	<doc:param xmlns="">
	<refpurpose> DB2LaTeX document class options for pdfTeX output</refpurpose>
	<doc:description>
		<para>
		The pdfTeX options are empty by default.
		These are the last options to be passed to <literal>\documentclass</literal>
		and override the common/article/book options.
		</para>
	</doc:description>
	</doc:param>
	<xsl:param name="latex.documentclass.pdftex"></xsl:param>

	<doc:param xmlns="">
	<refpurpose> DB2LaTeX document class options for dvips output</refpurpose>
	<doc:description>
		<para>
		The dvips options are empty by default.
		These are the last options to be passed to <literal>\documentclass</literal>
		and override the common/article/book options.
		</para>
	</doc:description>
	</doc:param>
	<xsl:param name="latex.documentclass.dvips"></xsl:param>

	<doc:param xmlns="">
	<refpurpose> DB2LaTeX graphics admonitions size</refpurpose>
	<doc:description>
		<para>
			Is passed as an optional parameter for <literal>\includegraphics</literal> and
			can take on any such legal values (or be empty).
		</para>
	</doc:description>
	</doc:param>
	<xsl:param name="latex.admonition.imagesize">width=1cm</xsl:param>

	<doc:param xmlns="">
	<refpurpose> DB2LaTeX allows using an (externally generated) cover page  </refpurpose>
	<doc:description>
		<para>
		You may supply a LaTeX file that will supersede DB2LaTeX's default
		cover page or title. If the value of this variable is non-empty, the
		generated LaTeX code includes \input{filename}. Otherwise, it uses the
		\maketitle command.
		</para>
		<warning><para>
			Bear in mind that using an external cover page breaks the
			"encapsulation" of DocBook. Further revisions of these stylesheets
			will add chunking support, and the automation of the cover file
			generation.
		</para></warning>
	</doc:description>
	</doc:param>
	<xsl:param name="latex.titlepage.file">title</xsl:param>
    <doc:param xmlns="">
	<refpurpose> Document Font(s)  </refpurpose>
	<doc:description>
			<para>
				Document fonts can be chosen by providing &LaTeX; package names.
				Common values include <literal>default</literal>, <literal>times</literal>, <literal>palatcm</literal>, <literal>charter</literal>, <literal>helvet</literal>, <literal>palatino</literal>, <literal>avant</literal>, <literal>newcent</literal> and <literal>bookman</literal>.
				Particular combinations may also work. For example, <literal>mathptm,charter,courier</literal>.
			</para>
			<!--
			If you want to change explicitly to a certain font, use the command \fontfamily{XYZ}\selectfont whereby XYZ can be set to: pag for Adobe AvantGarde, pbk for Adobe Bookman, pcr for Adobe Courier, phv for Adobe Helvetica, pnc for Adobe NewCenturySchoolbook, ppl for Adobe Palatino, ptm for Adobe Times Roman, pzc for Adobe ZapfChancery
			-->
	</doc:description>
    </doc:param>
    <xsl:param name="latex.document.font">palatino</xsl:param>

	<doc:param xmlns="">
	<refpurpose> Override DB2LaTeX's preamble with a custom preamble. </refpurpose>
	<doc:description>
		<para>
		When this variable is set, the entire DB2LaTeX premable will be superseded.
		<emphasis>You should not normally need or want to use this.</emphasis>
		It may cause LaTeX typesetting problems. This is a last resort or
		<quote>expert</quote> feature.
		</para>
	</doc:description>
	</doc:param>
	<xsl:param name="latex.override"></xsl:param>

</xsl:stylesheet>
