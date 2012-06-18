<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet
[
 <!ENTITY % mmlalias PUBLIC "MathML alias" "ent/mmlalias.ent">  %mmlalias;
 <!ENTITY % mmlextra PUBLIC "MathML extra" "ent/mmlextra.ent">  %mmlextra;
]>
<!--############################################################################# 
 |	$Id: mathml.content.constsymb.mod.xsl,v 1.2 2004/01/18 10:40:17 j-devenish Exp $
 |- #############################################################################
 |	$Author: j-devenish $												
 |														
 |   PURPOSE: MathML content markup, constants and symbols, 4.4.12.
 |	MathML namespace used -> mml
 + ############################################################################## -->

<xsl:stylesheet version='1.0'
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform" 
	xmlns:mml="http://www.w3.org/1998/Math/MathML" xmlns="http://www.w3.org/1998/Math/MathML">

<!-- integer numbers -->
<xsl:template match="mml:integers">
	<xsl:text>\mathbb Z </xsl:text>
</xsl:template>

<!-- real numbers -->
<xsl:template match="mml:reals">
	<xsl:text>\mathbb R </xsl:text>
</xsl:template>

<!-- rational numbers -->
<xsl:template match="mml:rationals">
	<xsl:text>\mathbb Q </xsl:text>
</xsl:template>

<!-- natural numbers -->
<xsl:template match="mml:naturalnumbers">
	<xsl:text>\mathbb N </xsl:text>
</xsl:template>

<!-- complex numbers -->
<xsl:template match="mml:complexes">
	<xsl:text>\mathbb C </xsl:text>
</xsl:template>

<!-- prime numbers -->
<xsl:template match="mml:primes">
	<xsl:text>\mathbb P </xsl:text>
  <mi><xsl:text disable-output-escaping='yes'>&amp;#x1D547;</xsl:text></mi>  <!-- open face P --> <!-- UNICODE char does not work -->
</xsl:template>







<!-- exponential base -->
<xsl:template match="mml:exponentiale">
	<xsl:text>\textrm{e} </xsl:text>
</xsl:template>

<!-- square root of -1 -->
<xsl:template match="mml:imaginaryi">
	<xsl:text>\textrm{i} </xsl:text>
</xsl:template>

<xsl:template match="mml:notanumber">
	<xsl:text>\NaN </xsl:text>
</xsl:template>

<!-- logical constant for truth -->
<xsl:template match="mml:true">
	<xsl:text>true</xsl:text>
</xsl:template>

<!-- logical constant for falsehood -->
<xsl:template match="mml:false">
	<xsl:text>false</xsl:text>
</xsl:template>

<!-- empty set -->
<xsl:template match="mml:emptyset">
	<xsl:text>\empty</xsl:text>
</xsl:template>

<!-- ratio of a circle's circumference to its diameter -->
<xsl:template match="mml:pi">
	<xsl:text>\pi</xsl:text>
</xsl:template>

<!-- Euler's constant -->
<xsl:template match="mml:eulergamma">
	<xsl:text>\Gamma</xsl:text>
</xsl:template>

<!-- Infinity -->
<xsl:template match="mml:infinity">
	<xsl:text>\infty</xsl:text>
</xsl:template>

</xsl:stylesheet>
