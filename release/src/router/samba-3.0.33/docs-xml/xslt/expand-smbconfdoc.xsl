<?xml version='1.0'?>
<!-- 
	smb.conf-documentation specific stylesheets
	Published under the GNU GPL

	(C) Jelmer Vernooij 					2002-2004
	(C) Alexander Bokovoy 					2002-2004
-->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:samba="http://www.samba.org/samba/DTD/samba-doc"
	version="1.1">

	<xsl:template match="description"><xsl:apply-templates/></xsl:template>

	<xsl:template match="value"><xsl:element name="literal"><xsl:apply-templates/></xsl:element></xsl:template>
	
	<xsl:template match="command"><xsl:element name="literal"><xsl:apply-templates/></xsl:element></xsl:template>

	<xsl:template match="synonym"><xsl:apply-templates/></xsl:template>

	<xsl:template match="related"><xsl:element name="para"><xsl:text>Related command</xsl:text><xsl:apply-templates/></xsl:element></xsl:template>

	<xsl:template match="samba:parameterlist">
		<xsl:apply-templates>
			<xsl:sort select="samba:parameter/@name"/>
		</xsl:apply-templates>
	</xsl:template>

	<xsl:template match="value/comment">
		<xsl:text>&#10;# </xsl:text>
		<xsl:apply-templates/>
	</xsl:template>

	<xsl:template match="samba:parameter">
		<xsl:variable name="cname"><xsl:value-of select="translate(translate(string(@name),' ',''),
				'abcdefghijklmnopqrstuvwxyz','ABCDEFGHIJKLMNOPQRSTUVWXYZ')"/>
		</xsl:variable>

		<xsl:variable name="name"><xsl:value-of select="@name"/></xsl:variable>

		<xsl:variable name="anchor">
			<xsl:element name="anchor">
				<xsl:attribute name="id">
					<xsl:value-of select="$cname"/>
				</xsl:attribute>
			</xsl:element>
		</xsl:variable>

		<xsl:variable name="context">
			<xsl:text> (</xsl:text>
			<xsl:value-of select="@context"/>
			<xsl:text>)</xsl:text>
		</xsl:variable>

		<xsl:variable name="term">
				<xsl:copy-of select="$anchor"/>
				<xsl:element name="title">
					<xsl:text>&#10;</xsl:text>
					<xsl:text>&#10;</xsl:text>
					<xsl:value-of select="@name"/>
					<xsl:value-of select="$context"/>
					<xsl:text>&#10;</xsl:text>
				</xsl:element>
		</xsl:variable>


		<!-- Generate list of examples -->
		<xsl:variable name="examples">
			<xsl:for-each select="value">
				<xsl:if test="@type = 'example'">
					<xsl:element name="para">
						<xsl:text>Example: </xsl:text>
						<xsl:element name="emphasis">
							<xsl:element name="parameter">
								<xsl:copy-of select="$name"/>
							</xsl:element>
							<xsl:text> = </xsl:text>
							<xsl:apply-templates select="."/>
							<xsl:text>&#10;</xsl:text>
						</xsl:element>
						<xsl:text>&#10;</xsl:text>
					</xsl:element>
				</xsl:if>
			</xsl:for-each>
		</xsl:variable>

		<xsl:variable name="tdefault">
			<xsl:for-each select="value">
				<xsl:if test="@type = 'default'">
					<xsl:element name="para">
						<xsl:text>Default: </xsl:text>
						<xsl:element name="emphasis">
							<xsl:element name="parameter">
								<xsl:copy-of select="$name"/>
							</xsl:element>
							<xsl:text> = </xsl:text>
							<xsl:apply-templates select="."/>
							<xsl:text>&#10;</xsl:text>
						</xsl:element>
						<xsl:text>&#10;</xsl:text>
					</xsl:element>
				</xsl:if>
			</xsl:for-each>
		</xsl:variable>

		<xsl:variable name="default">
			<xsl:choose>
				<xsl:when test="$tdefault = ''">
					<xsl:element name="para">
						<xsl:element name="emphasis">
							<xsl:text>No default</xsl:text>
						</xsl:element>	
					</xsl:element>
				</xsl:when>
				<xsl:otherwise>
					<xsl:copy-of select="$tdefault"/>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:variable>

		<xsl:variable name="content">
			<xsl:apply-templates select="description"/>
		</xsl:variable>

		<xsl:for-each select="synonym">
			<xsl:element name="section">
				<xsl:element name="indexterm">
					<xsl:attribute name="significance">
						<xsl:text>preferred</xsl:text>
					</xsl:attribute>
					<xsl:element name="primary">
						<xsl:value-of select="."/>
					</xsl:element>
					<xsl:element name="see">
						<xsl:value-of select="$name"/>
					</xsl:element>
				</xsl:element>
				<xsl:element name="title">
				<xsl:text>&#10;</xsl:text>
				<xsl:text>&#10;</xsl:text>
					<xsl:element name="anchor">
						<xsl:attribute name="id">
							<xsl:value-of select="translate(translate(string(.),' ',''), 'abcdefghijklmnopqrstuvwxyz','ABCDEFGHIJKLMNOPQRSTUVWXYZ')"/>
						</xsl:attribute>
					</xsl:element>
					<xsl:value-of select="."/>
					<xsl:text>&#10;</xsl:text>
				</xsl:element>
				<xsl:element name="variablelist">
					<xsl:element name="varlistentry">
						<xsl:element name="listitem">
							<xsl:element name="para">
								<xsl:text>This parameter is a synonym for </xsl:text>
								<xsl:element name="link">
									<xsl:attribute name="linkend">
										<xsl:value-of select="translate(translate(string($name),' ',''), 'abcdefghijklmnopqrstuvwxyz','ABCDEFGHIJKLMNOPQRSTUVWXYZ')"/>
									</xsl:attribute>
									<xsl:value-of select="$name"/>
								</xsl:element>
								<xsl:text>.</xsl:text>
							</xsl:element>
						</xsl:element>
					</xsl:element>
				</xsl:element>
				<xsl:text>&#10;</xsl:text>     
			</xsl:element>
		</xsl:for-each>

		<xsl:element name="section">
			<xsl:element name="indexterm">
				<xsl:attribute name="significance">
					<xsl:text>preferred</xsl:text>
				</xsl:attribute>
				<xsl:element name="primary">
				<xsl:value-of select="@name"/>
				</xsl:element>
			</xsl:element>
			<xsl:copy-of select="$term"/>
			<xsl:element name="variablelist">
				<xsl:element name="varlistentry">
					<xsl:element name="listitem">
						<xsl:copy-of select="$content"/> <xsl:text>&#10;</xsl:text>
						<xsl:copy-of select="$default"/> <xsl:text>&#10;</xsl:text>
						<xsl:copy-of select="$examples"/> <xsl:text>&#10;</xsl:text>
					</xsl:element>
				</xsl:element>
			</xsl:element>
			<xsl:text>&#10;</xsl:text>
		</xsl:element>
	</xsl:template>
</xsl:stylesheet>
