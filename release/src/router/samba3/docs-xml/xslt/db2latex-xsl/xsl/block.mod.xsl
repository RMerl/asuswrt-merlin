<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--#############################################################################
|	$Id: block.mod.xsl,v 1.15 2004/01/26 09:44:38 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
+ ############################################################################## -->

<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="block" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: block.mod.xsl,v 1.15 2004/01/26 09:44:38 j-devenish Exp $
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
				<doc:revision rcasver="1.6">&rev_2003_05;</doc:revision>
			</revhistory>
		</referenceinfo>
		<title>Block Objects <filename>block.mod.xsl</filename></title>
		<partintro>
			<para>The file <filename>block.mod.xsl</filename> contains the
			XSL templates for sundry block-formatted components.</para>
		</partintro>
	</doc:reference>

	<doc:template xmlns="">
		<refpurpose>
			Generic handler for interior elements of block-formatted components
		</refpurpose>
		<doc:description>
			<para>
			Applies all templates, excluding from <doc:db>title</doc:db>-related
			and <doc:db>blockinfo</doc:db>-like elements.
			</para>
		</doc:description>
		<doc:params>
			<variablelist>
				<varlistentry>
					<term>info</term>
					<listitem>
						<para>

						The name of the &DocBook; <quote>info</quote>-type
						element for this component. By default, this is equal
						to the name of the component with <literal>info</literal>
						appended. For example: <doc:db>sectioninfo</doc:db>
						for <doc:db>section</doc:db>.

						</para>
					</listitem>
				</varlistentry>
			</variablelist>
		</doc:params>
	</doc:template>

	<xsl:template name="content-templates">
		<xsl:param name="info" select="concat(local-name(.),'info')"/>
		<xsl:apply-templates select="node()[not(self::title or self::subtitle or self::titleabbrev or self::blockinfo or self::docinfo or local-name(.)=$info)]"/>
	</xsl:template>

	<xsl:template name="content-templates-rootid">
		<!--
		<xsl:message>Rootid <xsl:value-of select="$rootid"/></xsl:message>
		<xsl:message>local-name(.) <xsl:value-of select="local-name(.)"/></xsl:message>
		<xsl:message>count(ancestor::*) <xsl:value-of select="count(ancestor::*)"/></xsl:message>
		-->
		<xsl:choose>
			<xsl:when test="$rootid != '' and count(ancestor::*) = 0">
				<xsl:variable name="node" select="key('id', $rootid)"/>
				<xsl:message>count($node) <xsl:value-of select="count($node)"/></xsl:message>
				<xsl:choose>
					<xsl:when test="count($node) = 0">
						<xsl:message terminate="yes">
							<xsl:text>Root ID '</xsl:text>
							<xsl:value-of select="$rootid"/>
							<xsl:text>' not found in document.</xsl:text>
						</xsl:message>
					</xsl:when>
					<xsl:otherwise>
						<xsl:apply-templates select="$node"/>
					</xsl:otherwise>
				</xsl:choose>
			</xsl:when>
			<xsl:otherwise>
				<xsl:call-template name="content-templates"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>
			Generic handler for block-formatted components
		</refpurpose>
		<doc:description>
			<para>
			Calls <xref linkend="template.label.id"/>,
			applies templates for <doc:db>title</doc:db>,
			then applies templates for content elements.
			</para>
		</doc:description>
	</doc:template>

	<xsl:template name="block.object">
		<xsl:call-template name="label.id"/>
		<xsl:apply-templates select="title"/>
		<xsl:text>&#10;</xsl:text>
		<xsl:call-template name="content-templates"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>
			A quotation set off from the main text (not inline)
		</refpurpose>
		<doc:description>
			<para>
			Uses the &LaTeX; <function condition='env'>quote</function> environment.
			If an attribution is present, it will be set at the end.
			</para>
		</doc:description>
		<doc:seealso>
			<itemizedlist>
				<listitem><para><xref linkend="template.attribution-block.attribution"/></para></listitem>
				<listitem><para>&mapping;</para></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>

	<xsl:template match="blockquote">
		<xsl:call-template name="map.begin"/>
		<xsl:apply-templates/>
		<xsl:apply-templates select="attribution" mode="block.attribution"/>
		<xsl:call-template name="map.end"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>
			A short inscription that occurs at the beginning of a section, chapter, or document
		</refpurpose>
		<doc:description>
			<para>
			Uses the &LaTeX; <function condition='env'>quote</function> environment.
			If an attribution is present, it will be set at the end.
			</para>
		</doc:description>
		<doc:seealso>
			<itemizedlist>
				<listitem><para><xref linkend="template.attribution-block.attribution"/></para></listitem>
				<listitem><para>&mapping;</para></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>

	<xsl:template match="epigraph">
		<xsl:call-template name="map.begin"/>
		<xsl:apply-templates/>
		<xsl:apply-templates select="attribution" mode="block.attribution"/>
		<xsl:call-template name="map.end"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>
			This template produces no output
		</refpurpose>
		<doc:description>
			<para>

			The <doc:db>attribution</doc:db> element only occurs within <xref
			linkend="template.blockquote"/> and <xref
			linkend="template.epigraph"/>. However, the templates for those
			elements use a <quote>mode</quote> mechanism. Therefore, this
			template is intentionally suppressed and a replacement exists. See
			<xref linkend="template.attribution-block.attribution"/> instead.

			</para>
		</doc:description>
	</doc:template>

	<xsl:template match="attribution"/>

	<doc:template xmlns="">
		<refpurpose>
			The source of a block quote or epigraph
		</refpurpose>
		<doc:description>
			<para>
			Starts a new line with right-aligned text preceded by an em dash.
			</para>
		</doc:description>
	</doc:template>

	<xsl:template match="attribution" mode="block.attribution">
		<xsl:text>&#10;\hspace*\fill---</xsl:text>
		<xsl:apply-templates/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>
			A block of text that is isolated from the main flow
		</refpurpose>
		<doc:description>
			<para>
			This is formatted as a plain block.
			</para>
		</doc:description>
		<doc:notes>
			<para>
			This template should create sidebars (but it doesn't)!
			</para>
		</doc:notes>
		<doc:seealso>
			<itemizedlist>
				<listitem><para><xref linkend="template.block.object"/></para></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>

	<xsl:template match="sidebar">
		<xsl:call-template name="block.object"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>
			Titles and subtitles
		</refpurpose>
		<doc:description>
			<para>
				Simply applies templates.
			</para>
			<para>

				This is the default template, for <doc:db
				basename="title">title</doc:db> and <doc:db
				basename="subtitle">subtitles</doc:db>. The use of this
				template is controlled by the template for closing elements,
				which will often not apply templates for <sgmltag
				class="starttag">subtitles</sgmltag>. Furthermore, there may be
				templates to match <sgmltag class="starttag">titles</sgmltag>
				in specific contexts (in which case this template will not be
				used).

			</para>
			<para>
				
				This template is also used by &mapping;.
				
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.apply.title.templates"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.apply.title.templates.admonitions"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.formal.title.placement"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.maketitle"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.titlepage.file"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.formalpara.title.style"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.step.title.style"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.book.article.title.style"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.article.title.style"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.procedure.title.style"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.formalpara.title.style"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.figure.title.style"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:seealso>
			<itemizedlist>
				<listitem><para>&mapping;</para></listitem>
				<listitem><para><xref linkend="template.content-templates"/></para></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>

	<xsl:template match="title|subtitle">
		<xsl:apply-templates/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>
			Captions generated from <doc:db>title</doc:db>s
		</refpurpose>
		<doc:description>
			<para>
			Simply applies templates.
			</para>
			<para>
				The formatting of titles in <literal>caption.mode</literal> may
				depend on the enclosing element's template.
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.formalpara.title.style"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.step.title.style"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.book.article.title.style"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.article.title.style"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.procedure.title.style"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.formalpara.title.style"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.figure.title.style"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.formal.title.placement"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.caption.swapskip"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.equation.caption.style"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.example.caption.style"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.figure.caption.style"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.table.caption.style"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:notes>
			<para>
			Since captions may be incorporated into hyperlinks and
			tables of cross references, <quote>anchor</quote>-type
			elements should not be applied when in this mode.
			</para>
		</doc:notes>
	</doc:template>

	<xsl:template match="title|subtitle" mode="caption.mode">
		<xsl:apply-templates/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>
			Acknowledgements in an <doc:db>article</doc:db>
		</refpurpose>
		<doc:description>
			<para>
			This is formatted as a plain block by applying templates
			with leading and trailing blank lines.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:samples>
			<simplelist type='inline'>
				&test_article;
			</simplelist>
		</doc:samples>
	</doc:template>

	<xsl:template match="ackno">
		<xsl:text>&#10;</xsl:text>
		<xsl:apply-templates/>
		<xsl:text>&#10;</xsl:text>
	</xsl:template>

	<doc:template>
		<refpurpose> Interpret a user's placement preferences for certain &LaTeX; floats </refpurpose>
		<doc:notes>
			<para>

				This template should be invoked when the current node is a
				<doc:db>figure</doc:db> or <doc:db>table</doc:db>. If a
				<sgmltag class="attribute">condition</sgmltag> attribute exists
				and begins with <quote>db2latex:</quote>, or a <sgmltag
				class="pi">latex-float-placement</sgmltag> processing
				instruction is present, the remainder of its value will be used
				as the &LaTeX; <quote>float</quote> placement. Otherwise, the
				default placement is determined by the element's template.

			</para>
			<para>

				Currently, this template is used for <doc:db>figure</doc:db>s
				and <doc:db>table</doc:db>s but not <doc:db>example</doc:db>s
				or <doc:db>equation</doc:db>s.

			</para>
		</doc:notes>
	</doc:template>
	<xsl:template name="generate.latex.float.position">
		<xsl:param name="default" select="'hbt'"/>
		<xsl:choose>
			<xsl:when test="processing-instruction('latex-float-placement')">
				<xsl:value-of select="processing-instruction('latex-float-placement')"/>
			</xsl:when>
			<xsl:when test="starts-with(@condition, 'db2latex:')">
				<xsl:value-of select="substring-after(@condition, 'db2latex:')"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="$default"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>
</xsl:stylesheet>

