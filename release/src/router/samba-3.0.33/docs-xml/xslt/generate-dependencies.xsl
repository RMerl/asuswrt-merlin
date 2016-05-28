<?xml version='1.0'?>
<!--
	Find the image dependencies of a certain XML file
	Generates (part of) a Makefile

	- $(FNAME)-images-latex-{png,dvi} for role=latex
	- $(FNAME)-images-role for all other roles
	- $(TXTDIR)/$(FNAME)-text 

	(C) Jelmer Vernooij	2004-2005
-->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.1">
	<xsl:output method="text"/>

	<xsl:template match="/">
		<xsl:for-each select="//mediaobject/imageobject[@role and not(@role=following::imageobject/@role)]">
			<xsl:call-template name="generate-images">
				<xsl:with-param name="role"><xsl:value-of select="@role"/></xsl:with-param>
			</xsl:call-template>
		</xsl:for-each>
		<xsl:call-template name="generate-images">
			<xsl:with-param name="role"/>
		</xsl:call-template>
		<xsl:call-template name="generate-txt-chunks"/>
	</xsl:template>

	<xsl:template name="generate-images">
		<xsl:param name="role"/>
		<xsl:value-of select="$target"/><xsl:text>-images-</xsl:text><xsl:value-of select="$role"/><xsl:text> = </xsl:text>
		<xsl:for-each select="//mediaobject/imageobject[@role=$role]">
			<xsl:value-of select="imagedata/@fileref"/>
			<xsl:text> </xsl:text>
		</xsl:for-each>
		<xsl:text>&#10;</xsl:text>
	</xsl:template>

	<xsl:template name="generate-txt-chunks">
		<xsl:value-of select="$target"/><xsl:text>-txt-chunks: </xsl:text>
		<xsl:for-each select="(//chapter|//preface|//appendix)[@id]|book">
			<xsl:value-of select="$txtbasedir"/>
			<xsl:choose>
				<xsl:when test="name() = 'book'">
					<xsl:text>index</xsl:text>
				</xsl:when>
				<xsl:when test="@id != ''">
					<xsl:value-of select="@id"/>
				</xsl:when>
			</xsl:choose>
			<xsl:text>.txt </xsl:text>
		</xsl:for-each>
		<xsl:text>&#10;</xsl:text>
	</xsl:template>
</xsl:stylesheet>
