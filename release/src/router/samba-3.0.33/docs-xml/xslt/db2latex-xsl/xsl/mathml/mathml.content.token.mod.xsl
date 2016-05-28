<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet
[
 <!ENTITY % mmlalias PUBLIC "MathML alias" "ent/mmlalias.ent">  %mmlalias;
 <!ENTITY % mmlextra PUBLIC "MathML extra" "ent/mmlextra.ent">  %mmlextra;
]>
<!--############################################################################# 
 |	$Id: mathml.content.token.mod.xsl,v 1.1.1.1 2003/03/14 10:42:54 rcasellas Exp $
 |- #############################################################################
 |	$Author: rcasellas $												
 |	
 |	PURPOSE: MathML Content Markup, tokens (ci, cn, csymbol)
 |	MathML namespace used -> mml
 + ############################################################################## -->

<xsl:stylesheet version='1.0'
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform" 
	xmlns:mml="http://www.w3.org/1998/Math/MathML" xmlns="http://www.w3.org/1998/Math/MathML">

<xsl:strip-space elements="mml:math mml:mrow"/>


<!-- Content Number --> 
<!-- support for bases and types-->
<xsl:template match="mml:cn">
	<xsl:text>{</xsl:text>
	<xsl:text>{</xsl:text>
      	<xsl:choose>  
      	<xsl:when test="./@type='complex-cartesian' or ./@type='complex'">
        		<mn><xsl:value-of select="text()[position()=1]"/></mn>
			<xsl:choose>
				<xsl:when test="contains(text()[position()=2],'-')">
	  				<xsl:text>-</xsl:text><xsl:value-of select="substring-after(text()[position()=2],'-')"/>
	  			<!--	substring-after does not seem to work well in XT :
					if imaginary part is expressed with at least one space char 
					before the minus sign, then it does not work (we end up with 
					two minus sign since the one in the text is kept)-->
				</xsl:when>
				<xsl:otherwise>
	  				<xsl:text>+</xsl:text> <xsl:value-of select="text()[position()=2]"/>
				</xsl:otherwise>
			</xsl:choose>
			<xsl:text>\dot\textrm{i}</xsl:text>
		</xsl:when>
      	<xsl:when test="./@type='complex-polar'">
        		<xsl:text>\textrm{Polar}(</xsl:text><xsl:value-of select="text()[position()=1]"/><xsl:text>,</xsl:text>
						<xsl:value-of select="text()[position()=2]"/>
        		<xsl:text>)</xsl:text>
		</xsl:when>
		<xsl:when test="./@type='rational'">
        		<xsl:text>\frac{</xsl:text><xsl:value-of select="text()[position()=1]"/><xsl:text>}{</xsl:text>
				<xsl:value-of select="text()[position()=2]"/>
			<xsl:text>}</xsl:text>
		</xsl:when>
		<xsl:otherwise>
			<xsl:value-of select="."/>
		</xsl:otherwise>
	</xsl:choose>
	<xsl:text>}</xsl:text>
	<xsl:if test="@base and @base!=10">  <!-- base specified and different from 10 ; if base = 10 we do not display it -->
			<xsl:text>_{</xsl:text><xsl:value-of select="@base"/><xsl:text>}</xsl:text>
	</xsl:if>
	<xsl:text>}</xsl:text>
</xsl:template>



<!-- Content Identifier --> 
<!-- identifier -->
<!--support for presentation markup-->
<xsl:template match="mml:ci">
<xsl:choose>  
<xsl:when test="./@type='complex-cartesian' or ./@type='complex'">
    	<xsl:choose>
    		<xsl:when test="count(*)>0">  <!--if identifier is composed of real+imag parts-->
			<xsl:text>{</xsl:text>
			<xsl:choose>
				<xsl:when test="*[self::mml:mchar and position()=1]">  <!-- if real part is an mchar -->
          				<xsl:text>\textrm{</xsl:text><xsl:copy-of select="*[position()=1]"/><xsl:text>}</xsl:text>
				</xsl:when>
				<xsl:otherwise>  <!-- if real part is simple text -->
          				<xsl:text>\textrm{</xsl:text><xsl:copy-of select="*[position()=1]"/><xsl:text>}</xsl:text>
        			</xsl:otherwise>
			</xsl:choose>
			<xsl:choose> <!-- im part is negative-->
				<xsl:when test="contains(text()[preceding-sibling::*[position()=1 and self::mml:sep]],'-')">
          				<xsl:text>-\textrm{</xsl:text>
					<xsl:choose>
						<xsl:when test="mml:mchar[preceding-sibling::*[self::mml:sep]]"><!-- if im part is an mchar -->
							<xsl:copy-of select="mml:mchar[preceding-sibling::*[self::mml:sep]]"/>
						</xsl:when>
						<xsl:otherwise><!-- if im part is simple text -->
							<xsl:value-of select="substring-after(text()[preceding-sibling::*[position()=1 and self::mml:sep]],'-')"/>
						</xsl:otherwise>
					</xsl:choose>
					<xsl:text>}</xsl:text>
					<xsl:text>\dot\textrm{i}</xsl:text>
				</xsl:when>
				<xsl:otherwise> <!-- im part is not negative-->
          				<xsl:text>+\textrm{</xsl:text>
					<xsl:choose><!-- if im part is an mchar -->
						<xsl:when test="mml:mchar[preceding-sibling::*[self::mml:sep]]">
							<xsl:copy-of select="mml:mchar[preceding-sibling::*[self::mml:sep]]"/>
						</xsl:when>
						<xsl:otherwise><!-- if im part is simple text -->
							<xsl:value-of select="text()[preceding-sibling::*[position()=1 and self::mml:sep]]"/>
						</xsl:otherwise>
					</xsl:choose>
					<xsl:text>}</xsl:text>
					<xsl:text>\dot\textrm{i}</xsl:text>
				</xsl:otherwise>
			</xsl:choose>
			<xsl:text>}</xsl:text>
		</xsl:when>
		<xsl:otherwise>  <!-- if identifier is composed only of one text child-->
			<xsl:text>\dot\textrm{</xsl:text><xsl:value-of select="."/><xsl:text>}</xsl:text>
		</xsl:otherwise>
	</xsl:choose>
</xsl:when>
<xsl:when test="./@type='complex-polar'">
    <xsl:choose>
    <xsl:when test="count(*)>0">   <!--if identifier is composed of real+imag parts-->
      <xsl:text>{</xsl:text>
        <mi>Polar</mi>
        <mfenced><mi>
        <xsl:value-of select="text()[following-sibling::*[self::mml:sep]]"/>
        <xsl:if test="mml:mchar[following-sibling::*[self::mml:sep]]">
          <xsl:copy-of select="mml:mchar[following-sibling::*[self::mml:sep]]"/>
        </xsl:if>
        </mi>
        <mi>
        <xsl:value-of select="text()[preceding-sibling::*[self::mml:sep]]"/>
        <xsl:if test="mml:mchar[preceding-sibling::*[self::mml:sep]]">
          <xsl:copy-of select="mml:mchar[preceding-sibling::*[self::mml:sep]]"/>
        </xsl:if>
        </mi></mfenced>
      <xsl:text>}</xsl:text>
    </xsl:when>
    <xsl:otherwise>   <!-- if identifier is composed only of one text child-->
      <mi><xsl:value-of select="."/></mi>
    </xsl:otherwise>
    </xsl:choose>
</xsl:when> 
<xsl:when test="./@type='rational'">
    <xsl:choose>
    <xsl:when test="count(*)>0"> <!--if identifier is composed of two parts-->
      <xsl:text>{</xsl:text><mi>
      <xsl:value-of select="text()[following-sibling::*[self::mml:sep]]"/>
      <xsl:if test="mml:mchar[following-sibling::*[self::mml:sep]]">
        <xsl:copy-of select="mml:mchar[following-sibling::*[self::mml:sep]]"/>
      </xsl:if>
      </mi>
      <mo>/</mo>
      <mi>
      <xsl:value-of select="text()[preceding-sibling::*[self::mml:sep]]"/>
      <xsl:if test="mml:mchar[preceding-sibling::*[self::mml:sep]]">
        <xsl:copy-of select="mml:mchar[preceding-sibling::*[self::mml:sep]]"/>
      </xsl:if>
      </mi><xsl:text>}</xsl:text>
    </xsl:when>
    <xsl:otherwise>   <!-- if identifier is composed only of one text child-->
      <mi><xsl:value-of select="."/></mi>
    </xsl:otherwise>
    </xsl:choose>
  </xsl:when>
  <xsl:when test="./@type='vector'">
    <mi fontweight="bold">
      <xsl:value-of select="text()"/>
      <xsl:if test="mml:mchar">
        <xsl:copy-of select="mml:mchar"/>
      </xsl:if>
    </mi>
</xsl:when>
  <!-- type 'set' seems to be deprecated (use 4.4.12 instead); besides, there is no easy way to translate set identifiers to chars in ISOMOPF -->
<xsl:otherwise>  <!-- no type attribute provided -->
	<xsl:choose>
		<xsl:when test="mml:mchar"> <!-- test if identifier is expressed using mchar nodes -->
      <mi><xsl:value-of select="text()"/><xsl:copy-of select="mml:mchar"/></mi>
    </xsl:when>
    <xsl:when test="count(node()) != count(text())">
      <!--test if children are not all text nodes, meaning there is markup assumed 
      to be presentation markup (the case where there are mchar nodes has been tested just before)-->
	<xsl:text>{</xsl:text><xsl:copy-of select="child::*"/><xsl:text>}</xsl:text>
    </xsl:when>
    <xsl:otherwise>  <!-- common case -->
      <mi><xsl:value-of select="."/></mi>
    </xsl:otherwise>
    </xsl:choose>
</xsl:otherwise>
</xsl:choose>
</xsl:template>






<!-- externally defined symbols-->
<xsl:template match="mml:apply[mml:csymbol]">
	<xsl:text>{</xsl:text>
		<xsl:apply-templates select="mml:csymbol[position()=1]"/>
	<xsl:text>\left(</xsl:text>
	<xsl:for-each select="child::*[position()!=1]">
		<xsl:apply-templates select="."/>
	</xsl:for-each>
	<xsl:text>\right)</xsl:text>
	<xsl:text>}</xsl:text>
</xsl:template>


<xsl:template match="mml:csymbol">
<xsl:choose>
	<!--test if children are not all text nodes, meaning there is markup assumed to be presentation markup-->
	<!--perhaps it would be sufficient to test if there is more than one node or text node-->
	<xsl:when test="count(node()) != count(text())"> 
		<xsl:text>{</xsl:text> <xsl:copy-of select="child::*"/> <xsl:text>}</xsl:text>
	</xsl:when>
	<xsl:otherwise>
		<xsl:text>\textrm{</xsl:text> <xsl:copy-of select="."/> <xsl:text>}</xsl:text>
	</xsl:otherwise>
</xsl:choose>
</xsl:template>

<xsl:template match="mml:mchar">
  <xsl:copy-of select="."/>
</xsl:template>
<!--
<xsl:template match="mml:mtext">
  <xsl:copy-of select="."/>
</xsl:template>
-->
</xsl:stylesheet>
