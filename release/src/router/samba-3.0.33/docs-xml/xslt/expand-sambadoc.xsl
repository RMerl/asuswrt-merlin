<?xml version='1.0'?>
<!-- 
	Samba-documentation specific stylesheets
	Published under the GNU GPL

	(C) Jelmer Vernooij 					2002-2004
-->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:samba="http://www.samba.org/samba/DTD/samba-doc"
        xmlns:xlink='http://www.w3.org/1999/xlink'
	version="1.1">

	<xsl:output method="xml" encoding="UTF-8" doctype-public="-//OASIS//DTD DocBook XML V4.2//EN" indent="yes" doctype-system="http://www.oasis-open.org/docbook/xml/4.2/docbookx.dtd"/>

	<xsl:include href="strip-references.xsl"/>
	<xsl:include href="expand-smbconfdoc.xsl"/>

	<!-- This is needed to copy content unchanged -->
	<xsl:template match="@*|node()">
		<xsl:copy>
			<xsl:apply-templates select="@*|node()"/>
		</xsl:copy>
	</xsl:template>

	<xsl:template name="xsmbconfoption">
		<xsl:param name="name"/>
		<xsl:param name="content"/>
		<xsl:variable name="linkcontent">
			<xsl:element name="parameter">
				<xsl:attribute name="moreinfo">
					<xsl:text>none</xsl:text>
				</xsl:attribute>
				<xsl:value-of select="$name"/>	
			</xsl:element>

			<xsl:choose>
				<xsl:when test="$content != ''">
					<xsl:text> = </xsl:text>
					<xsl:value-of select="$content"/>
				</xsl:when>
			</xsl:choose>
		</xsl:variable>

		<xsl:choose>
			<xsl:when test="$noreference = 1">
				<xsl:value-of select="$linkcontent"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:variable name="newid"><xsl:value-of select="translate(translate(string($name),' ',''),'abcdefghijklmnopqrstuvwxyz','ABCDEFGHIJKLMNOPQRSTUVWXYZ')"/></xsl:variable>
				<xsl:element name="link">
					<xsl:attribute name="linkend">
						<xsl:value-of select="$newid"/>
					</xsl:attribute>
					<xsl:attribute name="xlink:href">smb.conf.5.html#<xsl:value-of select="$newid"/></xsl:attribute>
					<xsl:value-of select="$linkcontent"/>
				</xsl:element>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

	<xsl:template match="related">
		<xsl:element name="para">
			<xsl:text>Related command: </xsl:text>
			<xsl:call-template name="xsmbconfoption">
				<xsl:with-param name="name" select="text()"/>
			</xsl:call-template>
		</xsl:element>
	</xsl:template>

	<xsl:template match="smbconfblock/smbconfoption">
		<xsl:element name="member">
			<xsl:element name="indexterm">
					<xsl:value-of select="@name"/>
			</xsl:element>
			<xsl:choose>
				<xsl:when test="text() != ''">
					<xsl:element name="parameter">
						<xsl:value-of select="@name"/>
						<xsl:text> = </xsl:text>
						<xsl:value-of select="text()"/>
					</xsl:element>
				</xsl:when>
			</xsl:choose>
		</xsl:element>
	</xsl:template>

	<xsl:template match="smbconfblock/smbconfcomment">
		<xsl:element name="member">
			<xsl:text># </xsl:text>
			<xsl:apply-templates/>
		</xsl:element>
	</xsl:template>

	<xsl:template match="smbconfblock/smbconfsection">
		<xsl:element name="member">
			<xsl:text> </xsl:text>
		</xsl:element>
		<xsl:element name="member">
			<xsl:element name="parameter">
				<xsl:value-of select="@name"/>
			</xsl:element>
		</xsl:element>
	</xsl:template>

	<xsl:template match="smbconfoption">
		<xsl:call-template name="xsmbconfoption">
			<xsl:with-param name="name" select="@name"/>
			<xsl:with-param name="content" select="text()"/>
		</xsl:call-template>
	</xsl:template>


	<xsl:template match="smbconfblock">
		<xsl:element name="simplelist">
			<xsl:apply-templates/>
		</xsl:element>
	</xsl:template>

	<xsl:template match="smbconfsection">
		<xsl:element name="parameter">
			<xsl:value-of select="@name"/>
		</xsl:element>
	</xsl:template>

	<xsl:template match="imagefile">
		<xsl:element name="mediaobject">
			<xsl:element name="imageobject">
				<xsl:attribute name="role"><xsl:text>html</xsl:text></xsl:attribute>
				<xsl:element name="imagedata">
					<xsl:attribute name="fileref">
						<xsl:text>images/</xsl:text><xsl:value-of select="text()"/><xsl:text>.png</xsl:text></xsl:attribute>
					<xsl:attribute name="scale">
						<xsl:choose>
							<xsl:when test="@scale != ''">
								<xsl:value-of select="@scale"/>
							</xsl:when>

							<xsl:otherwise>
								<xsl:text>100</xsl:text>
							</xsl:otherwise>
						</xsl:choose>
					</xsl:attribute>
					<xsl:attribute name="scalefit"><xsl:text>1</xsl:text></xsl:attribute>
				</xsl:element>
			</xsl:element>
			<xsl:element name="imageobject">
				<xsl:element name="imagedata">
					<xsl:attribute name="fileref">
						<xsl:text>images/</xsl:text><xsl:value-of select="text()"/><xsl:text>.png</xsl:text></xsl:attribute>
					<xsl:attribute name="scale">
						<xsl:choose>
							<xsl:when test="@scale != ''">
								<xsl:value-of select="@scale"/>
							</xsl:when>

							<xsl:otherwise>
								<xsl:text>50</xsl:text>
							</xsl:otherwise>
						</xsl:choose>
					</xsl:attribute>
					<xsl:attribute name="scalefit"><xsl:text>1</xsl:text></xsl:attribute>
				</xsl:element>
			</xsl:element>
			<xsl:element name="imageobject">
				<xsl:attribute name="role"><xsl:text>latex</xsl:text></xsl:attribute>
				<xsl:element name="imagedata">
					<xsl:attribute name="fileref">
						<xsl:value-of select="$latex.imagebasedir"/><xsl:text>images/</xsl:text><xsl:value-of select="text()"/></xsl:attribute>
				</xsl:element>
			</xsl:element>

		</xsl:element>
	</xsl:template>

</xsl:stylesheet>
