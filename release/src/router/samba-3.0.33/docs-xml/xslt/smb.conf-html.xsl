<?xml version='1.0'?>
<!-- 
	Samba-documentation specific stylesheets
	Published under the GNU GPL

	(C) Jelmer Vernooij 					2005
-->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:exsl="http://exslt.org/common"
	xmlns:samba="http://www.samba.org/samba/DTD/samba-doc"
	version="1.1"
	extension-element-prefixes="exsl">

	<xsl:import href="html-common.xsl"/>

	<xsl:output method="xml" encoding="UTF-8" doctype-public="-//W3C//DTD XHTML 1.0 Strict//EN" indent="yes" doctype-system="http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd"/>

	<xsl:template match="samba:parameter">
		<xsl:element name="html">
			<xsl:element name="head">
				<xsl:element name="link">
					<xsl:attribute name="href"><xsl:text>smb.conf.xsl</xsl:text></xsl:attribute>
					<xsl:attribute name="rel"><xsl:text>stylesheet</xsl:text></xsl:attribute>
					<xsl:attribute name="type"><xsl:text>text/css</xsl:text></xsl:attribute>
				</xsl:element>

				<xsl:element name="title">
					<xsl:value-of select="@name"/>
				</xsl:element>
			</xsl:element>
			<xsl:element name="body">
				<xsl:element name="h1">
					<xsl:value-of select="@name"/>
				</xsl:element>

				<xsl:element name="div">
					<xsl:attribute name="class"><xsl:text>context</xsl:text></xsl:attribute>
					<xsl:text>Context: </xsl:text>
					<xsl:choose>
						<xsl:when test="@context = 'S'">
							<xsl:text>Share-specific</xsl:text>
						</xsl:when>

						<xsl:when test="@context = 'G'">
							<xsl:text>Global</xsl:text>
						</xsl:when>

						<xsl:otherwise>
							<xsl:message><xsl:text>Unknown value for context attribute : </xsl:text><xsl:value-of select="@context"/></xsl:message>
						</xsl:otherwise>
					</xsl:choose>
						
				</xsl:element>

				<xsl:element name="div">
					<xsl:attribute name="class"><xsl:text>type</xsl:text></xsl:attribute>
					<xsl:text>Type: </xsl:text>
					<xsl:value-of select="@type"/>
				</xsl:element>

				<xsl:element name="div">
					<xsl:attribute name="class"><xsl:text>synonyms</xsl:text></xsl:attribute>
					<xsl:text>Synonyms: </xsl:text>
					<xsl:for-each select="synonym">
						<xsl:text>, </xsl:text>
						<xsl:value-of select="text()"/>
					</xsl:for-each>
				</xsl:element>

				<xsl:for-each select="value">
					<xsl:element name="div">
						<xsl:attribute name="class"><xsl:text>value</xsl:text></xsl:attribute>
						<xsl:choose>
							<xsl:when test="@type = 'default'">
								<xsl:text>Default value</xsl:text>
							</xsl:when>
							<xsl:when test="@type = 'example'">
								<xsl:text>Example value</xsl:text>
							</xsl:when>
						</xsl:choose>

						<xsl:text>: </xsl:text>
					
						<xsl:value-of select="text()"/>

					</xsl:element>
				</xsl:for-each>

				<xsl:element name="div">
					<xsl:attribute name="class"><xsl:text>description</xsl:text></xsl:attribute>

					<xsl:for-each select="description">
						<xsl:apply-templates/>
					</xsl:for-each>
				</xsl:element>
			</xsl:element>
		</xsl:element>
	</xsl:template>

	<xsl:template match="description">
		<xsl:apply-templates/>
	</xsl:template>
</xsl:stylesheet>
