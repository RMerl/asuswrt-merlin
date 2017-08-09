<?xml version="1.0" encoding="ISO-8859-1"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version='1.0'
	xmlns:samba="http://www.samba.org/samba/DTD/samba-doc">

	<!-- Remove bits from the manual pages that are not relevant for the HOWTO Collection, 
	such as author info, etc -->
	<xsl:import href="strip-references.xsl"/>

	<!-- Handle Samba-specific elements -->
	<xsl:import href="latex/samba.xsl"/>

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

<xsl:output method="text" encoding="UTF-8" indent="yes"/>
<xsl:variable name="l10n.gentext.default.language" select="'en'"/>
<xsl:variable name="latex.example.caption.style"></xsl:variable>
<xsl:variable name="latex.hyperref.param.pdftex">hyperfigures,hyperindex,citecolor=black,urlcolor=black,filecolor=black,linkcolor=black,menucolor=red,pagecolor=black</xsl:variable>
<xsl:variable name="latex.use.tabularx">1</xsl:variable>
<xsl:variable name="latex.use.parskip">1</xsl:variable>
<xsl:variable name="latex.hyphenation.tttricks">1</xsl:variable>
<xsl:template name="latex.thead.row.entry">
<xsl:text>{\bfseries </xsl:text><xsl:apply-templates/><xsl:text>}</xsl:text>
</xsl:template>
<!--
<xsl:variable name="latex.class.book">sambadoc</xsl:variable>
-->
<xsl:variable name="latex.babel.language">english</xsl:variable>
<xsl:variable name="ulink.footnotes" select="1"/>
<xsl:variable name="ulink.show" select="0"/>

<!-- The 'imagefile' element, so we can use images without three layers of XML elements -->
<xsl:template match="imagefile">
	<xsl:text>\includegraphics[scale=</xsl:text>
	<xsl:choose>
		<xsl:when test="@scale != ''"><xsl:value-of select="@scale div 100"/></xsl:when>

		<xsl:otherwise><xsl:text>.50</xsl:text></xsl:otherwise>
	</xsl:choose>
	<xsl:text>]{</xsl:text>
	<xsl:value-of select="text()"/>
	<xsl:text>}&#10;</xsl:text>
</xsl:template>
</xsl:stylesheet>
