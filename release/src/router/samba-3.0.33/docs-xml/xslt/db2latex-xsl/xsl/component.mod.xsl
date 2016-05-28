<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: component.mod.xsl,v 1.6 2004/01/03 12:17:59 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
+ ############################################################################## -->

<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="component" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: component.mod.xsl,v 1.6 2004/01/03 12:17:59 j-devenish Exp $
			</releaseinfo>
			<authorgroup>
				&ramon;
				&james;
			</authorgroup>
			<copyright>
				<year>2000</year><year>2001</year><year>2002</year><year>2003</year>
				<holder>Ramon Casellas</holder>
			</copyright>
			<revhistory>
				<doc:revision rcasver="1.3">&rev_2003_05;</doc:revision>
			</revhistory>
		</referenceinfo>
		<title>Chapter-level Components <filename>component.mod.xsl</filename></title>
		<partintro>
			<para>The file <filename>component.mod.xsl</filename> contains
			XSL templates for a number of chapter-level components.</para>
		</partintro>
	</doc:reference>

	<!--
	<xsl:template name="component.title">
		<xsl:variable name="id">
			<xsl:call-template name="label.id"><xsl:with-param name="object" select="."/></xsl:call-template>
		</xsl:variable>
		<xsl:text>&#10;{\sc </xsl:text><xsl:apply-templates select="." mode="title.ref"/><xsl:text>}</xsl:text>
	</xsl:template>

	<xsl:template name="component.subtitle">
		<xsl:variable name="subtitle"><xsl:apply-templates select="." mode="subtitle.content"/></xsl:variable>
		<xsl:if test="$subtitle != ''">
			<xsl:text>&#10;{\sc </xsl:text><xsl:copy-of select="$subtitle"/><xsl:text>}</xsl:text>
		</xsl:if>
	</xsl:template>

	<xsl:template name="component.separator"/>
	-->

	<doc:template xmlns="">
		<refpurpose>Process some simply chapter-level components</refpurpose>
		<doc:description>
			<para>
				Invokes the mapping templates and applies content templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara>&mapping;</simpara></listitem>
				<listitem><simpara><xref linkend="template.content-templates"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="dedication|colophon|preface|partintro">
		<xsl:call-template name="map.begin"/>
		<xsl:call-template name="content-templates"/>
		<xsl:call-template name="map.end"/>
	</xsl:template>

	<!--
	<xsl:template match="colophon">
		<xsl:call-template name="label.id"/>
		<xsl:text>&#10;{\sc </xsl:text><xsl:apply-templates select="." mode="title.ref"/><xsl:text>}</xsl:text>
		<xsl:if test="subtitle">
			<xsl:text>&#10;{\sc </xsl:text>
			<xsl:apply-templates select="subtitle"/>
			<xsl:text>}</xsl:text>
		</xsl:if>
		<xsl:call-template name="content-templates"/>
	</xsl:template>
	-->

</xsl:stylesheet>

