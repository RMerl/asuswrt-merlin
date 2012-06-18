<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: keywords.mod.xsl,v 1.7 2004/01/09 12:02:15 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
+ ############################################################################## -->
<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="keywords" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: keywords.mod.xsl,v 1.7 2004/01/09 12:02:15 j-devenish Exp $
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
				<doc:revision rcasver="1.6">&rev_2003_05;</doc:revision>
			</revhistory>
		</referenceinfo>
		<title>Keywords <filename>keywords.mod.xsl</filename></title>
		<partintro>
			<para>The file <filename>keywords.mod.xsl</filename> contains the
			XSL templates for <doc:db>keywordsset</doc:db>
			and <doc:db>sectionset</doc:db>.</para>
		</partintro>
	</doc:reference>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>keywordset</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Uses a &LaTeX; mapping to express a block representation
				of keywords.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>

			<para><doc:db>keywordset</doc:db>s are only rendered in a limited
			number of situations.</para>

		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_book;
			</simplelist>
		</doc:samples>
		<doc:seealso>
			<itemizedlist>
				<listitem><para>&mapping;</para></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>

	<xsl:template match="keywordset">
		<xsl:call-template name="map.begin"/>
		<xsl:call-template name="gentext.template">
			<xsl:with-param name="context" select="'naturalblocklist'"/>
			<xsl:with-param name="name" select="'start'"/>
		</xsl:call-template>
		<xsl:apply-templates/>
		<xsl:call-template name="gentext.template">
			<xsl:with-param name="context" select="'naturalblocklist'"/>
			<xsl:with-param name="name" select="'end'"/>
		</xsl:call-template>
		<xsl:call-template name="map.end"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db basename="keyword">keywords</doc:db></refpurpose>
		<doc:description>
			<para>
				Emits keywords as regular text plus a separator.
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara>Localisation for the <literal>keyword.separator</literal> in the <literal>keywordset</literal> context.</simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:samples>
			<simplelist type='inline'>
				&test_book;
			</simplelist>
		</doc:samples>
	</doc:template>

	<xsl:template match="keyword">
		<xsl:if test="position() &gt; 1">
			<xsl:choose>
				<xsl:when test="position()=last() and position() &gt; 2">
					<xsl:call-template name="gentext.template">
						<xsl:with-param name="context" select="'naturalblocklist'"/>
						<xsl:with-param name="name" select="'lastofmany'"/>
					</xsl:call-template>
				</xsl:when>
				<xsl:when test="position()=last()">
					<xsl:call-template name="gentext.template">
						<xsl:with-param name="context" select="'naturalblocklist'"/>
						<xsl:with-param name="name" select="'lastoftwo'"/>
					</xsl:call-template>
				</xsl:when>
				<xsl:otherwise>
					<xsl:call-template name="gentext.template">
						<xsl:with-param name="context" select="'naturalblocklist'"/>
						<xsl:with-param name="name" select="'middle'"/>
					</xsl:call-template>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:if>
		<xsl:call-template name="inline.charseq"/>
	</xsl:template>

	<doc:template match="subjectset|subject" xmlns="">
		<refpurpose>Process <doc:db>subjectset</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Suppresses <doc:db basename="subjectset">subjectsets</doc:db>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>

	<xsl:template match="subjectset"/>

</xsl:stylesheet>
