<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: docbook.xsl,v 1.14 2004/01/04 09:03:25 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
+ ############################################################################## -->

<xsl:stylesheet 
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="docbook" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: docbook.xsl,v 1.14 2004/01/04 09:03:25 j-devenish Exp $
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
		<title>DocBook Driver <filename>docbook.xsl</filename></title>
		<partintro>
			<para>
				
				This is the <quote>parent</quote> stylesheet. The used
				<quote>modules</quote> are included here. The output encoding
				is <literal>text</literal> in ISO-8859-1, indented. Parameters
				may be found in
				<!--
				<link linkend="vars">vars.mod.xsl</link>,
				-->
				<link linkend="param-common">param-common.xsl</link>,
				<link linkend="param-switch">param-switch.xsl</link> and
				<link linkend="param-direct">param-direct.xsl</link>. Users
				may use this stylesheets directory or may construct their own
				<quote>customisation layer</quote> that uses
				<literal>xsl:import</literal>. Users may also override the
				<link linkend="latex.mapping">default &latex; mappings</link>.
				
			</para>
			<para>
				
				An <literal>id</literal> XSLT key is defined for all elements,
				matching their <sgmltag class="attribute">id</sgmltag>
				attribute, to work around <productname>xsltproc</productname>
				bugs.
				
			</para>
		</partintro>
	</doc:reference>

	<xsl:output method="text" encoding="ISO-8859-1" indent="yes"/>
	<xsl:key name="id" match="*" use="@id"/>

	<xsl:include href="common/l10n.xsl"/>
	<xsl:include href="common/common.xsl"/>
	<xsl:include href="common/gentext.xsl"/>
	<xsl:include href="common/subtitles.xsl"/>
	<xsl:include href="common/titles.xsl"/>
	<!--
	<xsl:include href="lib/lib.xsl"/>
	-->

	<xsl:include href="VERSION.xml"/>
	<xsl:include href="param-common.mod.xsl"/>
	<xsl:include href="param-switch.mod.xsl"/>
	<xsl:include href="param-direct.mod.xsl"/>
	<xsl:include href="latex.mapping.xsl"/>
	<xsl:include href="preamble.mod.xsl"/>
	<xsl:include href="labelid.mod.xsl"/>

	<xsl:include href="book-article.mod.xsl"/>

	<xsl:include href="component.mod.xsl"/>

	<xsl:include href="part-chap-app.mod.xsl"/>

	<xsl:include href="sections.mod.xsl"/>
	<xsl:include href="bridgehead.mod.xsl"/>

	<xsl:include href="abstract.mod.xsl"/>
	<xsl:include href="biblio.mod.xsl"/>
	<xsl:include href="revision.mod.xsl"/>

	<xsl:include href="admonition.mod.xsl"/>
	<xsl:include href="verbatim.mod.xsl"/>
	<xsl:include href="email.mod.xsl"/>
	<xsl:include href="sgmltag.mod.xsl"/>
	<xsl:include href="citation.mod.xsl"/>
	<xsl:include href="qandaset.mod.xsl"/>
	<xsl:include href="procedure.mod.xsl"/>
	<xsl:include href="lists.mod.xsl"/>
	<xsl:include href="callout.mod.xsl"/>

	<xsl:include href="figure.mod.xsl"/>
	<xsl:include href="graphic.mod.xsl"/>
	<xsl:include href="mediaobject.mod.xsl"/>

	<xsl:include href="index.mod.xsl"/>

	<xsl:include href="xref.mod.xsl"/>
	<xsl:include href="formal.mod.xsl"/>
	<xsl:include href="example.mod.xsl"/>
	<xsl:include href="table.mod.xsl"/>
	<xsl:include href="inline.mod.xsl"/>
	<xsl:include href="authorgroup.mod.xsl"/>
	<xsl:include href="dingbat.mod.xsl"/>
	<xsl:include href="keywords.mod.xsl"/>
	<xsl:include href="refentry.mod.xsl"/>
	<xsl:include href="glossary.mod.xsl"/>
	<xsl:include href="block.mod.xsl"/>

	<xsl:include href="synop-oop.mod.xsl"/>
	<xsl:include href="synop-struct.mod.xsl"/>

	<xsl:include href="pi.mod.xsl"/>

	<xsl:include href="footnote.mod.xsl"/>

	<xsl:include href="texmath.mod.xsl"/>
	<xsl:include href="mathelem.mod.xsl"/>
	<xsl:include href="mathml/mathml.mod.xsl"/>
	<xsl:include href="mathml/mathml.presentation.mod.xsl"/>
	<xsl:include href="mathml/mathml.content.mod.xsl"/>
	<xsl:include href="mathml/mathml.content.token.mod.xsl"/>
	<xsl:include href="mathml/mathml.content.functions.mod.xsl"/>
	<xsl:include href="mathml/mathml.content.constsymb.mod.xsl"/>

	<xsl:include href="para.mod.xsl"/>
	<xsl:include href="msgset.mod.xsl"/>

	<xsl:include href="normalize-scape.mod.xsl"/>

	<doc:template match="/" xmlns="">
		<refpurpose>Root node</refpurpose>
		<doc:description>
			<para>

				This template begins the conversion of a &docbook; document to
				&latex;.

			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="/">
		<xsl:variable name="xsl-vendor" select="system-property('xsl:vendor')"/>
		<xsl:message>################################################################################</xsl:message>
		<xsl:message> XSLT stylesheets DocBook - LaTeX 2e                                            </xsl:message>
		<xsl:message> Reqs: LaTeX 2e installation common packages                                    </xsl:message>
		<xsl:message>################################################################################</xsl:message>
		<xsl:message> RELEASE : <xsl:value-of select="$VERSION"/>                                    </xsl:message>
		<xsl:message> VERSION : <xsl:value-of select="$CVSVERSION"/>                                 </xsl:message>
		<xsl:message>     TAG : <xsl:value-of select="$TAG"/>                                        </xsl:message>
		<xsl:message>     WWW : http://db2latex.sourceforge.net                                      </xsl:message>
		<xsl:message> SUMMARY : http://www.sourceforge.net/projects/db2latex                         </xsl:message>
		<xsl:message>  AUTHOR : Ramon Casellas  casellas@infres.enst.fr                              </xsl:message>
		<xsl:message>  AUTHOR : James Devenish  j-devenish@users.sf.net                              </xsl:message>
		<xsl:message>   USING : <xsl:call-template name="set-vendor"/>                               </xsl:message>
		<xsl:message>################################################################################</xsl:message>
		<xsl:apply-templates/>
	</xsl:template>

	<!--############################################################################# -->
	<!-- XSL Processor Vendor														  -->
	<!-- XSL Mailing Lists http://www.dpawson.co.uk/xsl/N10378.html					  -->
	<!--############################################################################# -->
	<xsl:template name="set-vendor">
		<xsl:variable name="xsl-vendor" select="system-property('xsl:vendor')"/>
		<xsl:choose>
			<xsl:when test="contains($xsl-vendor, 'SAXON 6.4')">
			<xsl:text>SAXON 6.4.X</xsl:text>
			</xsl:when>
			<xsl:when test="contains($xsl-vendor, 'SAXON 6.2')">
			<xsl:text>SAXON 6.2.X</xsl:text>
			</xsl:when>
			<xsl:when test="starts-with($xsl-vendor,'SAXON')">
			<xsl:text>SAXON</xsl:text>
			</xsl:when>
			<xsl:when test="contains($xsl-vendor,'Apache')">
			<xsl:text>XALAN</xsl:text>
			</xsl:when>
			<xsl:when test="contains($xsl-vendor,'Xalan')">
			<xsl:text>XALAN</xsl:text>
			</xsl:when>
			<xsl:when test="contains($xsl-vendor,'libxslt')">
			<xsl:text>libxslt/xsltproc</xsl:text>
			</xsl:when>
			<xsl:when test="contains($xsl-vendor,'Clark')">
			<xsl:text>XT</xsl:text>
			</xsl:when>
			<xsl:otherwise>
			<xsl:text>UNKNOWN</xsl:text>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

	<doc:template match="*" xmlns="">
		<refpurpose>Catches unhandled elements</refpurpose>
		<doc:description>
			<para>

				This template emits an XSL message when &db2latex; has no
				template for an element that was encountered.

			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="*">
		<xsl:message>DB2LaTeX: Need to process XPath match <xsl:value-of select="concat(name(..),'/',name(.))"/></xsl:message>
		<xsl:text> [</xsl:text><xsl:value-of select="name(.)"/><xsl:text>] &#10;</xsl:text>
		<xsl:apply-templates/> 
		<xsl:text> [/</xsl:text><xsl:value-of select="name(.)"/><xsl:text>] &#10;</xsl:text>
	</xsl:template>

</xsl:stylesheet>

