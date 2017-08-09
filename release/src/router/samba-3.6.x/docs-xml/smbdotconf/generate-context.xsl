<?xml version='1.0'?>
<!-- vim:set sts=2 shiftwidth=2 syntax=xml: -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:exsl="http://exslt.org/common"
                xmlns:samba="http://samba.org/common"
		version="1.1"
                extension-element-prefixes="exsl">

<xsl:output method="xml" omit-xml-declaration="yes"/>

<xsl:param name="smb.context" select="'G'"/>

<!-- This is needed to copy content unchanged -->
<xsl:template match="@*|node()">
  <xsl:copy>
    <xsl:apply-templates select="@*|node()"/>
  </xsl:copy>
</xsl:template>

<xsl:template match="variablelist">
     <xsl:element name="itemizedlist">
        <xsl:apply-templates/>
     </xsl:element>
</xsl:template>

<xsl:template match="//samba:parameter">
  <xsl:variable name="name"><xsl:value-of select="translate(translate(string(@name),' ',''),
                  'abcdefghijklmnopqrstuvwxyz','ABCDEFGHIJKLMNOPQRSTUVWXYZ')"/>
  </xsl:variable>
  
  <xsl:if test="contains(@context,$smb.context) or $smb.context='ALL'">
     <xsl:element name="listitem">
        <xsl:element name="para">
           <xsl:element name="link">
              <xsl:attribute name="linkend">
                 <xsl:value-of select="$name"/>
              </xsl:attribute>
              <xsl:element name="parameter">
                 <xsl:attribute name="moreinfo"><xsl:text>none</xsl:text></xsl:attribute>
                 <xsl:value-of select="@name"/>
              </xsl:element>
           </xsl:element>
        </xsl:element>
     </xsl:element>
     <xsl:text>&#10;</xsl:text>     
  </xsl:if>
</xsl:template>

</xsl:stylesheet>
