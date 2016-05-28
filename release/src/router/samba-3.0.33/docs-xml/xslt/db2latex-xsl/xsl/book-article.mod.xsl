<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: book-article.mod.xsl,v 1.41 2004/01/31 11:05:54 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
+ ############################################################################## -->

<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="book-article" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: book-article.mod.xsl,v 1.41 2004/01/31 11:05:54 j-devenish Exp $
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
				<doc:revision rcasver="1.28">&rev_2003_05;</doc:revision>
			</revhistory>
		</referenceinfo>
		<title>Books and Articles <filename>book-article.mod.xsl</filename></title>
		<partintro>

			<para>

			Most &DocBook; documents are either <doc:db
			basename="article">articles</doc:db> or <doc:db
			basename="book">books</doc:db>, so this XSL template file is a
			classical entry point when processing &DocBook; documents.

			</para>

			<!--
			<doc:variables>
				&no_var;
			</doc:variables>
			-->
		</partintro>
	</doc:reference>

	<doc:template basename="book" xmlns="">
		<refpurpose>Process a &DocBook; <doc:db>book</doc:db> document</refpurpose>
		<doc:description>
			<para>
				Entry point for <doc:db basename="book">books</doc:db>.
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.book.afterauthor"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.toc.section.depth"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.section.depth"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.book.begindocument"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.titlepage.file"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.maketitle"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<!--
		<doc:notes>
		</doc:notes>
		-->
		<doc:samples>
			<simplelist type='inline'>
				&test_book;
				&test_defguide;
			</simplelist>
		</doc:samples>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara>&mapping;</simpara></listitem>
				<listitem><simpara><xref linkend="template.generate.latex.book.preamble"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="book">
		<!-- book:1: generate.latex.book.preamble -->
		<xsl:call-template name="generate.latex.book.preamble"/>
		<!-- book:2: output title information     -->
		<xsl:text>\title{</xsl:text>
			<xsl:apply-templates select="title|bookinfo/title"/>
			<xsl:apply-templates select="subtitle|bookinfo/subtitle"/>
		<xsl:text>}&#10;</xsl:text>
		<!-- book:3: output author information     -->
		<xsl:text>\author{</xsl:text>
		<xsl:choose>
			<xsl:when test="bookinfo/authorgroup">
				<xsl:apply-templates select="bookinfo/authorgroup"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:for-each select="bookinfo">
					<xsl:call-template name="authorgroup"/>
				</xsl:for-each>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:text>}&#10;</xsl:text>
		<!-- book:4: dump any preamble after author  -->
		<xsl:value-of select="$latex.book.afterauthor"/>
		<!-- book:5: set some counters  -->
		<xsl:text>&#10;\setcounter{tocdepth}{</xsl:text><xsl:value-of select="$toc.section.depth"/><xsl:text>}&#10;</xsl:text>
		<xsl:text>&#10;\setcounter{secnumdepth}{</xsl:text><xsl:value-of select="$section.depth"/><xsl:text>}&#10;</xsl:text>
		<!-- book:6: dump the begin document command  -->
		<xsl:value-of select="$latex.book.begindocument"/>
		<!-- book:7: include external Cover page if specified -->
		<xsl:if test="$latex.titlepage.file != ''">
			<xsl:text>&#10;\InputIfFileExists{</xsl:text><xsl:value-of select="$latex.titlepage.file"/>
			<xsl:text>}{\typeout{WARNING: Using cover page </xsl:text>
			<xsl:value-of select="$latex.titlepage.file"/>
			<xsl:text>}}</xsl:text>
		</xsl:if>
		<!-- book:7b: maketitle and set up pagestyle -->
		<xsl:value-of select="$latex.maketitle"/>
		<!-- book:8: - APPLY TEMPLATES -->
		<xsl:apply-templates select="bookinfo"/>
		<xsl:call-template name="content-templates-rootid"/>
		<!-- book:9:  call map.end -->
		<xsl:call-template name="map.end"/>
	</xsl:template>

	<doc:template basename="title" xmlns="">
		<refpurpose>Process <doc:db>title</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Typesets a bold title.
			</para>
		</doc:description>
	</doc:template>
	<xsl:template match="book/title">\bfseries <xsl:apply-templates/></xsl:template>

	<doc:template basename="subtitle" xmlns="">
		<refpurpose>Process <doc:db>subtitle</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Typesets a bold subtitle, spaced 12pt from the preceding <doc:db>title</doc:db>.
			</para>
		</doc:description>
	</doc:template>
	<xsl:template match="book/subtitle">\\[12pt]\normalsize <xsl:apply-templates/></xsl:template>

	<doc:template basename="title" xmlns="">
		<refpurpose>Process <doc:db>title</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Typesets a bold title.
			</para>
		</doc:description>
	</doc:template>
	<xsl:template match="book/bookinfo/title">\bfseries <xsl:apply-templates/></xsl:template>

	<doc:template basename="subtitle" xmlns="">
		<refpurpose>Process <doc:db>subtitle</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Typesets a bold subtitle, spaced 12pt from the preceding <doc:db>title</doc:db>.
			</para>
		</doc:description>
	</doc:template>
	<xsl:template match="book/bookinfo/subtitle">\\[12pt]\normalsize <xsl:apply-templates/></xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>bookinfo</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Processes a <doc:db>book</doc:db>'s <doc:db>bookinfo</doc:db>
				(will be invoked after the title page has been typeset).
			</para>
		</doc:description>
		<doc:notes>
			<para>

				Only the <doc:db>revhistory</doc:db>,
				<doc:db>abstract</doc:db>, <doc:db>keywordset</doc:db>,
				<doc:db>copyright</doc:db> and <doc:db>legalnotice</doc:db> are
				processed. Users may override this in their customisation
				layer.

			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="book/bookinfo">
		<xsl:apply-templates select="revhistory" />
		<xsl:apply-templates select="copyright" />
		<xsl:apply-templates select="keywordset" />
		<xsl:apply-templates select="legalnotice" />
		<xsl:apply-templates select="abstract" />
	</xsl:template>

	<doc:template basename="copyright" xmlns="">
		<refpurpose>Process <doc:db>bookinfo</doc:db>'s <doc:db>copyright</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Formats a block-style copyright.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:description>
			<para>Calls <xref linkend="template.copyright"/>.</para>
		</doc:description>
	</doc:template>
	<xsl:template match="bookinfo/copyright">
		<xsl:text>\begin{center}</xsl:text>
		<xsl:call-template name="copyright"/>
		<xsl:text>\end{center}&#10;</xsl:text>
	</xsl:template>

	<doc:template basename="article" xmlns="">
		<refpurpose>Process a <doc:db>book</doc:db>'s <doc:db>article</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Entry point for a <literal>book</literal>'s <doc:db basename="article">articles</doc:db>.
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.book.article.title.style"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.book.article.header.style"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:notes>
			<para>
				For double-sided books, each article will commence on a right-hand page.
			</para>
			<para>
				
				This template will call upon the following variables or
				templates in this order:
				<literal>generate.latex.pagestyle</literal>,
				<literal>title</literal> (or <doc:db>articleinfo</doc:db> or
				<doc:db>artheader</doc:db>),
				<literal>$latex.book.article.title.style</literal>,
				<literal>date</literal> (from <literal>articleinfo</literal> or
				<literal>artheader</literal>), <literal>authorgroup</literal>
				or <literal>author</literal> (or <literal>articleinfo</literal>
				or <literal>artheader</literal>),
				<literal>$latex.book.article.header.style</literal>,
				<literal>articleinfo</literal> or <literal>artheader</literal>
				in the XSLT <literal>article.within.book</literal> mode,
				<literal>content-templates</literal>.

			</para>
		</doc:notes>
		<!--
		<doc:samples>
			<simplelist type='inline'>
				&test_book;
				&test_defguide;
			</simplelist>
		</doc:samples>
		-->
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="template.generate.latex.pagestyle"/></simpara></listitem>
				<listitem><simpara><xref linkend="template.article/artheader|article/articleinfo-article.within.book"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
		<!--
	    <formalpara><title>Tasks</title>
		<itemizedlist>
		    <listitem><para>Calls <literal>generate.latex.article.preamble</literal>.</para></listitem>
		    <listitem><para>Outputs \title, \author, \date, getting the information from its children.</para></listitem>
		    <listitem><para>Calls <literal>latex.article.begindocument</literal>.</para></listitem>
		    <listitem><para>Calls <literal>latex.article.maketitle.</literal></para></listitem>
		    <listitem><para>Applies templates.</para></listitem>
		    <listitem><para>Calls <literal>latex.article.end</literal> template.</para></listitem>
		</itemizedlist>
	    </formalpara>
		-->
	<xsl:template match="book/article">
		<xsl:text>&#10;\makeatletter\if@openright\cleardoublepage\else\clearpage\fi</xsl:text>
		<xsl:call-template name="generate.latex.pagestyle"/>
		<xsl:text>\makeatother&#10;</xsl:text>	
		<!-- Get and output article title -->
		<xsl:variable name="article.title">
			<xsl:choose>
				<xsl:when test="./title"> 
					<xsl:apply-templates select="./title"/>
				</xsl:when>
				<xsl:when test="./articleinfo/title">
					<xsl:apply-templates select="./articleinfo/title"/>
				</xsl:when>
				<xsl:otherwise>
					<xsl:apply-templates select="./artheader/title"/>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:variable>
		<xsl:text>\begin{center}{</xsl:text>
		<xsl:value-of select="$latex.book.article.title.style"/>
		<xsl:text>{</xsl:text>
		<xsl:value-of select="$article.title"/>
		<xsl:text>}}\par&#10;</xsl:text>
		<!-- Display date information -->
		<xsl:variable name="article.date">
			<xsl:apply-templates select="./artheader/date|./articleinfo/date"/>
		</xsl:variable>
		<xsl:if test="$article.date!=''">
			<xsl:text>{</xsl:text>
			<xsl:value-of select="$article.date"/>
			<xsl:text>}\par&#10;</xsl:text>
		</xsl:if>
		<!-- Display author information -->
		<xsl:text>{</xsl:text>
		<xsl:value-of select="$latex.book.article.header.style"/>
		<xsl:text>{</xsl:text>
		<xsl:choose>
			<xsl:when test="articleinfo/authorgroup">
				<xsl:apply-templates select="articleinfo/authorgroup"/>
			</xsl:when>
			<xsl:when test="artheader/authorgroup">
				<xsl:apply-templates select="artheader/authorgroup"/>
			</xsl:when>
			<xsl:when test="articleinfo/author">
				<xsl:for-each select="artheader">
					<xsl:call-template name="authorgroup"/>
				</xsl:for-each>
			</xsl:when>
			<xsl:when test="artheader/author">
				<xsl:for-each select="artheader">
					<xsl:call-template name="authorgroup"/>
				</xsl:for-each>
			</xsl:when>
			<xsl:otherwise>
				<xsl:call-template name="authorgroup"/>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:text>}}\par&#10;</xsl:text>
		<xsl:apply-templates select="artheader|articleinfo" mode="article.within.book"/>
		<xsl:text>\end{center}&#10;</xsl:text>
		<xsl:call-template name="content-templates"/>
	</xsl:template>

	<doc:template basename="article" xmlns="">
		<refpurpose>Process a &DocBook; <doc:db>article</doc:db> document</refpurpose>
		<doc:description>
			<para>
				Entry point for <doc:db basename="article">articles</doc:db>.
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.toc.section.depth"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.section.depth"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.article.title.style"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.maketitle"/></simpara></listitem>
				<!--
				<listitem><simpara><xref linkend="param.latex.article.begindocument"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.article.end"/></simpara></listitem>
				-->
			</itemizedlist>
		</doc:variables>
		<doc:notes>
			<para>
				
				This template will call upon the following variables or
				templates in this order:
				<literal>generate.latex.article.preamble</literal>,
				<literal>$toc.section.depth</literal>,
				<literal>$section.depth</literal>,
				<literal>title</literal> (or <doc:db>articleinfo</doc:db> or
				<doc:db>artheader</doc:db>),
				<literal>$latex.article.title.style</literal>,
				<literal>date</literal> (from <literal>articleinfo</literal> or
				<literal>artheader</literal>), <literal>authorgroup</literal>
				or <literal>author</literal> (or <literal>articleinfo</literal>
				or <literal>artheader</literal>), <literal>map.begin</literal>,
				<literal>$latex.maketitle</literal>,
				<literal>articleinfo</literal> or <literal>artheader</literal>,
				<literal>content-templates</literal>,
				<literal>map.end</literal>.

			</para>
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_article;
				&test_minimal;
			</simplelist>
		</doc:samples>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara>&mapping;: this template will use the <literal>article</literal> mappings.</simpara></listitem>
				<listitem><simpara><xref linkend="template.article/artheader|article/articleinfo-standalone.article"/></simpara></listitem>
				<listitem><simpara><xref linkend="template.generate.latex.article.preamble"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="article">
		<xsl:call-template name="generate.latex.article.preamble"/>
		<xsl:text>&#10;\setcounter{tocdepth}{</xsl:text><xsl:value-of select="$toc.section.depth"/><xsl:text>}&#10;</xsl:text>
		<xsl:text>&#10;\setcounter{secnumdepth}{</xsl:text><xsl:value-of select="$section.depth"/><xsl:text>}&#10;</xsl:text>
	<!-- Get and output article title -->
		<xsl:variable name="article.title">
			<xsl:choose>
				<xsl:when test="./title"> 
					<xsl:apply-templates select="./title"/>
				</xsl:when>
				<xsl:when test="./articleinfo/title">
					<xsl:apply-templates select="./articleinfo/title"/>
				</xsl:when>
				<xsl:otherwise>
					<xsl:apply-templates select="./artheader/title"/>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:variable>
		<xsl:text>\title{</xsl:text>
		<xsl:value-of select="$latex.article.title.style"/>
		<xsl:text>{</xsl:text>
		<xsl:value-of select="$article.title"/>
		<xsl:text>}}&#10;</xsl:text>
		<!-- Display date information -->
		<xsl:variable name="article.date">
			<xsl:apply-templates select="./artheader/date|./articleinfo/date"/>
		</xsl:variable>
		<xsl:if test="$article.date!=''">
			<xsl:text>\date{</xsl:text>
			<xsl:value-of select="$article.date"/>
			<xsl:text>}&#10;</xsl:text>
		</xsl:if>
		<!-- Display author information -->
		<xsl:text>\author{</xsl:text>
		<xsl:choose>
			<xsl:when test="articleinfo/authorgroup">
				<xsl:apply-templates select="articleinfo/authorgroup"/>
			</xsl:when>
			<xsl:when test="artheader/authorgroup">
				<xsl:apply-templates select="artheader/authorgroup"/>
			</xsl:when>
			<xsl:when test="articleinfo/author">
				<xsl:for-each select="artheader">
					<xsl:call-template name="authorgroup"/>
				</xsl:for-each>
			</xsl:when>
			<xsl:when test="artheader/author">
				<xsl:for-each select="artheader">
					<xsl:call-template name="authorgroup"/>
				</xsl:for-each>
			</xsl:when>
			<xsl:otherwise>
				<xsl:call-template name="authorgroup"/>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:text>}&#10;</xsl:text>
		<!-- Display  begindocument command -->
		<xsl:call-template name="map.begin"/>
		<xsl:value-of select="$latex.maketitle"/>
		<xsl:apply-templates select="artheader|articleinfo" mode="standalone.article"/>
		<xsl:call-template name="content-templates-rootid"/>
		<xsl:call-template name="map.end"/>
    </xsl:template>

	<doc:template basename="date" xmlns="">
		<refpurpose>Process <doc:db>date</doc:db> in <doc:db>articleinfo</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<!--
		<doc:samples>
			<simplelist type='inline'>
				&test_book;
				&test_defguide;
			</simplelist>
		</doc:samples>
		-->
	</doc:template>
	<xsl:template match="articleinfo/date|artheader/date">
		<xsl:apply-templates/>
	</xsl:template>

	<doc:template basename="articleinfo" xmlns="">
		<refpurpose>Process <doc:db>articleinfo</doc:db> in <doc:db>article</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates for <doc:db>legalnotice</doc:db> and <doc:db>abstract</doc:db>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<!--
		<doc:samples>
			<simplelist type='inline'>
				&test_book;
				&test_defguide;
			</simplelist>
		</doc:samples>
		-->
	</doc:template>
	<xsl:template match="article/artheader|article/articleinfo" mode="standalone.article">
		<xsl:apply-templates select="keywordset" />
		<xsl:apply-templates select="legalnotice" />
		<xsl:apply-templates select="abstract"/>
	</xsl:template>

	<xsl:template match="article/artheader|article/articleinfo"/>

	<doc:template basename="articleinfo" xmlns="">
		<refpurpose>Process <doc:db>articleinfo</doc:db> in <doc:db>article</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates for <doc:db>abstract</doc:db> and <doc:db>legalnotice</doc:db>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<!--
		<doc:samples>
			<simplelist type='inline'>
				&test_book;
				&test_defguide;
			</simplelist>
		</doc:samples>
		-->
	</doc:template>
	<xsl:template match="article/artheader|article/articleinfo" mode="article.within.book">
		<xsl:apply-templates select="abstract"/>
		<xsl:apply-templates select="legalnotice" />
	</xsl:template>

	<doc:template basename="legalnotice" xmlns="">
		<refpurpose>Process <doc:db>legalnotice</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Typesets legal notices.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<note>
				<para>This should be done via the mapping system!</para>
			</note>
			<para>

				For two-column documents, the <doc:db>title</doc:db> is
				formatted in italics and followed immediately by the notice's
				content. For single-column documents, the
				<literal>title</literal> is formatted in bold, centred on a
				line of its own, and the body of the legal notice is formatted
				as an indented small-font quotation.

			</para>
			<para>
				The <doc:db>blockinfo</doc:db> is not processed
				(only the <doc:db>title</doc:db> is used).
			</para>
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_book;
				&test_entities;
			</simplelist>
		</doc:samples>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara>&mapping;</simpara></listitem>
				<listitem><simpara><xref linkend="template.legalnotice.title"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="legalnotice">
		<xsl:text>&#10;{\if@twocolumn&#10;</xsl:text>
			<xsl:text>\noindent\small\textit{&#10;</xsl:text>
			<xsl:call-template name="legalnotice.title"/>
			<xsl:text>}\/\bfseries---$\!$%&#10;</xsl:text>
		<xsl:text>\else&#10;</xsl:text>
			<xsl:text>\noindent\begin{center}\small\bfseries &#10;</xsl:text>
			<xsl:call-template name="legalnotice.title"/>
			<xsl:text>\end{center}\begin{quote}\small&#10;</xsl:text>
		<xsl:text>\fi&#10;</xsl:text>
		<xsl:call-template name="content-templates"/>
		<xsl:text>\vspace{0.6em}\par\if@twocolumn\else\end{quote}\fi}&#10;</xsl:text>
		<!--
		<xsl:text>\normalsize\rmfamily&#10;</xsl:text>
		-->
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose> Choose a title for <doc:db basename="legalnotice">legalnotices</doc:db> </refpurpose>
		<doc:description>
			<para>
				Typesets a title.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>

				Will look for
				<doc:db>blockinfo</doc:db>/<doc:db>title</doc:db>,
				<doc:db>legalnotice</doc:db>/<doc:db>title</doc:db>
				or a <quote>gentext</quote> title
				(the first of the three will be used).

			</para>
		</doc:notes>
		<doc:samples>
			<para>See <xref linkend="template.legalnotice"/></para>
		</doc:samples>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara>&mapping;</simpara></listitem>
				<listitem><simpara><xref linkend="template.legalnotice.title"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template name="legalnotice.title">
		<xsl:param name="title" select="blockinfo/title|title"/>
		<xsl:choose>
			<xsl:when test="count($title)>0">
				<xsl:apply-templates select="$title[1]"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:call-template name="gentext">
					<xsl:with-param name="key">legalnotice</xsl:with-param>
				</xsl:call-template>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

	<doc:template basename="toc" xmlns="">
		<refpurpose> Generate and typeset a <doc:db>toc</doc:db> </refpurpose>
		<doc:description>
			<para>
				Produce a chapter-level table of contents in &LaTeX;.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>

				This template ignores its contents and instead invokes the
				&LaTeX; <function condition="latex">tableofcontents</function>
				command. You will need to run your typesetter at least twice,
				and possibly three times, to have the table of contents
				generated normally. The headers, footers, and chapter title
				will be generated by &LaTeX;.

			</para>
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_book;
				&test_lot;
			</simplelist>
		</doc:samples>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="template.latex.noparskip"/></simpara></listitem>
				<listitem><simpara><xref linkend="template.latex.restoreparskip"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="toc" name="toc">
		<xsl:text>&#10;</xsl:text>
		<xsl:call-template name="latex.noparskip"/>
		<xsl:choose>
			<xsl:when test="$latex.use.hyperref=1">
				<xsl:text>
\makeatletter
\def\dbtolatex@contentsid{</xsl:text>
				<xsl:call-template name="generate.label.id"/>
				<xsl:text>}
\let\dbtolatex@@contentsname\contentsname
\newif\ifdocbooktolatexcontentsname\docbooktolatexcontentsnametrue
\def\dbtolatex@contentslabel{%
 \label{\dbtolatex@contentsid}\hypertarget{\dbtolatex@contentsid}{\dbtolatex@@contentsname}%
 \global\docbooktolatexcontentsnamefalse}
\def\contentsname{\ifdocbooktolatexcontentsname\dbtolatex@contentslabel\else\dbtolatex@@contentsname\fi}
\tableofcontents
\let\contentsname\dbtolatex@@contentsname
\Hy@writebookmark{}{\dbtolatex@@contentsname}{\dbtolatex@contentsid}{0}{toc}%
\makeatother
				</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text>\tableofcontents&#10;</xsl:text>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:call-template name="latex.restoreparskip"/>
	</xsl:template>

	<doc:template basename="toc" xmlns="">
		<refpurpose> Generate and typeset a <doc:db>toc</doc:db> </refpurpose>
		<doc:description>
			<para>
				Produce a chapter-level table of contents in &LaTeX;.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:params>
			<variablelist>
				<varlistentry>
					<term>prefer</term>
					<listitem>
						<para>

							&LaTeX; can provide <function
							condition="latex">listoffigures</function> and
							<function condition="latex">listoftables</function>
							by default. This parameter allows you to select
							which should be used. Recognised values are
							<quote>figures</quote> and <quote>tables</quote>.
							If the value is empty or not recognised, both lists
							will be output. By default, the value of the
							current node's non-empty <sgmltag
							class="attribute">condition</sgmltag>, <sgmltag
							class="attribute">role</sgmltag> or <sgmltag
							class="attribute">label</sgmltag> attribute will be
							used.

						</para>
					</listitem>
				</varlistentry>
			</variablelist>
		</doc:params>
		<doc:notes>
			<para>

				This template ignores its contents and instead invokes the
				&LaTeX; <function condition="latex">listoffigures</function> or
				<function condition="latex">listoftables</function> commands.
				You will need to run your typesetter at least twice, and
				possibly three times, to have the table of contents generated
				normally. The headers, footers, and chapter title will be
				generated by &LaTeX;.

			</para>
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_lot1;
				&test_lot2;
				&test_lot3;
			</simplelist>
		</doc:samples>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.bridgehead.in.lot"/></simpara></listitem>
				<listitem><simpara><xref linkend="template.latex.noparskip"/></simpara></listitem>
				<listitem><simpara><xref linkend="template.latex.restoreparskip"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="lot" name="lot">
		<xsl:param name="prefer">
			<xsl:choose>
				<xsl:when test="@condition!=''">
					<xsl:value-of select="@condition"/>
				</xsl:when>
				<xsl:when test="@role!=''">
					<xsl:value-of select="@role"/>
				</xsl:when>
				<xsl:otherwise>
					<xsl:value-of select="@label"/>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:param>
		<xsl:call-template name="latex.noparskip"/>
		<xsl:choose>
			<xsl:when test="$prefer='figures'">
				<xsl:text>\listoffigures&#10;</xsl:text>
			</xsl:when>
			<xsl:when test="$prefer='tables'">
				<xsl:text>\listoftables&#10;</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text>\listoffigures&#10;</xsl:text>
				<xsl:text>\listoftables&#10;</xsl:text>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:call-template name="latex.restoreparskip"/>
	</xsl:template>

<!--
	<xsl:template match="lotentry">
	</xsl:template>

	<xsl:template match="lotentry"/>
	<xsl:template match="tocpart|tocchap|tocfront|tocback|tocentry"/>
	<xsl:template match="toclevel1|toclevel2|toclevel3|toclevel4|toclevel5"/>
-->

	<doc:template xmlns="">
		<refpurpose> Choose the preferred page style for document body </refpurpose>
		<doc:description>
			<para>

				If no page style is preferred by the user, the defaults will be
				<literal>empty</literal> for <doc:db
				basename="article">articles</doc:db>, <literal>plain</literal>
				for <doc:db basename="book">books</doc:db>, or
				<literal>fancy</literal> (if the &LaTeX;
				<productname>fancyhdr</productname> package is permitted).

			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.pagestyle"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.use.fancyhdr"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:notes>
			<para>
			
			The &LaTeX; <function condition="latex">pagestyle</function>
			command is used to effect the page style.
			
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template name="generate.latex.pagestyle">
		<xsl:text>\pagestyle{</xsl:text>
		<xsl:choose>
			<xsl:when test="$latex.pagestyle!=''">
				<xsl:value-of select="$latex.pagestyle"/>
			</xsl:when>
			<xsl:when test="count(//book)&gt;0">
				<xsl:choose>
					<xsl:when test="$latex.use.fancyhdr=1"><xsl:text>fancy</xsl:text></xsl:when>
					<xsl:otherwise><xsl:text>plain</xsl:text></xsl:otherwise>
				</xsl:choose>
			</xsl:when>
			<xsl:otherwise><xsl:text>empty</xsl:text></xsl:otherwise>
		</xsl:choose>
		<xsl:text>}&#10;</xsl:text>
	</xsl:template>
	
</xsl:stylesheet>

