<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: sgmltag.mod.xsl,v 1.5 2003/12/31 13:18:04 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
+ ############################################################################## -->

<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="sgmltag" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: sgmltag.mod.xsl,v 1.5 2003/12/31 13:18:04 j-devenish Exp $
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
				<doc:revision rcasver="1.4">&rev_2003_05;</doc:revision>
			</revhistory>
		</referenceinfo>
		<title><acronym>SGML</acronym> Tags <filename>sgmltag.mod.xsl</filename></title>
		<partintro>
			<para>The file <filename>sgmltag.mod.xsl</filename> contains the
			XSL template for <doc:db>sgmltag</doc:db>.</para>
		</partintro>
	</doc:reference>

	<doc:template xmlns="">
		<refpurpose> Process <doc:db>sgmltag</doc:db> elements </refpurpose>
		<doc:description>
			<para>

				Expresses the element using inline sequences plus any necessary
				punctuation. Some classes are formatted as monospace text.

			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				Unknown classes are emitted without special formatting.
			</para>
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_book;
			</simplelist>
		</doc:samples>
	</doc:template>

	<xsl:template match="sgmltag">
		<xsl:param name="class">
			<xsl:choose>
				<xsl:when test="@class">
					<xsl:value-of select="normalize-space(@class)"/>
				</xsl:when>
				<xsl:otherwise>element</xsl:otherwise>
			</xsl:choose>
		</xsl:param>
		<xsl:choose>
			<xsl:when test="$class='attribute'">
				<xsl:call-template name="inline.charseq"/>
			</xsl:when>
			<xsl:when test="$class='attvalue'">
				<xsl:call-template name="inline.monoseq"/>
			</xsl:when>
			<xsl:when test="$class='element'">
				<xsl:call-template name="inline.monoseq"/>
			</xsl:when>
			<xsl:when test="$class='endtag'">
				<xsl:call-template name="inline.monoseq">
					<xsl:with-param name="content">
						<xsl:text>&lt;/</xsl:text>
						<xsl:apply-templates/>
						<xsl:text>&gt;</xsl:text>
					</xsl:with-param>
				</xsl:call-template>
			</xsl:when>
			<xsl:when test="$class='genentity'">
				<xsl:call-template name="inline.monoseq">
					<xsl:with-param name="content">
						<xsl:text>\&amp;</xsl:text>
						<xsl:apply-templates/>
						<xsl:text>;</xsl:text>
					</xsl:with-param>
				</xsl:call-template>
			</xsl:when>
			<xsl:when test="$class='numcharref'">
				<xsl:call-template name="inline.monoseq">
					<xsl:with-param name="content">
						<xsl:text>\&amp;\#</xsl:text>
						<xsl:apply-templates/>
						<xsl:text>;</xsl:text>
					</xsl:with-param>
				</xsl:call-template>
			</xsl:when>
			<xsl:when test="$class='paramentity'">
				<xsl:call-template name="inline.monoseq">
					<xsl:with-param name="content">
						<xsl:text>\%</xsl:text>
						<xsl:apply-templates/>
						<xsl:text>;</xsl:text>
					</xsl:with-param>
				</xsl:call-template>
			</xsl:when>
			<xsl:when test="$class='pi'">
				<xsl:call-template name="inline.monoseq">
					<xsl:with-param name="content">
						<xsl:text>&lt;?</xsl:text>
						<xsl:apply-templates/>
						<xsl:text>?&gt;</xsl:text>
					</xsl:with-param>
				</xsl:call-template>
			</xsl:when>
			<xsl:when test="$class='xmlpi'">
				<xsl:call-template name="inline.monoseq">
					<xsl:with-param name="content">
						<xsl:text>&lt;?</xsl:text>
						<xsl:apply-templates/>
						<xsl:text>?&gt;</xsl:text>
					</xsl:with-param>
				</xsl:call-template>
			</xsl:when>
			<xsl:when test="$class='starttag'">
				<xsl:call-template name="inline.monoseq">
					<xsl:with-param name="content">
						<xsl:text>&lt;</xsl:text>
						<xsl:apply-templates/>
						<xsl:text>&gt;</xsl:text>
					</xsl:with-param>
				</xsl:call-template>
			</xsl:when>
			<xsl:when test="$class='emptytag'">
				<xsl:call-template name="inline.monoseq">
					<xsl:with-param name="content">
						<xsl:text>&lt;</xsl:text>
						<xsl:apply-templates/>
						<xsl:text>/&gt;</xsl:text>
					</xsl:with-param>
				</xsl:call-template>
			</xsl:when>
			<xsl:when test="$class='sgmlcomment'">
				<xsl:call-template name="inline.monoseq">
					<xsl:with-param name="content">
						<xsl:text>&lt;!--</xsl:text>
						<xsl:apply-templates/>
						<xsl:text>--&gt;</xsl:text>
					</xsl:with-param>
				</xsl:call-template>
			</xsl:when>
			<xsl:otherwise>
				<xsl:call-template name="inline.charseq"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

</xsl:stylesheet>

