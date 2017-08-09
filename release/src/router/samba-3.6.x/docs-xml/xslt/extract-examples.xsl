<?xml version='1.0'?>
<!--
	Extract examples out of a DocBook/XML file into separate files.
	(C) Jelmer Vernooij	2003

	Published under the GNU GPLv3 or later
-->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:exsl="http://exslt.org/common"
				xmlns:samba="http://www.samba.org/samba/DTD/samba-doc"
		version="1.1"
                extension-element-prefixes="exsl">

<xsl:output method="xml"/>

<xsl:template match="example/title"></xsl:template>

<xsl:template match="example/simplelist/title"></xsl:template>

<!-- Parse all varlistentries and extract those of them which are descriptions of smb.conf
     parameters. We determine them by existence of <anchor> element inside <term> element.
     If <anchor> is there, then its 'id' attribute is translated to lower case and is used
     as basis for file name for that parameter.
-->
<xsl:template match="example">
	<!-- reconstruct varlistentry - not all of them will go into separate files
	and also we must repair the main varlistentry itself.
	-->
	<xsl:variable name="content">
		<xsl:apply-templates/>
	</xsl:variable>
	<!-- Now put varlistentry into separate file _if_ it has anchor associated with it -->
	<xsl:variable name="filename"><xsl:text>examples/</xsl:text><xsl:value-of select="@id"/>.conf</xsl:variable>
	<!-- Debug message for an operator, just to show progress of processing :) -->
	<xsl:message>
		<xsl:text>Writing </xsl:text>
		<xsl:value-of select="$filename"/>
		<xsl:text> for </xsl:text>
		<xsl:value-of select="title"/>
	</xsl:message>
	<!-- Write finally varlistentry to a separate file -->
	<exsl:document href="{$filename}" 
		method="xml" 
		encoding="UTF-8" 
		indent="yes"
		omit-xml-declaration="yes">
		<xsl:copy-of select="$content"/>
	</exsl:document>
</xsl:template>

</xsl:stylesheet>
