<?xml version='1.0'?>
<!--############################################################################# 
|	$Id: synop-struct.mod.xsl,v 1.5 2003/10/19 07:56:56 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
|														
|   PURPOSE:
+ ############################################################################## -->

<xsl:stylesheet 
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
    exclude-result-prefixes="doc" version='1.0'>

	<xsl:template match="synopsis">
	<xsl:call-template name="label.id"/>
	<xsl:apply-templates/>
	</xsl:template>

	<xsl:template match="cmdsynopsis">
	<xsl:call-template name="label.id"/>
	<xsl:text>&#10;\begin{list}{}{\setlength{\itemindent}{-\leftmargin}\setlength{\parsep}{0mm}}&#10;</xsl:text>
	<xsl:if test="@label!=''">
		<xsl:text>\item\textbf{</xsl:text>
		<xsl:call-template name="normalize-scape"><xsl:with-param name="string" select="@label"/></xsl:call-template>
		<xsl:text>}&#10;</xsl:text>
	</xsl:if>
	<xsl:text>\item\raggedright\texttt{</xsl:text>
	<xsl:apply-templates/>
	<xsl:text>}&#10;</xsl:text>
	<xsl:text>\end{list}&#10;</xsl:text>
	</xsl:template>


	<xsl:template match="cmdsynopsis/command">
	<xsl:apply-templates />
	</xsl:template>


	<xsl:template match="cmdsynopsis//replaceable" priority="2">
	<xsl:text>\textit{</xsl:text>
	<xsl:apply-templates />
	<xsl:text>}</xsl:text>
	</xsl:template>


    <xsl:template match="group|arg">
	<xsl:variable name="choice" select="@choice"/>
	<xsl:variable name="rep" select="@rep"/>
	<xsl:variable name="sepchar">
	    <xsl:choose>
		<xsl:when test="ancestor-or-self::*/@sepchar">
		    <xsl:value-of select="ancestor-or-self::*/@sepchar"/>
		</xsl:when>
		<xsl:otherwise>
		    <xsl:text> </xsl:text>
		</xsl:otherwise>
	    </xsl:choose>
	</xsl:variable>
	<xsl:if test="position()>1"><xsl:value-of select="$sepchar"/></xsl:if>
	<xsl:choose>
	    <xsl:when test="$choice='plain'">
		<xsl:value-of select="$arg.choice.plain.open.str"/>
	    </xsl:when>
	    <xsl:when test="$choice='req'">
		<xsl:value-of select="$arg.choice.req.open.str"/>
	    </xsl:when>
	    <xsl:when test="$choice='opt'">
		<xsl:value-of select="$arg.choice.opt.open.str"/>
	    </xsl:when>
	    <xsl:otherwise>
		<xsl:value-of select="$arg.choice.def.open.str"/>
	    </xsl:otherwise>
	</xsl:choose>
	<xsl:apply-templates/>
	<xsl:choose>
	    <xsl:when test="$rep='repeat'">
		<xsl:value-of select="$arg.rep.repeat.str"/>
	    </xsl:when>
	    <xsl:when test="$rep='norepeat'">
		<xsl:value-of select="$arg.rep.norepeat.str"/>
	    </xsl:when>
	    <xsl:otherwise>
		<xsl:value-of select="$arg.rep.def.str"/>
	    </xsl:otherwise>
	</xsl:choose>
	<xsl:choose>
	    <xsl:when test="$choice='plain'">
		<xsl:value-of select="$arg.choice.plain.close.str"/>
	    </xsl:when>
	    <xsl:when test="$choice='req'">
		<xsl:value-of select="$arg.choice.req.close.str"/>
	    </xsl:when>
	    <xsl:when test="$choice='opt'">
		<xsl:value-of select="$arg.choice.opt.close.str"/>
	    </xsl:when>
	    <xsl:otherwise>
		<xsl:value-of select="$arg.choice.def.close.str"/>
	    </xsl:otherwise>
	</xsl:choose>
    </xsl:template>

    <xsl:template match="group/arg">
	<xsl:variable name="choice" select="@choice"/>
	<xsl:variable name="rep" select="@rep"/>
	<xsl:if test="position()>1"><xsl:value-of select="$arg.or.sep"/></xsl:if>
	<xsl:apply-templates/>
    </xsl:template>

    <xsl:template match="sbr">
	<xsl:text>&#10;</xsl:text>
    </xsl:template>

    <!-- ==================================================================== -->

    <xsl:template match="synopfragmentref">
	<!-- VAR target : -->
	<xsl:variable name="target" select="key('id',@linkend)"/>
	<!-- VAR snum : -->
	<xsl:variable name="snum">
	    <xsl:apply-templates select="$target" mode="synopfragment.number"/>
	</xsl:variable>

	<xsl:text> {\em (</xsl:text> <xsl:value-of select="$snum"/> <xsl:text>) }</xsl:text>
    </xsl:template>

    <xsl:template match="synopfragment" mode="synopfragment.number">
	<xsl:number format="1"/>
    </xsl:template>

    <xsl:template match="synopfragment">
	<xsl:variable name="snum">
	    <xsl:apply-templates select="." mode="synopfragment.number"/>
	</xsl:variable>
	<p>
	    <a name="#{@id}">
		<xsl:text>(</xsl:text>
		<xsl:value-of select="$snum"/>
		<xsl:text>)</xsl:text>
	    </a>
	    <xsl:text> </xsl:text>
	    <xsl:apply-templates/>
	</p>
    </xsl:template>   


    <xsl:template match="funcsynopsis">
	<xsl:call-template name="informal.object"/>
    </xsl:template>


    <xsl:template match="funcsynopsisinfo">
	<xsl:call-template name="verbatim.apply.templates"/>
    </xsl:template>


    <xsl:template match="funcprototype">
	<xsl:apply-templates/>
	<xsl:if test="$funcsynopsis.style='kr'">
	    <xsl:apply-templates select="./paramdef" mode="kr-funcsynopsis-mode"/>
	</xsl:if>
    </xsl:template>

    <xsl:template match="funcdef">
	<xsl:apply-templates/>
    </xsl:template>

    <xsl:template match="funcdef/function">
	<xsl:choose>
	    <xsl:when test="$funcsynopsis.decoration != 0">
		<xsl:text>\textbf{ </xsl:text><xsl:apply-templates/><xsl:text> } </xsl:text>
	    </xsl:when>
	    <xsl:otherwise>
		<xsl:apply-templates/>
	    </xsl:otherwise>
	</xsl:choose>
    </xsl:template>


    <xsl:template match="void">
	<xsl:choose>
	    <xsl:when test="$funcsynopsis.style='ansi'">
		<xsl:text>(void);</xsl:text>
	    </xsl:when>
	    <xsl:otherwise>
		<xsl:text>();</xsl:text>
	    </xsl:otherwise>
	</xsl:choose>
    </xsl:template>

    <xsl:template match="varargs">
	<xsl:text>( ... );</xsl:text>
    </xsl:template>

    <xsl:template match="paramdef">
	<!-- VAR paramnum -->
	<xsl:variable name="paramnum"> <xsl:number count="paramdef" format="1"/> </xsl:variable>

	<xsl:if test="$paramnum=1">(</xsl:if>
	<xsl:choose>
	    <xsl:when test="$funcsynopsis.style='ansi'">
		<xsl:apply-templates/>
	    </xsl:when>
	    <xsl:otherwise>
		<xsl:apply-templates select="./parameter"/>
	    </xsl:otherwise>
	</xsl:choose>
	<xsl:choose>
	    <xsl:when test="following-sibling::paramdef">
		<xsl:text>, </xsl:text>
	    </xsl:when>
	    <xsl:otherwise>
		<xsl:text>);</xsl:text>
	    </xsl:otherwise>
	</xsl:choose>
    </xsl:template>



    <xsl:template match="paramdef/parameter">
	<xsl:choose>
	    <xsl:when test="$funcsynopsis.decoration != 0">
		<xsl:apply-templates/>
	    </xsl:when>
	    <xsl:otherwise>
		<xsl:apply-templates/>
	    </xsl:otherwise>
	</xsl:choose>
	<xsl:if test="following-sibling::parameter">
	    <xsl:text>, </xsl:text>
	</xsl:if>
    </xsl:template>



    <xsl:template match="paramdef" mode="kr-funcsynopsis-mode">
	\newline
	<xsl:apply-templates/>
	<xsl:text>;</xsl:text>
    </xsl:template>

    <xsl:template match="funcparams">
	<xsl:text>(</xsl:text>
	<xsl:apply-templates/>
	<xsl:text>)</xsl:text>
    </xsl:template>

</xsl:stylesheet>
