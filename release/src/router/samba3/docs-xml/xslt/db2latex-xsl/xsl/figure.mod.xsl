<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: figure.mod.xsl,v 1.17 2004/01/26 09:43:31 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $												
+ ############################################################################## -->

<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="figure" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: figure.mod.xsl,v 1.17 2004/01/26 09:43:31 j-devenish Exp $
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
				<doc:revision rcasver="1.14">&rev_2003_05;</doc:revision>
			</revhistory>
		</referenceinfo>
		<title>Figures and InformalFigures <filename>figure.mod.xsl</filename></title>
		<partintro>
			<para>
			
			<doc:todo>Insert documentation here.</doc:todo>
			
			</para>
		</partintro>
	</doc:reference>

	<doc:template xmlns="">
		<refpurpose> Typeset a caption for a formal figure  </refpurpose>
		<doc:description>
			<para>
			
			Formats a caption, if any, as a centred block.
			
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.figure.caption.style"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.caption.lot.titles.only"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.figure.title.style"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:notes>
			<para>

				The &LaTeX; caption is a combination of <doc:db>title</doc:db>
				and <doc:db>caption</doc:db> children.
				A <link linkend="template.lot">list of figures</link>
				will contain cross-references to these formal figures.

			</para>
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_subfig;
			</simplelist>
		</doc:samples>
	</doc:template>
	<xsl:template name="generate.aux.figure.caption">
		<xsl:text>{</xsl:text>
		<xsl:value-of select="$latex.figure.caption.style"/>
		<xsl:choose>
			<xsl:when test="$latex.caption.lot.titles.only='1'">
				<xsl:text>{\caption[{</xsl:text>
				<xsl:apply-templates select="title"/>
				<xsl:text>}]{{</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text>{\caption{{</xsl:text>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:value-of select="$latex.figure.title.style"/>
		<xsl:text>{</xsl:text>
		<xsl:apply-templates select="title"/>
		<xsl:text>}}</xsl:text>
		<xsl:if test="count(child::mediaobject/caption)=1">
			<xsl:text>. </xsl:text>
			<xsl:apply-templates select="mediaobject/caption" />
		</xsl:if>
		<xsl:text>}</xsl:text>
		<xsl:call-template name="label.id"/>
		<xsl:text>}}&#10;</xsl:text>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose> Process <doc:db>figure</doc:db> elements </refpurpose>
		<doc:description>
			<para>
			
			Formats a formal, <quote>floating</quote> figure with a title and caption.
			The figure may contain multiple subfigures.
			
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.formal.title.placement"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:notes>
			<para>

				If the figure contains only one mediaobject, that mediaobject's
				caption will be used as the figure caption. However, if there
				are multiple meid in the figure, then it will be divided into
				subfigures with their own caption.

			</para>
			<para>

				If a <sgmltag class="attribute">condition</sgmltag> attribute
				exists and begins with <quote>db2latex:</quote>, or a <sgmltag
				class="pi">latex-float-placement</sgmltag> processing
				instruction is present, the remainder of its value will be used
				as the &LaTeX; <quote>float</quote> placement. Otherwise, the
				default placement is <quote>hbt</quote>.

			</para>
			&essential_preamble;
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_subfig;
			</simplelist>
		</doc:samples>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara>&mapping;</simpara></listitem>
				<listitem><simpara><xref linkend="template.generate.aux.figure.caption"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="figure">
		<xsl:variable name="placement">
			<xsl:call-template name="generate.formal.title.placement">
				<xsl:with-param name="object" select="local-name(.)" />
			</xsl:call-template>
		</xsl:variable>
		<xsl:variable name="position">
			<xsl:call-template name="generate.latex.float.position">
				<xsl:with-param name="default" select="'hbt'"/>
			</xsl:call-template>
		</xsl:variable>
		<xsl:call-template name="map.begin">
			<xsl:with-param name="string" select="$position"/>
		</xsl:call-template>
		<xsl:if test="$placement='before'">
			<xsl:text>\captionswapskip{}</xsl:text>
			<xsl:call-template name="generate.aux.figure.caption" />
			<xsl:text>\captionswapskip{}</xsl:text>
		</xsl:if>
		<xsl:apply-templates select="*[name(.) != 'title']"/>
		<xsl:if test="$placement!='before'">
			<xsl:call-template name="generate.aux.figure.caption" />
		</xsl:if>
		<xsl:call-template name="map.end">
			<xsl:with-param name="string" select="$position"/>
		</xsl:call-template>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose> Typeset a caption for an informal figure  </refpurpose>
		<doc:description>
			<para>
			
			Formats a caption, if any, as a centred block.
			
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>

				A caption, if any, is gleaned from the <doc:db>caption</doc:db>
				child of any <doc:db>mediaobject</doc:db> contained within the
				<doc:db>informalfigure</doc:db>, and will be formatted as a
				centred block.

			</para>
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_subfig;
			</simplelist>
		</doc:samples>
	</doc:template>
	<xsl:template name="generate.aux.informalfigure.caption">
		<xsl:if test="count(child::mediaobject/caption)=1">
			<xsl:text>\begin{center}&#10;</xsl:text>
			<xsl:apply-templates select="mediaobject/caption" />
			<xsl:text>\end{center}&#10;</xsl:text>
		</xsl:if>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>informalfigure</doc:db> elements</refpurpose>
		<doc:description>
			<para>
			
			Apply templates for an informal figure.
			
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				The caption is inserted below the informalfigure.
			</para>
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_subfig;
			</simplelist>
		</doc:samples>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara>&mapping;</simpara></listitem>
				<listitem><simpara><xref linkend="template.generate.aux.informalfigure.caption"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="informalfigure">
		<xsl:call-template name="map.begin"/>
		<xsl:apply-templates/>
		<xsl:call-template name="generate.aux.informalfigure.caption" />
		<xsl:call-template name="map.end"/>
	</xsl:template>

	<!--
    <xsl:template match="figure[programlisting]">
	<xsl:call-template name="map.begin">
	    <xsl:with-param name="keyword" select="programlisting"/>
	</xsl:call-template>
	<xsl:apply-templates />
	<xsl:call-template name="map.end">
	    <xsl:with-param name="keyword" select="programlisting"/>
	</xsl:call-template>
    </xsl:template>
	-->

</xsl:stylesheet>
