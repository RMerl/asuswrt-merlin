<?xml version="1.0" encoding="ISO-8859-1"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version='1.0'
	xmlns:samba="http://www.samba.org/samba/DTD/samba-doc">
<xsl:import href="db2latex-xsl/xsl/docbook.xsl"/>
<xsl:import href="strip-references.xsl"/>

<xsl:param name="latex.mapping.xml" select="document('latex.overrides.xml')"/>

<xsl:param name="generate.toc">
	/appendix toc,title
	article/appendix  nop
	/article  toc,title
	book      toc,title,figure,table,example,equation
	/chapter  toc,title,lop
	part      toc,title
	/preface  toc,title
	qandadiv  toc
	qandaset  toc
	procedure lop
	reference toc,title
	/sect1    toc
	/sect2    toc
	/sect3    toc
	/sect4    toc
	/sect5    toc
	/section  toc
	set       toc,title
</xsl:param>

<!-- Show real name of the link rather then user specified description -->
<xsl:template match="link">
	<xsl:variable name="target" select="key('id',@linkend)[1]"/>
	<xsl:variable name="refelem" select="local-name($target)"/>
	<xsl:if test="$refelem=''">
		<xsl:message><xsl:text>XRef to nonexistent id: </xsl:text><xsl:value-of select="@linkend"/></xsl:message>
		<xsl:text>XrefId[?</xsl:text>
		<xsl:apply-templates/>
		<xsl:text>?]</xsl:text>
	</xsl:if>

	<xsl:call-template name="generate.hyperlink">
		<xsl:with-param name="target" select="$target"/>
		<xsl:with-param name="text">
			<xsl:call-template name="generate.xref.text">
				<xsl:with-param name="target" select="$target"/>
			</xsl:call-template>
		</xsl:with-param>
	</xsl:call-template>
</xsl:template>

<!-- LaTeX doesn't accept verbatim stuff in titles -->
<xsl:template match="//title/filename|//title/command|//title/parameter|//title/constant">
  <xsl:variable name="content">
    <xsl:apply-templates/>
  </xsl:variable>
  <xsl:if test="$content != ''">
    <xsl:value-of select="$content" />
  </xsl:if>
</xsl:template>

<xsl:output method="text" encoding="UTF-8" indent="yes"/>
<xsl:variable name="l10n.gentext.default.language" select="'en'"/>
<xsl:variable name="latex.document.font">default</xsl:variable>
<xsl:variable name="latex.example.caption.style"></xsl:variable>
<xsl:variable name="latex.hyperref.param.pdftex">hyperfigures,hyperindex,citecolor=black,urlcolor=black,filecolor=black,linkcolor=black,menucolor=red,pagecolor=black</xsl:variable>
<xsl:variable name="admon.graphics.path">xslt/figures</xsl:variable>
<xsl:variable name="latex.use.tabularx">1</xsl:variable>
<xsl:variable name="latex.fancyhdr.lh"></xsl:variable>
<xsl:variable name="latex.use.fancyhdr"></xsl:variable>
<xsl:variable name="latex.use.parskip">1</xsl:variable>
<xsl:variable name="latex.use.ucs">1</xsl:variable>
<xsl:variable name="latex.inputenc">utf8</xsl:variable>
<xsl:variable name="latex.book.varsets" select="''"/>
<xsl:variable name="latex.hyphenation.tttricks">1</xsl:variable>
<xsl:variable name="latex.titlepage.file"></xsl:variable>
<xsl:template name="latex.thead.row.entry">
<xsl:text>{\bfseries </xsl:text><xsl:apply-templates/><xsl:text>}</xsl:text>
</xsl:template>
<xsl:variable name="latex.documentclass">sambadoc</xsl:variable>
<xsl:variable name="latex.babel.language">english</xsl:variable>
<xsl:variable name="ulink.footnotes" select="1"/>
<xsl:variable name="ulink.show" select="0"/>

<xsl:template match="smbconfblock/smbconfoption">
	<xsl:text>	</xsl:text><xsl:value-of select="@name"/>
	<xsl:if test="text() != ''">
		<xsl:text> = </xsl:text>
		<xsl:value-of select="text()"/>
	</xsl:if>
	<xsl:text>&#10;</xsl:text>
</xsl:template>

<xsl:template match="smbconfblock/smbconfcomment">
	<xsl:text># </xsl:text>
	<xsl:apply-templates/>
	<xsl:text>&#10;</xsl:text>
</xsl:template>

<xsl:template match="smbconfblock/smbconfsection">
	<xsl:value-of select="@name"/>
	<xsl:text>&#10;</xsl:text>
</xsl:template>

<xsl:template match="smbconfoption">
	<xsl:text>\smbconfoption{</xsl:text>
	<xsl:call-template name="scape">
		<xsl:with-param name="string" select="@name"/>
	</xsl:call-template>
	<xsl:text>}</xsl:text>

	<xsl:choose>
		<xsl:when test="text() != ''">
			<xsl:text> = </xsl:text>
			<xsl:call-template name="scape">
				<xsl:with-param name="string" select="text()"/>
			</xsl:call-template>
		</xsl:when>
	</xsl:choose>
</xsl:template>

<xsl:template match="smbconfblock">
	<xsl:text>&#10;\begin{lstlisting}[language=smbconf,style=smbconfblock]&#10;</xsl:text>
	<xsl:apply-templates/>
	<xsl:text>\end{lstlisting}&#10;</xsl:text>
</xsl:template>

<xsl:template match="smbconfsection">
	<xsl:text>\smbconfsection{</xsl:text>
	<xsl:call-template name="scape">
		<xsl:with-param name="string" select="@name"/>
	</xsl:call-template>
	<xsl:text>}</xsl:text>
</xsl:template>

<xsl:template match="imagefile">
	<xsl:text>\includegraphics[scale=</xsl:text>
	<xsl:choose>
		<xsl:when test="@scale != ''"><xsl:value-of select="@scale div 100"/></xsl:when>

		<xsl:otherwise><xsl:text>.50</xsl:text></xsl:otherwise>
	</xsl:choose>
	<xsl:text>]{</xsl:text>
	<xsl:value-of select="$latex.imagebasedir"/><xsl:text>images/</xsl:text>
	<xsl:value-of select="text()"/>
	<xsl:text>}&#10;</xsl:text>
</xsl:template>

<!-- smb.conf documentation -->

<xsl:template match="description"><xsl:apply-templates/></xsl:template>

<xsl:template match="value"><xsl:apply-templates/></xsl:template>

<xsl:template match="synonym"><xsl:apply-templates/></xsl:template>

<xsl:template match="related"><xsl:apply-templates/></xsl:template>

<xsl:template match="//samba:parameterlist">
	<xsl:text>\begin{description}&#10;</xsl:text>
	<xsl:apply-templates>
		<xsl:sort select="samba:parameter/@name"/>
	</xsl:apply-templates>
	<xsl:text>\end{description}&#10;</xsl:text>
</xsl:template>

<xsl:template match="value/comment">
	<xsl:text>&#10;# </xsl:text>
	<xsl:apply-templates/>
</xsl:template>

<xsl:template match="/">
	<xsl:apply-templates/>
</xsl:template>

<xsl:template match="refentry">
	<xsl:text>\section{</xsl:text><xsl:value-of select="refmeta/refentrytitle"/><xsl:text>}&#10;</xsl:text>
	<xsl:apply-templates/>
</xsl:template>

<xsl:template match="//samba:parameter">
	<xsl:for-each select="synonym">
		<xsl:text>\item[{</xsl:text><xsl:value-of select="."/><xsl:text>}]\null{}&#10;</xsl:text>
	<xsl:text>\index{</xsl:text><xsl:value-of select="."/><xsl:text>}&#10;</xsl:text>
	<xsl:text>This parameter is a synonym for \smbconfoption{</xsl:text><xsl:value-of select="../@name"/><xsl:text>}.</xsl:text>
	</xsl:for-each>

	<xsl:text>\item[{</xsl:text><xsl:value-of select="@name"/>
	<xsl:text> (</xsl:text>
	<xsl:value-of select="@context"/>
	<xsl:text>)</xsl:text>
	<xsl:text>}]\null{}&#10;&#10;</xsl:text>
	<xsl:text>\index{</xsl:text><xsl:value-of select="@name"/><xsl:text>}&#10;</xsl:text>

	<!-- Print default value-->
	<xsl:text>&#10;</xsl:text>
	<xsl:text>Default: </xsl:text>
	<xsl:text>\emph{</xsl:text>
	<xsl:choose>
		<xsl:when test="value[@type='default'] != ''">
			<xsl:value-of select="@name"/>
			<xsl:text> = </xsl:text>
			<xsl:apply-templates select="value"/>
		</xsl:when>
		<xsl:otherwise>
			<xsl:text>No default</xsl:text>
		</xsl:otherwise>
	</xsl:choose>
	<xsl:text>}</xsl:text>
	<xsl:text>&#10;</xsl:text>

	<!-- Generate list of examples -->
	<xsl:text>&#10;</xsl:text>
	<xsl:for-each select="value[@type='example']">
		<xsl:text>&#10;</xsl:text>
		<xsl:text>Example: </xsl:text>
		<xsl:text>\emph{</xsl:text><xsl:value-of select="../@name"/>
		<xsl:text> = </xsl:text>
		<xsl:apply-templates select="."/>
		<xsl:text>}</xsl:text>
		<xsl:text>&#10;</xsl:text>
	</xsl:for-each>

	<!-- Description -->
	<xsl:apply-templates select="description"/>
</xsl:template>

</xsl:stylesheet>
