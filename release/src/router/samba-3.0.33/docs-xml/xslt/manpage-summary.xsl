<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:exsl="http://exslt.org/common"
	version="1.1">

	<xsl:output method="xml" encoding="UTF-8" omit-xml-declaration="yes"/>

	<xsl:template match="refentry">
		<xsl:element name="varlistentry">
			<xsl:apply-templates/>
		</xsl:element>
	</xsl:template>

	<xsl:template match="refentry/refmeta">
		<xsl:element name="term">
			<xsl:element name="ulink">
				<xsl:attribute name="url">
					<xsl:value-of select="refentrytitle"/><xsl:text>.</xsl:text><xsl:value-of select="manvolnum"/><xsl:text>.html</xsl:text>
				</xsl:attribute>
				<xsl:value-of select="refentrytitle"/><xsl:text>(</xsl:text><xsl:value-of select="manvolnum"/><xsl:text>)</xsl:text>
			</xsl:element>
		</xsl:element>
	</xsl:template>

	<xsl:template match="refentry/refnamediv">
		<xsl:element name="listitem">
			<xsl:element name="para">
				<xsl:value-of select="refpurpose"/><xsl:text>&#10;</xsl:text>
			</xsl:element>
		</xsl:element>
	</xsl:template>

	<xsl:template match="@*|node()">
		<xsl:apply-templates/>
	</xsl:template>
</xsl:stylesheet>
