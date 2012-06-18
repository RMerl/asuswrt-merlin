<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: dingbat.mod.xsl,v 1.4 2004/01/02 05:11:38 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
+ ############################################################################## -->

<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="dingbat" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: dingbat.mod.xsl,v 1.4 2004/01/02 05:11:38 j-devenish Exp $
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
		<title>Dingbats <filename>dingbat.mod.xsl</filename></title>
		<partintro>
			<para>
			
			Provides some named dingbats. These should probably be incorporated
			into the normal localisation mechanism, in future.
			
			</para>
		</partintro>
	</doc:reference>

	<doc:template xmlns="">
		<refpurpose> Generate a &LaTeX; dingbat </refpurpose>
		<doc:description>
			<para>
			
			Chooses a &LaTeX; sequence based on the requested dingbat name.
			
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:params>
			<variablelist>
				<varlistentry>
					<term>dingbat</term>
					<listitem><simpara>The name of the dingbat.</simpara></listitem>
				</varlistentry>
			</variablelist>
		</doc:params>
	</doc:template>
	<xsl:template name="dingbat">
		<xsl:param name="dingbat">bullet</xsl:param>
		<xsl:choose>
			<xsl:when test="$dingbat='bullet'"> $\bullet$ </xsl:when>
			<xsl:when test="$dingbat='copyright'">\copyright{}</xsl:when>
			<xsl:when test="$dingbat='trademark'">\texttrademark{}</xsl:when>
			<xsl:when test="$dingbat='registered'">\textregistered{}</xsl:when>
			<xsl:when test="$dingbat='nbsp'">~</xsl:when>
			<xsl:when test="$dingbat='ldquo'">``</xsl:when>
			<xsl:when test="$dingbat='rdquo'">''</xsl:when>
			<xsl:when test="$dingbat='lsquo'">`</xsl:when>
			<xsl:when test="$dingbat='rsquo'">'</xsl:when>
			<xsl:when test="$dingbat='em-dash'">---</xsl:when>
			<xsl:when test="$dingbat='mdash'">---</xsl:when>
			<xsl:when test="$dingbat='en-dash'">--</xsl:when>
			<xsl:when test="$dingbat='ndash'">--</xsl:when>
			<xsl:otherwise>
			<xsl:text> [dingbat?] </xsl:text>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

</xsl:stylesheet>
