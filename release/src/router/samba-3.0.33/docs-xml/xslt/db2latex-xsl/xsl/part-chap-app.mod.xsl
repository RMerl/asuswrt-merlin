<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: part-chap-app.mod.xsl,v 1.7 2004/01/18 11:56:29 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
+ ############################################################################## -->

<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="part-chap-app" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: part-chap-app.mod.xsl,v 1.7 2004/01/18 11:56:29 j-devenish Exp $
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
				<doc:revision rcasver="1.4">&rev_2003_05;</doc:revision>
			</revhistory>
		</referenceinfo>
		<title>Parts, Chapters and Appendixes <filename>part-chap-app.mod.xsl</filename></title>
		<partintro>
			<para>
			
			
			
			</para>
		</partintro>
	</doc:reference>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>set</doc:db>, <doc:db>part</doc:db> and <doc:db>chapter</doc:db> elements</refpurpose>
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
	<xsl:template match="set|part|chapter">
		<xsl:call-template name="map.begin"/>
		<xsl:call-template name="content-templates-rootid"/>
		<xsl:call-template name="map.end"/>
	</xsl:template>

	<!--
    <doc:template match="chapter" xmlns="">
	<refpurpose> XSL template for Chapters.</refpurpose>
	<doc:description>
	    <para> This is the main entry point for a <sgmltag class="start">chapter</sgmltag> subtree.
		This template processes any chapter. Outputs <literal>\chapter{title}</literal>, calls 
		templates and apply-templates. Since chapters only apply in books, 
		some assumptions could be done in order to optimize the stylesheet behaviour.</para>

	    <formalpara><title>Remarks and Bugs</title>
		<itemizedlist>
		    <listitem><para> 
			EMPTY templates: chapter/title, 
			chapter/titleabbrev, 
			chapter/subtitle, 
			chapter/docinfo|chapterinfo.</para></listitem>
		</itemizedlist>
	    </formalpara>

	    <formalpara><title>Affected by</title> map. 
	    </formalpara>
	</doc:description>
    </doc:template>
	-->

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>appendix</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Formats appendices.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				For each appendix, mappings are used and content templates are
				applies.
			</para>
			<para>
				For <doc:db>appendix</doc:db> elements that occur as children of
				<doc:db>book</doc:db> or <doc:db>part</doc:db>, the &LaTeX;
				<function condition="latex">chapter</function> command will be
				used. Otherwise, the <function condition="latex">section</function>
				will be used.
			</para>
			<para>
				If an <doc:db>appendix</doc:db> is the first, or the last, then
				the &LaTeX; mappings for appendix-groups will be invoked in
				addition to the mapping for the appendix itself. For
				chapter-level appendices, the <quote>appendices-chapter</quote>
				is used. Otherwise, the <quote>appendices-section</quote>
				mapping is used.
			</para>
		</doc:notes>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara>&mapping;</simpara></listitem>
				<listitem><simpara><xref linkend="template.content-templates"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="appendix">
		<xsl:if test="not (preceding-sibling::appendix)">
			<xsl:text>&#10;</xsl:text>
			<xsl:choose>
				<xsl:when test="local-name(..)='book' or local-name(..)='part'">
					<xsl:text>\newcommand{\dbappendix}[1]{\chapter{#1}}%&#10;</xsl:text>
					<xsl:call-template name="map.begin">
						<xsl:with-param name="keyword">appendices-chapter</xsl:with-param>
					</xsl:call-template>
				</xsl:when>
				<xsl:otherwise>
					<xsl:text>\newcommand{\dbappendix}[1]{\section{#1}}%&#10;</xsl:text>
					<xsl:call-template name="map.begin">
						<xsl:with-param name="keyword">appendices-section</xsl:with-param>
					</xsl:call-template>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:if>
		<xsl:call-template name="map.begin"/>
		<xsl:call-template name="content-templates"/>
		<xsl:call-template name="map.end"/>
		<xsl:if test="not (following-sibling::appendix)">
			<xsl:text>&#10;</xsl:text>
			<xsl:choose>
				<xsl:when test="local-name(..)='book' or local-name(..)='part'">
					<xsl:call-template name="map.end">
						<xsl:with-param name="keyword">appendices-chapter</xsl:with-param>
					</xsl:call-template>
				</xsl:when>
				<xsl:otherwise>
					<xsl:call-template name="map.end">
						<xsl:with-param name="keyword">appendices-section</xsl:with-param>
					</xsl:call-template>
				</xsl:otherwise>
			</xsl:choose>
			<xsl:text>&#10;</xsl:text>
		</xsl:if>
	</xsl:template>

</xsl:stylesheet>

