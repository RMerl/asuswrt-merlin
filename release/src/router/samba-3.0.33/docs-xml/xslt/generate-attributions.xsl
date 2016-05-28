<?xml version='1.0'?>
<!--
	Generate Docbook/XML file with attributions based on chapter/author tags
	(C) Jelmer Vernooij 2003
-->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
				version="1.1">

				<xsl:output method="xml" doctype-public="-//Samba-Team//DTD DocBook V4.2-Based Variant V1.0//EN" doctype-system="http://www.samba.org/samba/DTD/samba-doc" indent="yes"/>

<!-- Remove all character data -->
<xsl:template match="@*|node()">
   <xsl:apply-templates select="@*|node()"/>
</xsl:template>

<xsl:template match="/">
	<xsl:element name="preface">
		<xsl:element name="title"><xsl:text>Attribution</xsl:text></xsl:element>
		<xsl:apply-templates/>
	</xsl:element>
</xsl:template>

<xsl:template match="chapter|preface">
	<xsl:choose>
		<xsl:when test="chapterinfo/author != ''">
			<xsl:element name="para">
				<xsl:element name="link">
					<xsl:attribute name="linkend"><xsl:value-of select="@id"/></xsl:attribute>
					<xsl:value-of select="title"/>
				</xsl:element>
				<xsl:text>&#10;&#9;</xsl:text>
				<xsl:element name="itemizedlist">
					<xsl:apply-templates/>
				<xsl:text>&#9;</xsl:text>
				</xsl:element>
				<xsl:text>&#10;</xsl:text>
			</xsl:element>
		</xsl:when>
	</xsl:choose>
</xsl:template>

<xsl:template match="chapterinfo/author">
	<xsl:choose>
	<xsl:when test="firstname != ''">
	<xsl:text>&#9;</xsl:text>
	<xsl:element name="listitem">
		<xsl:element name="para">
			<xsl:value-of select="firstname"/>
			<xsl:if test="othername != ''">
				<xsl:text> </xsl:text>
				<xsl:value-of select="othername"/>
				<xsl:text> </xsl:text>
			</xsl:if>
			<xsl:text> </xsl:text><xsl:value-of select="surname"/>
			<xsl:choose>
				<xsl:when test="affiliation/address/email != ''">
					<xsl:element name="ulink">
						<xsl:attribute name="noescape">
						<xsl:text>1</xsl:text>
						</xsl:attribute>
						<xsl:attribute name="url">
							<xsl:text>mailto:</xsl:text>
							<xsl:value-of select="affiliation/address/email"/>
						</xsl:attribute>
					</xsl:element>
				</xsl:when>
			</xsl:choose>
			<xsl:choose>
				<xsl:when test="contrib != ''">
					<xsl:text> (</xsl:text>
						<xsl:value-of select="contrib"/>
					<xsl:text>) </xsl:text>
					</xsl:when>
			</xsl:choose>
		</xsl:element>
	</xsl:element>
	<xsl:text>&#10;</xsl:text>
	</xsl:when>
	</xsl:choose>
</xsl:template>

</xsl:stylesheet>
