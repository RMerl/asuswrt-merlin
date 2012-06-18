<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet
[
 <!ENTITY % mmlalias PUBLIC "MathML alias" "ent/mmlalias.ent">  %mmlalias;
 <!ENTITY % mmlextra PUBLIC "MathML extra" "ent/mmlextra.ent">  %mmlextra;
]>
<!--############################################################################# 
 |	$Id: mathml.content.mod.xsl,v 1.1.1.1 2003/03/14 10:42:54 rcasellas Exp $		
 |- #############################################################################
 |	$Author: rcasellas $												
 |														
 |   PURPOSE: MathML content markup.
 |	Note: these elements are not part of the DocBook DTD. I have extended
 |    the docbook DTD in order to support this tags, so that's why I have these 
 |	templates here.
 |   
 |	MathML namespace used -> mml
 + ############################################################################## -->

<xsl:stylesheet version='1.0'
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform" 
	xmlns:mml="http://www.w3.org/1998/Math/MathML" xmlns="http://www.w3.org/1998/Math/MathML">

<xsl:template match="mml:semantics">
</xsl:template>


<xsl:template match="mml:set|mml:list">
</xsl:template>


<xsl:template match="mml:matrix">
</xsl:template>

<xsl:template match="mml:reln">
</xsl:template>

<xsl:template match="mml:lambda">
</xsl:template>






<!-- Interval -->
<!-- att. closure : open, closed, open-closed, or closed-open, with a default value of closed. -->
<xsl:template match="mml:interval">
<xsl:choose>
	<xsl:when test="@closure = 'open'">		
		<xsl:call-template name="interval.render">
		<xsl:with-param name="node" select="."/>
		<xsl:with-param name="fst">\left( </xsl:with-param>
		<xsl:with-param name="scd">\right) </xsl:with-param>
		</xsl:call-template>
	</xsl:when>
	<xsl:when test="@closure = 'open-closed'">	
		<xsl:call-template name="interval.render">
		<xsl:with-param name="node" select="."/>
		<xsl:with-param name="fst">\left( </xsl:with-param>
		<xsl:with-param name="scd">\right] </xsl:with-param>
		</xsl:call-template>
	</xsl:when>
	<xsl:when test="@closure = 'closed-open'">	
		<xsl:call-template name="interval.render">
		<xsl:with-param name="node" select="."/>
		<xsl:with-param name="fst">\left[ </xsl:with-param>
		<xsl:with-param name="scd">\right) </xsl:with-param>
		</xsl:call-template>
	</xsl:when>
	<xsl:otherwise> 
		<xsl:call-template name="interval.render">
		<xsl:with-param name="node" select="."/>
		<xsl:with-param name="fst">\left[ </xsl:with-param>
		<xsl:with-param name="scd">\right] </xsl:with-param>
		</xsl:call-template>
	</xsl:otherwise>
</xsl:choose>
</xsl:template>

<xsl:template name="interval.render">
<xsl:param name="node"/>
<xsl:param name="fst"/>
<xsl:param name="scd"/>
<xsl:choose>
	<!-- Two real numbers define the interval -->
	<xsl:when test="count(child::*) = 2">
		<xsl:value-of select="$fst"/>
		<xsl:apply-templates select="$node/child::*[1]"/>
		<xsl:text> , </xsl:text>
		<xsl:apply-templates select="$node/child::*[2]"/>
		<xsl:value-of select="$scd"/>
	</xsl:when>
	<!-- A condition defines the interval -->
	<xsl:otherwise> </xsl:otherwise>
</xsl:choose>
</xsl:template>

</xsl:stylesheet>
