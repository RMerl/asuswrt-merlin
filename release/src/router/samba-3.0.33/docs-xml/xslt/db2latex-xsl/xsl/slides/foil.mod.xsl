<?xml version='1.0'?>
<!--############################################################################# 
|	$Id: foil.mod.xsl,v 1.1 2003/04/06 18:31:49 rcasellas Exp $
|- #############################################################################
|	$Author: rcasellas $
|														
|   PURPOSE:
+ ############################################################################## -->

<xsl:stylesheet 
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
    exclude-result-prefixes="doc" version='1.0'>


    <!--############################################################################# -->
    <!-- DOCUMENTATION                                                                -->
    <doc:reference id="foil" xmlns="">
    </doc:reference>
    <!--############################################################################# -->

    <xsl:template match="foilgroup">
	<xsl:text>                                                                        &#10;&#10;</xsl:text>
	<xsl:text>%---------------------------------------------------------------------- PART &#10;</xsl:text>
	<xsl:text>\part{</xsl:text><xsl:apply-templates select="title"/><xsl:text>            }&#10;</xsl:text>
	<xsl:text>%---------------------------------------------------------------------- PART &#10;</xsl:text>
	<xsl:call-template name="label.id"/>
	<xsl:text>&#10;&#10;</xsl:text>
	<xsl:apply-templates select="foil"/>
    </xsl:template>

    <xsl:template match="foilgroup/title">
	<xsl:apply-templates />
    </xsl:template>

    <xsl:template match="foil">
	<xsl:text>&#10;</xsl:text>
	<xsl:text>%---------------------------------------------------------------------- SLIDE &#10;</xsl:text>
	<xsl:text>\begin{slide}{</xsl:text>
	<xsl:apply-templates select="title"/>
	<xsl:text>}&#10;</xsl:text>
	<xsl:call-template name="label.id"/>
	<xsl:text>&#10;</xsl:text>
	<xsl:apply-templates select="*[not (self::title)]"/>
	<xsl:text>\end{slide}&#10;</xsl:text>
    </xsl:template>

    <xsl:template match="foil/title">
	<xsl:apply-templates />
    </xsl:template>

</xsl:stylesheet>
