<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" 
                xmlns:xlink='http://www.w3.org/1999/xlink'
                exclude-result-prefixes="xlink"
		version="1.0">

<xsl:param name="base.dir" select="'../htmldocs/'"/>
<xsl:param name="bridgehead.in.toc" select="1"/>
<xsl:param name="citerefentry.link" select="'1'"/>
<xsl:param name="css.decoration" select="1"/>
<xsl:param name="html.stylesheet" select="'../samba.css'"/>
<xsl:param name="html.stylesheet.type">text/css</xsl:param>
<xsl:param name="use.extensions" select="'0'"/>
<xsl:param name="use.id.as.filename" select="'1'"/>
<xsl:param name="use.local.olink.style" select="1"/> 
<xsl:param name="use.role.as.xrefstyle" select="1"/>
<xsl:param name="use.role.for.mediaobject" select="1"/>

<xsl:template name="generate.citerefentry.link">
  <xsl:value-of select="refentrytitle"/><xsl:text>.</xsl:text><xsl:value-of select="manvolnum"/><xsl:text>.html</xsl:text>
</xsl:template>

<xsl:template match="author">
</xsl:template>

<xsl:template match="link" name="link">
  <xsl:param name="linkend" select="@linkend"/>
  <xsl:param name="a.target"/>
  <xsl:param name="xhref" select="@xlink:href"/>

  <xsl:variable name="content">
    <xsl:call-template name="anchor"/>
    <xsl:choose>
      <xsl:when test="count(child::node()) &gt; 0">
        <!-- If it has content, use it -->
        <xsl:apply-templates/>
      </xsl:when>
      <!-- else look for an endterm -->
      <xsl:when test="@endterm">
        <xsl:variable name="etargets" select="key('id',@endterm)"/>
        <xsl:variable name="etarget" select="$etargets[1]"/>
        <xsl:choose>
          <xsl:when test="count($etarget) = 0">
            <xsl:message>
              <xsl:value-of select="count($etargets)"/>
              <xsl:text>Endterm points to nonexistent ID: </xsl:text>
              <xsl:value-of select="@endterm"/>
            </xsl:message>
            <xsl:text>???</xsl:text>
          </xsl:when>
          <xsl:otherwise>
              <xsl:apply-templates select="$etarget" mode="endterm"/>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:when>
      <xsl:otherwise>
         <xsl:choose>
            <xsl:when test="key('id', @linkend)/title[1]">
	      <xsl:call-template name="gentext.startquote"/>
	      <xsl:value-of select="key('id', @linkend)/title[1]"/>
	      <xsl:call-template name="gentext.endquote"/>
            </xsl:when>
         <xsl:otherwise>
    	    <xsl:message>
        	<xsl:text>Link element has no content and no Endterm. And linkend's pointer has no title. </xsl:text>
        	<xsl:text>Nothing to show in the link to </xsl:text>
        	<xsl:value-of select="(@linkend)[1]"/>
    	    </xsl:message>
    	 </xsl:otherwise>
        </xsl:choose>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:call-template name="simple.xlink">
    <xsl:with-param name="node" select="."/>
    <xsl:with-param name="linkend" select="$linkend"/>
    <xsl:with-param name="content" select="$content"/>
    <xsl:with-param name="a.target" select="$a.target"/>
    <xsl:with-param name="xhref" select="$xhref"/>
  </xsl:call-template>

</xsl:template>

</xsl:stylesheet>
