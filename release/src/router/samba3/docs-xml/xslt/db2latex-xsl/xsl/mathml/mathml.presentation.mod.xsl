<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet
[
 <!ENTITY % mmlalias PUBLIC "MathML alias" "ent/mmlalias.ent">  %mmlalias;
 <!ENTITY % mmlextra PUBLIC "MathML extra" "ent/mmlextra.ent">  %mmlextra;
]>
<!--############################################################################# 
 |	$Id: mathml.presentation.mod.xsl,v 1.4 2004/01/18 10:39:50 j-devenish Exp $		
 |- #############################################################################
 |	$Author: j-devenish $												
 |														
 |   PURPOSE: MathML presentation markup.
 |	Note: these elements are not part of the DocBook DTD. I have extended
 |    the docbook DTD in order to support this tags, so that's why I have these 
 |	templates here.
 |   
 |	MathML namespace used -> mml
 + ############################################################################## -->

<xsl:stylesheet version='1.0'
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform" 
	xmlns:mml="http://www.w3.org/1998/Math/MathML" xmlns="http://www.w3.org/1998/Math/MathML">

<xsl:template match="mml:mrow">
	<xsl:text>{</xsl:text> <xsl:apply-templates/> <xsl:text>}</xsl:text>
</xsl:template>

<xsl:variable name="latex.entities.xml" select="document('latex.entities.xml')"/>


<!-- TOKENS -->
<!-- Math Identifier -->
<xsl:template match="mml:mi">
	<xsl:variable name="fontstyle" select="@fontstyle"/>
	<xsl:variable name="identifier" select="normalize-space(.)"/>
	<xsl:variable name="equivalent">
		<xsl:if test="string-length($identifier)=1">
			<xsl:value-of select="$latex.entities.xml/latex/character[@entity=$identifier]"/>
		</xsl:if>
	</xsl:variable>
	<xsl:choose>
		<xsl:when test="$identifier='&ExponentialE;'">
			<xsl:text>\textrm{e}</xsl:text>
		</xsl:when>
		<xsl:when test="$identifier='&ImaginaryI;'">
			<xsl:text>\textrm{i}</xsl:text>
		</xsl:when>
		<xsl:when test="$identifier='&#x0221E;'"><!--/infty infinity -->
			<xsl:text>\infty</xsl:text>
		</xsl:when>
		<!-- currently tries to map single-character identifiers only -->
		<xsl:when test="$equivalent!=''">
			<xsl:text>{</xsl:text>
			<xsl:copy-of select="$equivalent"/>
			<xsl:text>}</xsl:text>
		</xsl:when>
		<xsl:otherwise>
			<xsl:if test="$fontstyle='normal' or string-length($identifier)&gt;1">
				<xsl:text>\textrm</xsl:text>
			</xsl:if>
			<xsl:text>{</xsl:text>
			<xsl:copy-of select="$identifier"/>
			<xsl:text>}</xsl:text>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<!-- Math Number -->
<xsl:template match="mml:mn">
	<xsl:copy-of select="normalize-space(.)"/>
</xsl:template>

<!-- Math Phantom -->
<xsl:template match="mml:mphantom">
	<xsl:apply-templates/>
</xsl:template>

<!-- Empty unless $character is a single character -->
<xsl:template name="generate.equivalent">
	<xsl:param name="arguments" select="0"/>
	<xsl:param name="character"/>
	<xsl:if test="string-length($character)=1">
		<xsl:choose>
			<xsl:when test="$arguments&gt;0">
				<xsl:value-of select="$latex.entities.xml/latex/character[@entity=$character and @arguments=$arguments]"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="$latex.entities.xml/latex/character[@entity=$character and @arguments='']"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:if>
</xsl:template>

<!-- Math Operator -->
<xsl:template match="mml:mo">
	<xsl:variable name="operator" select="normalize-space(.)"/>
	<xsl:variable name="equivalent">
		<xsl:call-template name="generate.equivalent">
			<xsl:with-param name="character" select="$operator"/>
		</xsl:call-template>
	</xsl:variable>
	<xsl:choose>
		<xsl:when test="$operator='&ApplyFunction;'">
			<xsl:text></xsl:text>
		</xsl:when>
		<xsl:when test="$operator='&InvisibleComma;'">
			<xsl:text>\thinspace</xsl:text>
		</xsl:when>
		<xsl:when test="$operator='&InvisibleTimes;'">
			<xsl:text>\thinspace</xsl:text>
		</xsl:when>
		<xsl:when test="$operator='&Integral;'">
			<xsl:text>\int</xsl:text>
		</xsl:when>
		<xsl:when test="$operator='&Product;'">
			<xsl:text>\prod</xsl:text>
		</xsl:when>
		<xsl:when test="$operator='&Sum;'">
			<xsl:text>\sum</xsl:text>
		</xsl:when>
		<xsl:when test="$operator='&Hat;'">
			<xsl:text>\sphat</xsl:text>
		</xsl:when>
		<xsl:when test="$operator='&RightArrow;'">
			<xsl:text>\longrightarrow</xsl:text>
		</xsl:when>
		<xsl:when test="$operator='&Element;'">
			<xsl:text>\in</xsl:text>
		</xsl:when>
		<xsl:when test="$operator='&VerticalBar;'">
			<xsl:text>|</xsl:text>
		</xsl:when>
		<xsl:when test="$operator='&DifferentialD;'">
			<xsl:text>\textrm{d}</xsl:text>
		</xsl:when>
		<xsl:when test="$operator='('">
			<xsl:text> {\left( </xsl:text>
		</xsl:when>
		<xsl:when test="$operator=')'">
			<xsl:text> \right)} </xsl:text>
		</xsl:when>
		<xsl:when test="$operator='{'">
			<xsl:text> {\left\{ </xsl:text>
		</xsl:when>
		<xsl:when test="$operator='}'">
			<xsl:text> \right\}} </xsl:text>
		</xsl:when>
		<xsl:when test="$operator='['">
			<xsl:text> {\left[ </xsl:text>
		</xsl:when>
		<xsl:when test="$operator=']'">
			<xsl:text> \right]} </xsl:text>
		</xsl:when>
		<xsl:when test="$operator='max'">
			<xsl:text> \max </xsl:text>
		</xsl:when>
		<xsl:when test="$operator='min'">
			<xsl:text> \min </xsl:text>
		</xsl:when>
		<xsl:when test="$operator='+' or $operator='-' or $operator='/' or $operator='*'">
			<xsl:value-of select="$operator"/>
		</xsl:when>
		<xsl:when test="$equivalent">
			<xsl:value-of select="$equivalent"/>
		</xsl:when>
		<xsl:otherwise>
			<xsl:text>\operatorname{</xsl:text>
			<xsl:value-of select="$operator" />
			<xsl:text>}</xsl:text>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<!-- Math String -->
<xsl:template match="mml:ms">
	<xsl:text>\textrm{</xsl:text>
	<xsl:copy-of select="normalize-space(.)" />
	<xsl:text>}</xsl:text>
</xsl:template>

<!-- Math Text -->
<xsl:template match="mml:mtext">
	<xsl:message>RCAS mtext, <xsl:copy-of select="."/> </xsl:message>
	<xsl:text>\textrm{</xsl:text>
	<xsl:copy-of select="." />
	<xsl:text>}</xsl:text>
</xsl:template>

<!-- Math Space -->
<xsl:template match="mml:mspace">
	<xsl:if test="@width!='' and not(contains(@width,'%'))">
		<xsl:text>\textrm{\hspace{</xsl:text><!-- kludge! -->
		<xsl:value-of select="@width"/>
		<xsl:text>}}</xsl:text>
	</xsl:if>
	<xsl:if test="@height!='' or @depth!=''">
		<xsl:message>Warning: mspace support does not include height or depth.</xsl:message>
	</xsl:if>
</xsl:template>





<xsl:template match="mml:msup">
        <xsl:apply-templates select="*[1]"/>
        <xsl:text>^{</xsl:text><xsl:apply-templates select="*[2]"/><xsl:text>}</xsl:text>
</xsl:template>

<xsl:template match="mml:msub">
        <xsl:apply-templates select="*[1]"/>
        <xsl:text>_{</xsl:text><xsl:apply-templates select="*[2]"/><xsl:text>}</xsl:text>
</xsl:template>

<xsl:template match="mml:msubsup">
<xsl:choose>
	<xsl:when test="name(*[1])='mo'">
		<xsl:apply-templates select="*[1]"/>
		<!-- sub -->
		<xsl:text>_{</xsl:text><xsl:apply-templates select="*[2]"/><xsl:text>}</xsl:text>
		<!-- super -->
		<xsl:text>^{</xsl:text><xsl:apply-templates select="*[3]"/><xsl:text>}</xsl:text>
     </xsl:when>
     <xsl:otherwise>
	     	<!-- base -->
		<xsl:text>{</xsl:text><xsl:apply-templates select="*[1]"/><xsl:text>}</xsl:text>
		<!-- sub -->
		<xsl:text>_{</xsl:text><xsl:apply-templates select="*[2]"/><xsl:text>}</xsl:text>
		<!-- super -->
		<xsl:text>^{</xsl:text><xsl:apply-templates select="*[3]"/><xsl:text>}</xsl:text>
	</xsl:otherwise>
</xsl:choose>
</xsl:template>

<xsl:template match="mml:mmultiscripts">
</xsl:template>

<xsl:template match="mml:munder">
<!--
<xsl:choose>
	<xsl:when test="*[2] = &#818;">
		<xsl:text>\underline{</xsl:text><xsl:apply-templates select="*[1]"/><xsl:text>}</xsl:text>
	</xsl:when>
	<xsl:when test="normalize-space(*[2]) = &#65080;">
		<xsl:text>\underbrace{</xsl:text><xsl:apply-templates select="*[1]"/><xsl:text>}</xsl:text>
	</xsl:when>
	<xsl:when test="normalize-space(*[2]) = &#9141;">
		<xsl:text>\underbrace{</xsl:text><xsl:apply-templates select="*[1]"/><xsl:text>}</xsl:text>
	</xsl:when>
	<xsl:otherwise>
	<xsl:text>\underset{</xsl:text>
		<xsl:apply-templates select="*[2]"/>
	<xsl:text>}{</xsl:text>
		<xsl:apply-templates select="*[1]"/>
	<xsl:text>}</xsl:text>
-->
	<xsl:text>{</xsl:text>
	<xsl:apply-templates select="*[1]"/>
	<xsl:text>_{</xsl:text>
	<xsl:apply-templates select="*[2]"/>
	<xsl:text>}}</xsl:text>
<!--
	</xsl:otherwise>
</xsl:choose>
-->
</xsl:template>
<xsl:template match="mml:mover">
<!--<xsl:choose>
	<xsl:when test="normalize-space(*[2]) = &#175;">
		<xsl:text>\overline{</xsl:text><xsl:apply-templates select="*[1]"/><xsl:text>}</xsl:text>
	</xsl:when>
	<xsl:when test="normalize-space(*[2]) = &#65079;"> 
		<xsl:text>\overbrace{</xsl:text><xsl:apply-templates select="*[1]"/><xsl:text>}</xsl:text>
	</xsl:when>
	<xsl:when test="normalize-space(*[2]) = &#65077;"> 
		<xsl:text>\widehat{</xsl:text><xsl:apply-templates select="*[1]"/><xsl:text>}</xsl:text>
	</xsl:when>
	<xsl:when test="normalize-space(*[2]) = &#9140;">
		<xsl:text>\widehat{</xsl:text><xsl:apply-templates select="*[1]"/><xsl:text>}</xsl:text>
	</xsl:when>
	<xsl:otherwise>
	</xsl:otherwise>
</xsl:choose>-->
	<xsl:choose>
		<xsl:when test="@accent='true' or ( local-name(*[2])='mo' and not(@accent='false'))">
			<xsl:variable name="equivalent">
				<xsl:call-template name="generate.equivalent">
					<xsl:with-param name="arguments" select="1"/>
					<xsl:with-param name="character" select="normalize-space(*[2])"/>
				</xsl:call-template>
			</xsl:variable>
			<xsl:choose>
				<xsl:when test="$equivalent!=''">
					<xsl:text>{</xsl:text>
					<xsl:value-of select="$equivalent"/>
					<xsl:text>{</xsl:text>
					<xsl:apply-templates select="*[1]"/>
					<xsl:text>}}</xsl:text>
				</xsl:when>
				<xsl:otherwise>
					<xsl:text>{</xsl:text>
					<xsl:apply-templates select="*[1]"/>
					<xsl:text>^{</xsl:text>
					<xsl:apply-templates select="*[2]"/>
					<xsl:text>}}</xsl:text>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:when>
		<xsl:otherwise>
			<xsl:text>{</xsl:text>
			<xsl:apply-templates select="*[1]"/>
			<xsl:text>^{</xsl:text>
			<xsl:apply-templates select="*[2]"/>
			<xsl:text>}}</xsl:text>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>



<!-- Math UnderOver -->
<xsl:template match="mml:munderover">
	<xsl:text>{</xsl:text>
	<xsl:apply-templates select="*[1]"/>
	<xsl:text>_{</xsl:text>
	<xsl:apply-templates select="*[2]"/>
	<xsl:text>}</xsl:text>
	<xsl:text>^{</xsl:text>
	<xsl:apply-templates select="*[3]"/>
	<xsl:text>}}</xsl:text>
	<!--
	<xsl:text>\overset{</xsl:text>
			<xsl:apply-templates select="*[3]"/>
		<xsl:text>}{\underset{</xsl:text>
				<xsl:apply-templates select="*[2]"/>
					<xsl:text>}{</xsl:text>
				<xsl:apply-templates select="*[1]"/>
	<xsl:text>}}</xsl:text>
	-->
</xsl:template>



<!-- Math Fenced -->
<xsl:template match="mml:mfenced">
<!-- get open,close, separators att -->
	<xsl:choose>
		<xsl:when test="@open='('">
			<xsl:text> {\left( </xsl:text>
		</xsl:when>
		<xsl:when test="@open='{'">
			<xsl:text> {\left\{ </xsl:text>
		</xsl:when>
		<xsl:when test="@open='['">
			<xsl:text> {\left[\, </xsl:text>
		</xsl:when>
		<xsl:otherwise>
			<xsl:text> {\left( </xsl:text>
		</xsl:otherwise>
	</xsl:choose>
	<xsl:apply-templates select="*[1]"/>
	<xsl:choose>
		<xsl:when test="@close=')'">
			<xsl:text> \right)} </xsl:text>
		</xsl:when>
		<xsl:when test="@close='}'">
			<xsl:text> \right\}} </xsl:text>
		</xsl:when>
		<xsl:when test="@close=']'">
			<xsl:text> \,\right]} </xsl:text>
		</xsl:when>
		<xsl:otherwise>
			<xsl:text> \right)} </xsl:text>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>



<!-- Math frac -->
<xsl:template match="mml:mfrac">
<xsl:choose>
<xsl:when test="@linethickness">
	<xsl:choose>
	<xsl:when test="@linethickness='thin'">
		<xsl:text> \frac[1pt]{ </xsl:text>
	</xsl:when>
	<xsl:when test="@linethickness='medium'">
		<xsl:text> \frac[1.1pt]{ </xsl:text>
	</xsl:when>
	<xsl:when test="@linethickness='thick'">
		<xsl:text> \frac[1.2pt]{ </xsl:text>
	</xsl:when>
	<xsl:otherwise>
		<xsl:text> \frac[</xsl:text><xsl:value-of select="@linethickness"/><xsl:text>]{ </xsl:text>
	</xsl:otherwise>
	</xsl:choose>
</xsl:when>
<xsl:otherwise>
	<xsl:text> \frac{ </xsl:text>
</xsl:otherwise>
</xsl:choose>
<!--	<xsl:value-of select="*[1]"/> -->
<xsl:apply-templates select="*[1]"/>
<xsl:text> }{ </xsl:text>
<!--	<xsl:value-of select="*[2]"/> -->
<xsl:apply-templates select="*[2]"/>
<xsl:text> }</xsl:text>
</xsl:template>


<!-- Math msqrt -->
<xsl:template match="mml:msqrt">
	<xsl:text> \sqrt{ </xsl:text>
	<xsl:apply-templates/>
	<xsl:text> }</xsl:text>
</xsl:template>


<!-- Math mroot -->
<xsl:template match="mml:mroot">
	<xsl:text> \sqrt[</xsl:text><xsl:apply-templates select="*[1]"/><xsl:text>]{</xsl:text>
		<xsl:apply-templates select="*[2]"/><xsl:text> }</xsl:text>
</xsl:template>



<xsl:template name="mtable.format.tabular">
  	<xsl:param name="cols" select="1"/>
	<xsl:param name="i" select="1"/>
  	<xsl:choose>
		<!-- Out of the recursive iteration -->
    		<xsl:when test="$i > $cols"></xsl:when>
		<!-- There are still columns to count -->
    		<xsl:otherwise>
        		<xsl:text>c</xsl:text>
				<!-- Recursive for next column -->
      			<xsl:call-template name="mtable.format.tabular">
        				<xsl:with-param name="i" select="$i+1"/>
        				<xsl:with-param name="cols" select="$cols"/>
      			</xsl:call-template>
    		</xsl:otherwise>
	</xsl:choose>
</xsl:template>


<xsl:template match="mml:mtable">
<xsl:variable name="rows" select="mml:mtr"/>
<xsl:text>\begin{array}{</xsl:text>
<xsl:call-template name="mtable.format.tabular"><xsl:with-param name="cols" select="count($rows)"/></xsl:call-template>
<xsl:text>}&#10;</xsl:text>
	<xsl:apply-templates/>
<xsl:text>\end{array} </xsl:text>
</xsl:template>

<xsl:template match="mml:mtr">
<!-- Row starts here -->
<xsl:apply-templates/>
<!-- End Row here -->
</xsl:template>

<xsl:template match="mml:mtd">
    <xsl:apply-templates/><xsl:text> &amp; </xsl:text> 
</xsl:template>

<xsl:template match="mml:mtd[position()=last()]">
    <xsl:apply-templates/><xsl:text>\\&#10;</xsl:text>
</xsl:template>

<xsl:template match="mml:mtd[position()=last()]">
    <xsl:apply-templates/><xsl:text>\\&#10;</xsl:text>
</xsl:template>

<xsl:template match="mml:mphantom">
    <xsl:apply-templates mode="phantom"/>
</xsl:template>

<xsl:template match="mml:mi" mode="phantom">
	<xsl:variable name="fontstyle" select="@fontstyle"/>
	<xsl:variable name="identifier" select="normalize-space(.)"/>
	<xsl:choose>
		<xsl:when test="$identifier='&ExponentialE;'">
			<xsl:text>\textrm{e}</xsl:text>
		</xsl:when>
		<xsl:when test="$identifier='&ImaginaryI;'">
			<xsl:text>\textrm{i}</xsl:text>
		</xsl:when>
		<xsl:otherwise>
			<xsl:text>\textrm{</xsl:text> <xsl:copy-of select="$identifier"/> <xsl:text>}</xsl:text>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>
</xsl:stylesheet>
