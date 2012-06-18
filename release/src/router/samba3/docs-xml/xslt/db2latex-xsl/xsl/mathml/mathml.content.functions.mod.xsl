<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet
[
 <!ENTITY % mmlalias PUBLIC "MathML alias" "ent/mmlalias.ent">  %mmlalias;
 <!ENTITY % mmlextra PUBLIC "MathML extra" "ent/mmlextra.ent">  %mmlextra;
]>
<!--############################################################################# 
 |	$Id: mathml.content.functions.mod.xsl,v 1.2 2004/01/18 10:45:35 j-devenish Exp $		
 |- #############################################################################
 |	$Author: j-devenish $												
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

<xsl:template match="mml:fn">
</xsl:template>

<!--
<xsl:template match="mml:apply/lowlimit">
<xsl:variable name="first" select="../child::*[1]"/>
</xsl:template>

<xsl:template match="mml:apply/uplimit">
<xsl:variable name="first" select="../child::*[1]"/>
</xsl:template>

<xsl:template match="mml:apply/degree">
<xsl:variable name="first" select="../child::*[1]"/>
</xsl:template>
-->


<!-- conditions -->
<!-- no support for deprecated reln-->
<xsl:template match="mml:condition">
	<xsl:text>{</xsl:text><xsl:apply-templates/><xsl:text>}</xsl:text>
</xsl:template>






<!--UNARY 
	  unary arithmetic			exp, factorial, minus, abs, conjugate, arg, real, imaginary
        unary logical				not
        unary functional			inverse, ident
        unary elementary classical
        functions					sin, cos, tan, sec, csc, cot, sinh, cosh, tanh, sech, csch, coth, arcsin,
                                          arccos, arctan, arccosh, arccot, arccoth, arccsc, arccsch, arcsec, arcsech, arcsinh,
                                          arctanh, exp, ln, log
        unary linear algebra			determinant, transpose
        unary calculus and vector calculus
                                          divergence, grad, curl, laplacian
        unary set-theoretic			card
-->

<!-- BINARY
	  binary arithmetic 			quotient, divide, minus, power, rem
        binary logical				implies, equivalent, approx
        binary set operators			setdiff
        binary linear algebra			vectorproduct, scalarproduct, outerproduct
-->

<!-- N-ARY and OTHER
 	 n-ary statistical mean, sdev, variance, median, mode
        n-ary logical and, or, xor
        n-ary linear algebra selector
        n-ary set operator union, intersect
        n-ary functional fn, compose
        integral, sum, product operators int, sum, product
        differential operator diff, partialdiff
        quantifier forall, exists
-->
<!-- Get the first child (operator), and check if its name is a fn , or operator-->
<!-- Is the operator taking qualifiers? -->
<!-- Operators  : int, sum, product, root, diff, partialdiff, limit, log, moment, min, max, forall, exists -->
<!-- Qualifiers : lowlimit, uplimit, bvar, degree, logbase, interval, condition  -->


<!-- apply/apply -->
<xsl:template match="mml:apply[mml:apply]">  <!-- when the function itself is defined by other functions: (F+G)(x) -->
	<xsl:choose>
		<xsl:when test="count(child::*)>=2">
			<xsl:text>{</xsl:text><xsl:apply-templates select="child::*[position()=1]"/><xsl:text>}</xsl:text>	
			<xsl:text>{</xsl:text><xsl:apply-templates select="child::*[position()!=1]"/><xsl:text>}</xsl:text>	
		</xsl:when>
	<xsl:otherwise> <!-- apply only contains apply, no operand-->
			<xsl:text>{</xsl:text><xsl:apply-templates select="child::*"/><xsl:text>}</xsl:text>	
	</xsl:otherwise>
	</xsl:choose>
</xsl:template>


<!-- force function or operator MathML 1.0 deprecated-->
<!-- partial support for func/operators defined using presentation markup-->
<xsl:template match="mml:apply[mml:fn]">
<mrow>
<xsl:choose>
<xsl:when test="*[position()=1 and self::mml:fn]/mml:mo/mml:mchar/@name='PlusMinus'">
  <!--if operator is infix (we assume this to be the default when we have mchars(for instance PlusMinus); perhaps we should test further the name attribute)-->
  <xsl:choose>
  <xsl:when test="count(child::*)>=3">
    <mrow>
      <xsl:for-each select="child::*[position()!=last() and  position()!=1]">
        <xsl:apply-templates select="."/><xsl:copy-of select="preceding-sibling::mml:fn/*"/>
      </xsl:for-each>
      <xsl:apply-templates select="child::*[position()!=1 and position()=last()]"/>
    </mrow>
  </xsl:when>
  <xsl:when test="count(child::*)=2">
    <mrow><xsl:copy-of select="child::mml:fn[position()=1]/*"/><xsl:apply-templates select="child::*[position()=2]"/></mrow>
  </xsl:when>
  <xsl:otherwise> <!-- apply only contains fn, no operand-->
    <mrow><xsl:apply-templates select="child::mml:fn/*"/></mrow>
  </xsl:otherwise>
  </xsl:choose>
</xsl:when>  
<xsl:otherwise>  <!-- if operator is prefix (common case)-->
  <xsl:choose>
    <xsl:when test="name(mml:fn/*[position()=1])='apply'"> <!-- fn definition is complex, surround with brackets, but only one child-->
      <mfenced separators=""><mrow><xsl:apply-templates select="mml:fn/*"/></mrow></mfenced>
    </xsl:when>
    <xsl:otherwise>
      <mi><xsl:apply-templates select="mml:fn/*"/></mi>
    </xsl:otherwise>
  </xsl:choose>
  <xsl:if test="count(*)>1">  <!-- if no operands, don't put empty parentheses-->
    <mo><mchar name="ApplyFunction"/></mo>
    <mfenced>
      <xsl:apply-templates select="*[position()!=1]"/>
    </mfenced>
  </xsl:if>
</xsl:otherwise>
</xsl:choose>
</mrow>
</xsl:template>




<!-- quotient -->
<xsl:template match="mml:apply[mml:quotient]">
  <mrow>  <!-- the third notation uses UNICODE chars x0230A and x0230B -->
    <mo>integer part of</mo>
    <mrow>
      <xsl:choose> <!-- surround with brackets if operands are composed-->
      <xsl:when test="child::*[position()=2] and name()='mml:apply'">
        <mfenced separators=""><xsl:apply-templates select="*[position()=2]"/></mfenced>
      </xsl:when>
      <xsl:otherwise>
        <xsl:apply-templates select="*[position()=2]"/>
      </xsl:otherwise>
      </xsl:choose>
      <mo>/</mo>
      <xsl:choose>
      <xsl:when test="child::*[position()=3] and name()='mml:apply'">
        <mfenced separators=""><xsl:apply-templates select="*[position()=3]"/></mfenced>
      </xsl:when>
      <xsl:otherwise>
        <xsl:apply-templates select="*[position()=3]"/>
      </xsl:otherwise>
      </xsl:choose>
    </mrow>
  </mrow>
</xsl:template>


<!-- factorial -->
<xsl:template match="mml:apply[mml:factorial]">
	<xsl:text>{</xsl:text>
	<xsl:choose> 
		<xsl:when test="name(*[position()=2])='mml:apply'">
			<xsl:text>{</xsl:text><xsl:apply-templates select="*[position()=2]"/><xsl:text>}</xsl:text>
		</xsl:when>
		<xsl:otherwise>
			<xsl:apply-templates select="*[position()=2]"/>
		</xsl:otherwise>
	</xsl:choose>
	<xsl:text>!(fact)</xsl:text>
	<xsl:text>}</xsl:text>
</xsl:template>


<!-- divide -->
<xsl:template match="mml:apply[mml:divide]">
	<xsl:text>{ \frac</xsl:text>
	<xsl:text>{ </xsl:text>
	<xsl:apply-templates select="child::*[position()=2]"/>
	<xsl:text>}</xsl:text>
	<xsl:text>{ </xsl:text>
	<xsl:apply-templates select="child::*[position()=3]"/>
	<xsl:text>}</xsl:text>
	<xsl:text>}</xsl:text>
</xsl:template>


<!-- APPLY CONTAINING MAX	-->
<xsl:template match="mml:apply/mml:max"/>
<xsl:template match="mml:apply[mml:max]">
	<xsl:text>{</xsl:text>
	<xsl:choose>
		<xsl:when test="mml:bvar"> <!-- if there are bvars-->
			<xsl:text>\max_{</xsl:text>
		<!-- Select every bvar except the last one (position() only counts bvars, not the other siblings)-->
			<xsl:for-each select="mml:bvar[position()!=last()]">  
            		<xsl:apply-templates select="."/><xsl:text>,</xsl:text>
			</xsl:for-each>
			<xsl:apply-templates select="mml:bvar[position()=last()]"/>
			<xsl:text>}</xsl:text>
		</xsl:when>
		<xsl:otherwise> <!-- No bvars, no underscore... -->
			<xsl:text>\max</xsl:text>
		</xsl:otherwise>
	</xsl:choose>
	<xsl:text>\left\{</xsl:text>
     	<xsl:for-each select="child::*[name()!='mml:condition' and 	name()!='mml:bvar' and 	name()!='mml:max' and 	position()!=last()]">
     		<xsl:apply-templates select="."/><xsl:text>,</xsl:text>
		<xsl:message>RCAS: MathML mml:apply[mml:min] Applying templates to <xsl:copy-of select="name(.)"/></xsl:message>
	</xsl:for-each>
     	<xsl:apply-templates select="child::*[name()!='mml:condition' and	name()!='mml:bvar' and 	name()!='mml:max' and 	position()=last()]"/>
	<!-- If there is a condition, do not close... -->	
	<xsl:if test="mml:condition">
     		<xsl:text>|</xsl:text><xsl:apply-templates select="mml:condition"/>
	</xsl:if>
     	<xsl:text>\right\}</xsl:text>
	<xsl:text>}</xsl:text>
</xsl:template>


<!-- APPLY CONTAINING MIN	-->
<xsl:template match="mml:apply/mml:min"/>
<xsl:template match="mml:apply[mml:min]">
	<xsl:text>{</xsl:text>
	<xsl:choose>
		<xsl:when test="mml:bvar"> <!-- if there are bvars-->
			<xsl:text>\min_{</xsl:text>
			<!-- Select every bvar except the last one (position() only counts bvars, not the other siblings)-->
			<xsl:for-each select="mml:bvar[position()!=last()]">  
            		<xsl:apply-templates select="."/><xsl:text>,</xsl:text>
			</xsl:for-each>
			<xsl:apply-templates select="mml:bvar[position()=last()]"/>
			<xsl:text>}</xsl:text>
		</xsl:when>
		<xsl:otherwise> <!-- No bvars, no underscore... -->
			<xsl:text>\min</xsl:text>
		</xsl:otherwise>
	</xsl:choose>
	<xsl:text>\left\{</xsl:text>
     	<xsl:for-each select="child::*[name()!='mml:condition' and 	name()!='mml:bvar' and 	name()!='mml:min' and 	position()!=last()]">
		<xsl:message>RCAS: MathML mml:apply[mml:min] Applying templates to <xsl:copy-of select="name(.)"/></xsl:message>
     		<xsl:apply-templates select="."/>
		<xsl:text>,</xsl:text>
	</xsl:for-each>
     	<xsl:apply-templates select="child::*[name()!='mml:condition' and	name()!='mml:bvar' and 	name()!='mml:min' and 	position()=last()]"/>
	<!-- If there is a condition, do not close... -->	
	<xsl:if test="mml:condition">
     		<xsl:text>|</xsl:text><xsl:apply-templates select="mml:condition"/>
	</xsl:if>
     	<xsl:text>\right\}</xsl:text>
	<xsl:text>}</xsl:text>
</xsl:template>








<!-- APPLY CONTAINING substraction(minus)	-->
<!-- unary or binary operator			-->
<xsl:template match="mml:apply[mml:minus]">
<xsl:text>{</xsl:text>
<xsl:choose> <!-- binary -->
	<xsl:when test="count(child::*)=3">
		<xsl:apply-templates select="child::*[position()=2]"/>
		<xsl:text>-</xsl:text>
		<xsl:choose>
	      	<xsl:when test="((name(*[position()=3])='mml:ci' or name(*[position()=3])='mml:cn') and contains(*[position()=3]/text(),'-')) or (name(*[position()=3])='mml:apply')">
			<xsl:text>\left(</xsl:text> <xsl:apply-templates select="*[position()=3]"/><xsl:text>\right)</xsl:text>
			<!-- surround negative or complex things with brackets -->
			</xsl:when>
	      	<xsl:otherwise>
      	  		<xsl:apply-templates select="*[position()=3]"/>
		      </xsl:otherwise>
		</xsl:choose>
	</xsl:when>
	<xsl:otherwise> <!-- unary -->
		<xsl:text>-</xsl:text>
		<xsl:choose>
    		<xsl:when test=
			"((name(*[position()=2])='mml:ci' or name(*[position()=2])='mml:cn') and contains(*[position()=2]/text(),'-')) or (name(*[position()=2])='mml:apply')">
			<xsl:text>\left(</xsl:text>
				<xsl:apply-templates select="child::*[position()=last()]"/>
			<xsl:text>\right)</xsl:text>
		</xsl:when>
		<xsl:otherwise>
			<xsl:apply-templates select="child::*[position()=last()]"/>
		</xsl:otherwise>
		</xsl:choose>
	</xsl:otherwise>
</xsl:choose>
<xsl:text>}</xsl:text>
</xsl:template>


<!-- addition -->
<xsl:template match="mml:apply[mml:plus]">
  <xsl:choose>
  <xsl:when test="count(child::*)>=3">
    <mrow>
      <xsl:choose>
        <xsl:when test="((name(*[position()=2])='mml:ci' or name(*[position()=2])='mml:cn') and contains(*[position()=2]/text(),'-')) or (*[position()=2 and self::mml:apply and child::mml:minus])">
          <mfenced separators=""><xsl:apply-templates select="*[position()=2]"/></mfenced> <!-- surround negative things with brackets -->
        </xsl:when>
        <xsl:otherwise>
          <xsl:apply-templates select="*[position()=2]"/>
        </xsl:otherwise>
      </xsl:choose>
      <xsl:for-each select="child::*[position()!=1 and position()!=2]">
        <xsl:choose>
        <xsl:when test="((name(.)='mml:ci' or name(.)='mml:cn') and contains(./text(),'-')) or (self::mml:apply and child::mml:minus)"> <!-- surround negative things with brackets -->
          <mo>+</mo><mfenced separators=""><xsl:apply-templates select="."/></mfenced>
        </xsl:when>
        <xsl:otherwise>
          <mo>+</mo><xsl:apply-templates select="."/>
        </xsl:otherwise>
        </xsl:choose>
      </xsl:for-each>
    </mrow>
  </xsl:when>
  <xsl:when test="count(child::*)=2">
    <mrow>
      <mo>+</mo><xsl:apply-templates select="child::*[position()=2]"/>
    </mrow>
  </xsl:when>
  <xsl:otherwise>
    <mo>+</mo>
  </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<!-- power -->
<xsl:template match="mml:apply[mml:power]">
	<xsl:text> {</xsl:text>
	<xsl:choose>
		<xsl:when test="name(*[position()=2])='mml:apply'">
			<xsl:text>\left(</xsl:text>
			<xsl:apply-templates select="child::*[position()=2]"/>
			<xsl:text>\rigth)</xsl:text>
		</xsl:when>
		<xsl:otherwise>
			<xsl:apply-templates select="child::*[position()=2]"/>
		</xsl:otherwise>
	</xsl:choose>
	<xsl:text>}^{</xsl:text>
		<xsl:apply-templates select="child::*[position()=3]"/>
	<xsl:text>} </xsl:text>
</xsl:template>

<!-- remainder -->
<xsl:template match="mml:apply[mml:rem]">
  <mrow>
    <xsl:choose> <!-- surround with brackets if operands are composed-->
    <xsl:when test="name(*[position()=2])='mml:apply'">
      <mfenced separators=""><xsl:apply-templates select="*[position()=2]"/></mfenced>
    </xsl:when>
    <xsl:otherwise>
      <xsl:apply-templates select="*[position()=2]"/>
    </xsl:otherwise>
    </xsl:choose>
    <mo>mod</mo>
    <xsl:choose>
    <xsl:when test="name(*[position()=3])='mml:apply'">
      <mfenced separators=""><xsl:apply-templates select="*[position()=3]"/></mfenced>
    </xsl:when>
    <xsl:otherwise>
      <xsl:apply-templates select="*[position()=3]"/>
    </xsl:otherwise>
    </xsl:choose>
  </mrow>
</xsl:template>

<!-- multiplication -->
<xsl:template match="mml:apply[mml:times]">
<xsl:choose>
<xsl:when test="count(child::*)>=3">
  <mrow>
    <xsl:for-each select="child::*[position()!=last() and  position()!=1]">
      <xsl:choose>
      <xsl:when test="mml:plus"> <!--add brackets around + children for priority purpose-->
        <mfenced separators=""><xsl:apply-templates select="."/></mfenced><mo><mchar name="InvisibleTimes"/></mo>
      </xsl:when>
      <xsl:when test="mml:minus"> <!--add brackets around - children for priority purpose-->
        <mfenced separators=""><xsl:apply-templates select="."/></mfenced><mo><mchar name="InvisibleTimes"/></mo>
      </xsl:when>
      <xsl:when test="(name(.)='mml:ci' or name(.)='mml:cn') and contains(text(),'-')"> <!-- have to do it using contains because starts-with doesn't seem to work well in XT-->
        <mfenced separators=""><xsl:apply-templates select="."/></mfenced><mo><mchar name="InvisibleTimes"/></mo>
      </xsl:when>
      <xsl:otherwise>
        <xsl:apply-templates select="."/><mo><mchar name="InvisibleTimes"/></mo>
      </xsl:otherwise>
      </xsl:choose>
    </xsl:for-each>
    <xsl:for-each select="child::*[position()=last()]">
      <xsl:choose>
      <xsl:when test="mml:plus">
        <mfenced separators=""><xsl:apply-templates select="."/></mfenced>
      </xsl:when>
      <xsl:when test="mml:minus">
        <mfenced separators=""><xsl:apply-templates select="."/></mfenced>
      </xsl:when>
      <xsl:when test="(name(.)='mml:ci' or name(.)='mml:cn') and contains(text(),'-')"> <!-- have to do it using contains because starts-with doesn't seem to work well in  XT-->
        <mfenced separators=""><xsl:apply-templates select="."/></mfenced>
      </xsl:when>
      <xsl:otherwise>
        <xsl:apply-templates select="."/>
      </xsl:otherwise>
      </xsl:choose>
    </xsl:for-each>
  </mrow>
</xsl:when>
<xsl:when test="count(child::*)=2">  <!-- unary -->
  <mrow>
    <mo><mchar name="InvisibleTimes"/></mo>
    <xsl:choose>
      <xsl:when test="mml:plus">
        <mfenced separators=""><xsl:apply-templates select="*[position()=2]"/></mfenced>
      </xsl:when>
      <xsl:when test="mml:minus">
        <mfenced separators=""><xsl:apply-templates select="*[position()=2]"/></mfenced>
      </xsl:when>
      <xsl:when test="(*[position()=2 and self::mml:ci] or *[position()=2 and self::mml:cn]) and contains(*[position()=2]/text(),'-')">
        <mfenced separators=""><xsl:apply-templates select="*[position()=2]"/></mfenced>
      </xsl:when>
      <xsl:otherwise>
        <xsl:apply-templates select="*[position()=2]"/>
      </xsl:otherwise>
    </xsl:choose>
  </mrow>
</xsl:when>
<xsl:otherwise>  <!-- no operand -->
  <mo><mchar name="InvisibleTimes"/></mo>
</xsl:otherwise>
</xsl:choose>
</xsl:template>

<!-- root -->
<xsl:template match="mml:apply[mml:root]">
  <xsl:choose>
  <xsl:when test="mml:degree">
    <xsl:choose>
    <xsl:when test="mml:degree/mml:cn/text()='2'"> <!--if degree=2 display a standard square root-->
      <msqrt>
        <xsl:apply-templates select="child::*[position()=3]"/>
      </msqrt>
    </xsl:when>
    <xsl:otherwise>
      <mroot>
        <xsl:apply-templates select="child::*[position()=3]"/>
        <mrow><xsl:apply-templates select="mml:degree/*"/></mrow>
      </mroot>
    </xsl:otherwise>
    </xsl:choose>
  </xsl:when>
  <xsl:otherwise> <!-- no degree specified-->
    <msqrt>
      <xsl:apply-templates select="child::*[position()=2]"/>
    </msqrt>
  </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<!-- greatest common divisor -->
<xsl:template match="mml:apply[mml:gcd]">
  <mrow>
    <mi>gcd</mi><mo><mchar name="ApplyFunction"/></mo>
    <mfenced>
      <xsl:apply-templates select="child::*[position()!=1]"/>
    </mfenced>
  </mrow>
</xsl:template>

<!-- AND -->
<xsl:template match="mml:apply[mml:and]">
<mrow>
  <xsl:choose>
  <xsl:when test="count(*)>=3"> <!-- at least two operands (common case)-->
    <xsl:for-each select="child::*[position()!=last() and  position()!=1]">
      <xsl:choose>
      <xsl:when test="mml:or"> <!--add brackets around OR children for priority purpose-->
        <mfenced separators=""><xsl:apply-templates select="."/></mfenced><mo><mchar name="And"/></mo>
      </xsl:when>
      <xsl:when test="mml:xor"> <!--add brackets around XOR children for priority purpose-->
       <mfenced separators=""><xsl:apply-templates select="."/></mfenced><mo><mchar name="And"/></mo> 
      </xsl:when>
      <xsl:otherwise>
        <xsl:apply-templates select="."/><mo><mchar name="And"/></mo>
      </xsl:otherwise>
      </xsl:choose>
    </xsl:for-each>
    <xsl:for-each select="child::*[position()=last()]">
      <xsl:choose>
      <xsl:when test="mml:or">
        <mfenced separators=""><xsl:apply-templates select="."/></mfenced>
      </xsl:when>
      <xsl:when test="mml:xor">
        <mfenced separators=""><xsl:apply-templates select="."/></mfenced>
      </xsl:when>
      <xsl:otherwise>
        <xsl:apply-templates select="."/>
      </xsl:otherwise>
      </xsl:choose>
    </xsl:for-each>
  </xsl:when>
  <xsl:when test="count(*)=2">
    <mo><mchar name="And"/></mo><xsl:apply-templates select="*[position()=last()]"/>
  </xsl:when>
  <xsl:otherwise>
    <mo><mchar name="And"/></mo>
  </xsl:otherwise>
  </xsl:choose>
</mrow>
</xsl:template>

<!-- OR -->
<xsl:template match="mml:apply[mml:or]">
<mrow>
  <xsl:choose>
  <xsl:when test="count(*)>=3">
    <xsl:for-each select="child::*[position()!=last() and  position()!=1]">
      <xsl:apply-templates select="."/><mo><mchar name="Or"/></mo>
    </xsl:for-each>
    <xsl:apply-templates select="child::*[position()=last()]"/>
    </xsl:when>
    <xsl:when test="count(*)=2">
      <mo><mchar name="Or"/></mo><xsl:apply-templates select="*[position()=last()]"/>
  </xsl:when>
  <xsl:otherwise>
    <mo><mchar name="Or"/></mo>
  </xsl:otherwise>
  </xsl:choose>
</mrow>
</xsl:template>

<!-- XOR -->
<xsl:template match="mml:apply[mml:xor]">
<mrow>
  <xsl:choose>
  <xsl:when test="count(*)>=3">
    <xsl:for-each select="child::*[position()!=last() and  position()!=1]">
      <xsl:apply-templates select="."/><mo>xor</mo>
    </xsl:for-each>
    <xsl:apply-templates select="child::*[position()=last()]"/>
    </xsl:when>
    <xsl:when test="count(*)=2">
      <mo>xor</mo><xsl:apply-templates select="*[position()=last()]"/>
  </xsl:when>
  <xsl:otherwise>
    <mo>xor</mo>
  </xsl:otherwise>
  </xsl:choose>
</mrow>
</xsl:template>

<!-- NOT -->
<xsl:template match="mml:apply[mml:not]">
  <mrow>
    <mo><mchar name="Not"/></mo>
    <xsl:choose>
    <xsl:when test="child::mml:apply"><!--add brackets around OR,AND,XOR children for priority purpose-->
      <mfenced separators="">
        <xsl:apply-templates select="child::*[position()=2]"/>
      </mfenced>
    </xsl:when>
    <xsl:otherwise>
      <xsl:apply-templates select="child::*[position()=2]"/>
    </xsl:otherwise>
    </xsl:choose>
  </mrow>
</xsl:template>

<!-- implies -->
<xsl:template match="mml:apply[mml:implies]">
  <mrow>
    <xsl:apply-templates select="child::*[position()=2]"/>
    <mo><mchar name="DoubleRightArrow"/></mo>
    <xsl:apply-templates select="child::*[position()=3]"/>
  </mrow>
</xsl:template>

<xsl:template match="mml:reln[mml:implies]">
  <mrow>
    <xsl:apply-templates select="child::*[position()=2]"/>
    <mo><mchar name="DoubleRightArrow"/></mo>
    <xsl:apply-templates select="child::*[position()=3]"/>
  </mrow>
</xsl:template>

<!-- for all-->
<xsl:template match="mml:apply[mml:forall]">
  <mrow>
    <mo><mchar name="ForAll"/></mo>
    <mrow>
      <xsl:for-each select="mml:bvar[position()!=last()]">
        <xsl:apply-templates select="."/><mo>,</mo>
      </xsl:for-each>
      <xsl:apply-templates select="mml:bvar[position()=last()]"/>
    </mrow>
    <xsl:if test="mml:condition">
      <mrow><mo>,</mo><xsl:apply-templates select="mml:condition"/></mrow>
    </xsl:if>
    <xsl:choose>
      <xsl:when test="mml:apply">
        <mo>:</mo><xsl:apply-templates select="mml:apply"/>
      </xsl:when>
      <xsl:when test="mml:reln">
        <mo>:</mo><xsl:apply-templates select="mml:reln"/>
      </xsl:when>
    </xsl:choose>
  </mrow>
</xsl:template>

<!-- in -->
<xsl:template match="mml:apply[mml:in]">
	<xsl:text>{</xsl:text>
    	<xsl:apply-templates select="child::*[position()=2]"/>
	<xsl:text>\in</xsl:text>
    	<xsl:apply-templates select="child::*[position()=3]"/>
	<xsl:text>}</xsl:text>
</xsl:template>

<!-- leq -->
<xsl:template match="mml:apply[mml:leq]">
	<xsl:text>{</xsl:text>
    	<xsl:apply-templates select="child::*[position()=2]"/>
	<xsl:text>\leq</xsl:text>
    	<xsl:apply-templates select="child::*[position()=3]"/>
	<xsl:text>}</xsl:text>
</xsl:template>

<!-- geq -->
<xsl:template match="mml:apply[mml:geq]">
	<xsl:text>{</xsl:text>
    	<xsl:apply-templates select="child::*[position()=2]"/>
	<xsl:text>\geq</xsl:text>
    	<xsl:apply-templates select="child::*[position()=3]"/>
	<xsl:text>}</xsl:text>
</xsl:template>

<!-- domain -->
<xsl:template match="mml:apply[mml:domain]">
	<xsl:text>\mbox{dom}(</xsl:text>
   	<xsl:apply-templates select="child::*[position()=2]"/>
	<xsl:text>)</xsl:text>
</xsl:template>

<!-- notin -->
<xsl:template match="mml:apply[mml:notin]">
	<xsl:text>{</xsl:text>
    	<xsl:apply-templates select="child::*[position()=2]"/>
	<xsl:text>\notin</xsl:text>
    	<xsl:apply-templates select="child::*[position()=3]"/>
	<xsl:text>}</xsl:text>
</xsl:template>

<!-- exist-->
<xsl:template match="mml:apply[mml:exists]">
  <mrow>
    <mo><mchar name="Exists"/></mo>
    <mrow>
      <xsl:for-each select="mml:bvar[position()!=last()]">
        <xsl:apply-templates select="."/><mo>,</mo>
      </xsl:for-each>
      <xsl:apply-templates select="mml:bvar[position()=last()]"/>
    </mrow>
    <xsl:if test="mml:condition">
      <mrow><mo>,</mo><xsl:apply-templates select="mml:condition"/></mrow>
    </xsl:if>
    <xsl:choose>
      <xsl:when test="mml:apply">
        <mo>:</mo><xsl:apply-templates select="mml:apply"/>
      </xsl:when>
      <xsl:when test="mml:reln">
        <mo>:</mo><xsl:apply-templates select="mml:reln"/>
      </xsl:when>
    </xsl:choose>
  </mrow>
</xsl:template>

<!-- absolute value -->
<xsl:template match="mml:apply[mml:abs]">
  <mrow><mo>|</mo><xsl:apply-templates select="child::*[position()=last()]"/><mo>|</mo></mrow>
</xsl:template>

<!-- conjugate -->
<xsl:template match="mml:apply[mml:conjugate]">
  <mover>
    <xsl:apply-templates select="child::*[position()=2]"/>
    <mo><mchar name="ovbar"/></mo>  <!-- does not work, UNICODE x0233D  or perhaps OverBar-->
  </mover>
</xsl:template>

<!-- argument of complex number -->
<xsl:template match="mml:apply[mml:arg]">
  <mrow>
    <mi>arg</mi><mo><mchar name="ApplyFunction"/></mo><mfenced separators=""><xsl:apply-templates select="child::*[position()=2]"/></mfenced>
  </mrow>
</xsl:template>

<!-- real part of complex number -->
<xsl:template match="mml:apply[mml:real]">
  <mrow>
    <mi><xsl:text disable-output-escaping='yes'>&amp;#x0211C;</xsl:text><!-- mchar Re or realpart should work--></mi>
    <mo><mchar name="ApplyFunction"/></mo>
    <mfenced separators=""><xsl:apply-templates select="child::*[position()=2]"/></mfenced>
  </mrow>
</xsl:template>

<!-- imaginary part of complex number -->
<xsl:template match="mml:apply[mml:imaginary]">
  <mrow>
    <mi><xsl:text disable-output-escaping='yes'>&amp;#x02111;</xsl:text><!-- mchar Im or impart should work--></mi>
    <mo><mchar name="ApplyFunction"/></mo>
    <mfenced separators=""><xsl:apply-templates select="child::*[position()=2]"/></mfenced>
  </mrow>
</xsl:template>

<!-- lowest common multiple -->
<xsl:template match="mml:apply[mml:lcm]">
  <mrow>
    <mi>lcm</mi><mo><mchar name="ApplyFunction"/></mo>
    <mfenced>
      <xsl:apply-templates select="child::*[position()!=1]"/>
    </mfenced>
  </mrow>
</xsl:template>


</xsl:stylesheet>
