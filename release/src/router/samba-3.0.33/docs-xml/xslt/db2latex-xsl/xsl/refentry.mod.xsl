<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: refentry.mod.xsl,v 1.7 2004/01/14 14:54:32 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
+ ############################################################################## -->

<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="refentry" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: refentry.mod.xsl,v 1.7 2004/01/14 14:54:32 j-devenish Exp $
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
				<doc:revision rcasver="1.5">&rev_2003_05;</doc:revision>
			</revhistory>
		</referenceinfo>
		<title>References and Entries <filename>refentry.mod.xsl</filename></title>
		<partintro>
			<para>The file <filename>refentry.mod.xsl</filename> contains
			XSL templates for <doc:db basename="reference">references</doc:db>.</para>
		</partintro>
	</doc:reference>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>reference</doc:db> elements</refpurpose>
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
	<xsl:template match="reference">
		<xsl:call-template name="map.begin"/>
		<xsl:call-template name="content-templates"/>
		<xsl:call-template name="map.end"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>refentry</doc:db> elements</refpurpose>
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
	<xsl:template match="refentry">
		<xsl:variable name="refmeta" select=".//refmeta"/>
		<xsl:variable name="refentrytitle" select="$refmeta//refentrytitle"/>
		<xsl:variable name="refnamediv" select=".//refnamediv"/>
		<xsl:variable name="refname" select="$refnamediv//refname"/>
		<xsl:variable name="title">
			<xsl:choose>
			<xsl:when test="$refentrytitle">
				<xsl:apply-templates select="$refentrytitle[1]"/>
			</xsl:when>
			<xsl:when test="$refname">
				<xsl:apply-templates select="$refname[1]"/>
				<xsl:apply-templates select="$refnamediv//refpurpose"/>
			</xsl:when>
			</xsl:choose>
		</xsl:variable>
		<xsl:call-template name="map.begin">
			<xsl:with-param name="string" select="$title"/>
		</xsl:call-template>
		<xsl:call-template name="content-templates"/>
		<xsl:call-template name="map.end">
			<xsl:with-param name="string" select="$title"/>
		</xsl:call-template>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>refentry</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Suppresses <doc:db basename="refentry">refentries</doc:db>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="refmeta"/>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>refentrytitle</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Format a reference entry title.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				This template uses <literal>inline.charseq</literal>,
				though I'm not sure if this should be changed to
				a single <literal>apply-templates</literal>.
			</para>
		</doc:notes>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="template.inline.charseq"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="refentrytitle">
		<xsl:call-template name="inline.charseq"/>
	</xsl:template>

	<!--
	<xsl:template match="refnamediv">
		<xsl:call-template name="block.object"/>
	</xsl:template>
	-->

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>manvolnum</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Format a reference volume number.
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.refentry.xref.manvolnum"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:notes>
			<para>
				If <xref linkend="param.refentry.xref.manvolnum"/> is set,
				this template will apply templates. Otherwise, no output
				is produced.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="manvolnum">
		<xsl:if test="$refentry.xref.manvolnum != 0">
			<xsl:text>(</xsl:text>
			<xsl:apply-templates/>
			<xsl:text>)</xsl:text>
		</xsl:if>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>refnamediv</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Format a reference header.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:description>
			<para>
				Calls <xref linkend="template.block.object"/>.
			</para>
		</doc:description>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="template.block.object"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="refnamediv">
		<xsl:call-template name="block.object"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>refname</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Formats a <doc:db>refname</doc:db> as a start-of-section
				for a <doc:db>refentry</doc:db>.
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.refentry.generate.name"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:notes>
			<para>Will commence a &LaTeX; <function
			condition="latex">section</function> if necessary.</para>
			<para>Designed to accommodate multiple <doc:db>refname</doc:db>s in
			a single <doc:db>refentry</doc:db>.</para>
			<para>The use of a comma between multiple <doc:db>refname</doc:db>s
			should probably be localised.</para>
		</doc:notes>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara>&mapping;</simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="refname">
		<xsl:if test="not (preceding-sibling::refname)">
			<xsl:text>&#10;\section*{</xsl:text>
			<xsl:if test="$refentry.generate.name != 0">
			<xsl:call-template name="gentext.element.name"/>
			</xsl:if>
			<xsl:text>}&#10;</xsl:text>
		</xsl:if>
		<xsl:apply-templates/>
		<xsl:if test="following-sibling::refname">
			<xsl:text>, </xsl:text>
		</xsl:if>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>refpurpose</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Formats a <doc:db>refpurpose</doc:db>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>Outputs an em dash and then applies templates.</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="refpurpose">
		<xsl:text> --- </xsl:text>
		<xsl:apply-templates/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>refdescriptor</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Formats a <doc:db>refdescriptor</doc:db>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>Suppressed.</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="refdescriptor">
		<!-- todo: finish this -->
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>refclass</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Formats a <doc:db>refclass</doc:db>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>Applies templates. If the role attribute is not empty,
			it will be output prior to content, separated by a colon.</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="refclass">
		<xsl:if test="@role!=''">
			<xsl:value-of select="@role"/>
			<xsl:text>: </xsl:text>
		</xsl:if>
		<xsl:apply-templates/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>refsynopsisdiv</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Formats a <doc:db>refsynopsisdiv</doc:db> as an unnumbered subsection.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="refsynopsisdiv">
		<xsl:call-template name="label.id"/>
		<xsl:text>&#10;\subsection*{Synopsis}&#10;</xsl:text>
		<xsl:call-template name="content-templates"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <sgmltag>refsect</sgmltag> elements</refpurpose>
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
	<xsl:template match="refsect1|refsect2|refsect3">
		<xsl:call-template name="map.begin"/>
		<xsl:call-template name="content-templates"/>
		<xsl:call-template name="map.end"/>
	</xsl:template>

</xsl:stylesheet>
