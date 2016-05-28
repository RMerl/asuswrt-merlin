<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: formal.mod.xsl,v 1.13 2004/01/03 09:48:34 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
+ ############################################################################## -->
<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="formal" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: formal.mod.xsl,v 1.13 2004/01/03 09:48:34 j-devenish Exp $
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
				<doc:revision rcasver="1.10">&rev_2003_05;</doc:revision>
			</revhistory>
		</referenceinfo>
		<title>Formal Objects <filename>formal.mod.xsl</filename></title>
		<partintro>
			<para>The file <filename>formal.mod.xsl</filename> contains generic
			XSL templates for <quote>formal</quote> (title-bearing, block-style)
			components and <quote>informal</quote> (no-title, block-style)
			components.
			It also contains templates for <doc:db>equation</doc:db>
			and <doc:db>informalequation</doc:db> (which should probably
			be moved to <filename>block.mod.xsl</filename>).</para>
		</partintro>
	</doc:reference>

	<doc:template xmlns="">
		<refpurpose> Typeset a formal object generically </refpurpose>
		<doc:description>
			<para>
				This template formats the current node as a formal object
				by calling <xref linkend="template.formal.object.title"/>
				then <xref linkend="template.content-templates"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="template.formal.object.title"/></simpara></listitem>
				<listitem><simpara><xref linkend="template.content-templates"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template name="formal.object">
		<xsl:call-template name="formal.object.title"/>
		<xsl:call-template name="content-templates"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose> Typeset the title of a formal object generically </refpurpose>
		<doc:description>
			<para>

				Outputs an anchor for cross-references and hyper-links, then
				applies templates in the <literal>title.content</literal> mode
				(supported by <filename>common/common.xsl</filename>)

			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template name="formal.object.title">
		<xsl:param name="title">
			<xsl:apply-templates select="." mode="title.content"/>
		</xsl:param>
		<xsl:call-template name="label.id"/>
		<xsl:copy-of select="$title"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose> Typeset an informal object generically </refpurpose>
		<doc:description>
			<para>

				Outputs an anchor for cross-references and hyper-links, then
				applies all templates.

			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template name="informal.object">
		<xsl:call-template name="label.id"/>
		<xsl:apply-templates/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose> Typeset formal or informal object generically </refpurpose>
		<doc:description>
			<para>

				Chooses whether the current node is a <quote>formal</quote>-
				or <quote>informal</quote>-like object and calls either
				<xref linkend="template.formal.object"/> or
				<xref linkend="template.informal.object"/>.

			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template name="semiformal.object">
		<xsl:choose>
			<xsl:when test="title"><xsl:call-template name="formal.object"/></xsl:when>
			<xsl:otherwise><xsl:call-template name="informal.object"/></xsl:otherwise>
		</xsl:choose>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose> Determine the relative position for a <doc:db>caption</doc:db> </refpurpose>
		<doc:description>
			<para>
				Chooses the position of a caption to be <quote>before</quote>
				or <quote>after</quote> the selected object.
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.formal.title.placement"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:params>
			<variablelist>
				<varlistentry>
					<term>object</term>
					<listitem>
						<para>

						The type of object (i.e. name of the element) to which the caption pertains.

						</para>
					</listitem>
				</varlistentry>
			</variablelist>
		</doc:params>
		<doc:notes>
			<para>

				If <quote>after</quote> has not been specified for the given
				object in <xref linkend="param.formal.title.placement"/>,
				<quote>before</quote> will be used by default.

			</para>
		</doc:notes>
	</doc:template>
	<xsl:template name="generate.formal.title.placement">
		<xsl:param name="object" select="figure" />
		<xsl:variable name="param.placement" select="substring-after(normalize-space($formal.title.placement),concat($object, ' '))"/>
		<xsl:choose>
			<xsl:when test="contains($param.placement, ' ')">
				<xsl:value-of select="substring-before($param.placement, ' ')"/>
			</xsl:when>
			<xsl:when test="$param.placement = ''">before</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="$param.placement"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

	<!-- ========================================  -->
	<!-- XSL Template for DocBook Equation Element -->
	<!-- 2003/07/04 Applied patches from J.Pavlovic -->
	<!-- ========================================  -->
	<doc:template xmlns="">
		<refpurpose>Process <doc:db>equation</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Formats an titled, block-style equation.
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.equation.caption.style"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.alt.is.preferred"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:notes>
			<para>

				If an <doc:db>informalequation</doc:db> is present, it will be
				typeset in preference to any other elements. Otherwise, this
				template will follow the same logic as the <xref
				linkend="template.informalequation"/> template.

			</para>
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_book;
			</simplelist>
		</doc:samples>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara>&mapping;</simpara></listitem>
				<listitem><simpara><xref linkend="template.generate.formal.title.placement"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="equation">
		<!-- Equation title placement -->
		<xsl:variable name="placement">
			<xsl:call-template name="generate.formal.title.placement">
				<xsl:with-param name="object" select="local-name(.)" />
			</xsl:call-template>
		</xsl:variable>
		<!-- Equation caption -->
		<xsl:variable name="caption">
			<xsl:text>{</xsl:text>
			<xsl:value-of select="$latex.equation.caption.style"/>
			<xsl:text>{\caption{</xsl:text>
			<xsl:apply-templates select="title" mode="caption.mode"/>
			<xsl:text>}</xsl:text>
			<xsl:call-template name="label.id"/>
			<xsl:text>}}&#10;</xsl:text>
		</xsl:variable>
		<xsl:call-template name="map.begin"/>
		<xsl:if test="$placement='before'">
			<xsl:text>\captionswapskip{}</xsl:text>
			<xsl:value-of select="$caption" />
			<xsl:text>\captionswapskip{}</xsl:text>
		</xsl:if>
		<xsl:choose>
			<xsl:when test="informalequation">
				<xsl:apply-templates select="informalequation"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:variable name="tex" select="alt[@role='tex' or @role='latex']|mediaobject/textobject[@role='tex' or @role='latex']|mediaobject/textobject/phrase[@role='tex' or @role='latex']"/>
				<xsl:choose>
					<xsl:when test="$tex">
						<xsl:apply-templates select="$tex"/>
					</xsl:when>
					<xsl:when test="alt and $latex.alt.is.preferred='1'">
						<xsl:apply-templates select="alt"/>
					</xsl:when>
					<xsl:when test="mediaobject">
						<xsl:apply-templates select="mediaobject"/>
					</xsl:when>
					<xsl:when test="alt">
						<xsl:apply-templates select="alt"/>
					</xsl:when>
					<xsl:otherwise>
						<xsl:apply-templates select="graphic"/>
					</xsl:otherwise>
				</xsl:choose>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:if test="$placement!='before'"><xsl:value-of select="$caption" /></xsl:if>
		<xsl:call-template name="map.end"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>informalequation</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Formats an untitled, block-style equation.
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.alt.is.preferred"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:notes>
			<para>

				The <quote>equation</quote> will be found by searching the
				<sgmltag>informalequation</sgmltag> for its <quote>most
				appropriate</quote> child elements. Firstly, this template will
				search for all <doc:db>alt</doc:db>, <doc:db
				basename="textobject">mediaobject/textobject</doc:db> or
				<doc:db
				basename="phrase">mediaobject/textobject/phrase</doc:db>
				children that have a &roleattr; of <quote>latex</quote> or
				<quote>tex</quote>. If none were found, the template will
				search for generic <doc:db>mediaobject</doc:db> or
				<doc:db>alt</doc:db> children. If <xref
				linkend="param.latex.alt.is.preferred"/> is set, <sgmltag
				basename="alt">alts</sgmltag> will be preferred over <sgmltag
				basename="mediaobject">mediaobjects</sgmltag>. If none of these
				elements was found, the template will format any
				<doc:db>graphic</doc:db> children.

			</para>
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_book;
				<!--
				&test_pavlov;
				-->
			</simplelist>
		</doc:samples>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara>&mapping;</simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="informalequation">
		<xsl:variable name="tex" select="alt[@role='tex' or @role='latex']|mediaobject/textobject[@role='tex' or @role='latex']|mediaobject/textobject/phrase[@role='tex' or @role='latex']"/>
		<xsl:text>&#10;</xsl:text>
		<xsl:choose>
			<xsl:when test="$tex">
				<xsl:apply-templates select="$tex"/>
			</xsl:when>
			<xsl:when test="alt and $latex.alt.is.preferred='1'">
				<xsl:apply-templates select="alt"/>
			</xsl:when>
			<xsl:when test="mediaobject">
				<xsl:apply-templates select="mediaobject"/>
			</xsl:when>
			<xsl:when test="alt">
				<xsl:apply-templates select="alt"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:apply-templates select="graphic"/>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:text>&#10;&#10;</xsl:text>
	</xsl:template>

</xsl:stylesheet>
