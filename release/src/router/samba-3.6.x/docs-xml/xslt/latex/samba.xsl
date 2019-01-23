<?xml version="1.0" encoding="ISO-8859-1"?>
<!--
	Expand Samba-specific XML elements to LaTeX, for use with dblatex

	Copyright (C) 2003,2004,2009 Jelmer Vernooij <jelmer@samba.org>
	Published under the GNU GPLv3 or later
-->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version='1.0'
	xmlns:samba="http://www.samba.org/samba/DTD/samba-doc">

<!-- FIXME: dblatex should have some way to load additional packages, user.params.set2 
	isn't really meant for this sort of thing -->
<xsl:template name="user.params.set2">
	<xsl:text>\usepackage{samba}&#10;</xsl:text>
</xsl:template>

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
