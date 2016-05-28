<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: graphic.mod.xsl,v 1.5 2004/01/02 07:25:45 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
+ ############################################################################## -->
<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="graphic" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: graphic.mod.xsl,v 1.5 2004/01/02 07:25:45 j-devenish Exp $
			</releaseinfo>
			<authorgroup>
				&ramon;
				&james;
			</authorgroup>
			<copyright>
				<year>2000</year><year>2001</year><year>2002</year><year>2003</year><year>2004</year>
				<holder>Ramon Casellas</holder>
			</copyright>
			<revhistory>
				<doc:revision rcasver="1.12">&rev_2003_05;</doc:revision>
			</revhistory>
		</referenceinfo>
		<title>Graphics <filename>graphic.mod.xsl</filename></title>
		<partintro>
			<para>
			
			
			
			</para>
		</partintro>
	</doc:reference>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>screenshot</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="screenshot">
		<xsl:apply-templates/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>screeninfo</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Suppressed.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="screeninfo"/>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>graphic</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Formats a graphic as a paragraph.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="template.inlinegraphic"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="graphic">
		<xsl:text>&#10;&#10;</xsl:text>
		<xsl:call-template name="inlinegraphic"/>
		<xsl:text>&#10;&#10;</xsl:text>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>inlinegraphic</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Calls &LaTeX; <function
				condition="latex">includegraphics</function>
				with the value of <sgmltag class="attribute">fileref</sgmltag>
				or the URI of <sgmltag class="attribute">entityref</sgmltag>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="inlinegraphic" name="inlinegraphic">
		<xsl:text>\includegraphics{</xsl:text>
		<xsl:choose>
			<xsl:when test="@entityref">
				<xsl:value-of select="unparsed-entity-uri(@entityref)"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="@fileref"/>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:text>}</xsl:text>
	</xsl:template>

</xsl:stylesheet>
