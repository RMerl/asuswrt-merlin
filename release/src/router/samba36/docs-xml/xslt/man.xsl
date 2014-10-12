<?xml version='1.0'?>
<!-- vim:set sts=2 shiftwidth=2 syntax=xml: -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	        xmlns:xlink='http://www.w3.org/1999/xlink'
                version='1.0'>

<xsl:import href="http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl"/>

<xsl:param name="chunk.section.depth" select="0"/>
<xsl:param name="chunk.first.sections" select="1"/>
<xsl:param name="use.id.as.filename" select="1"/>
<xsl:param name="man.endnotes.are.numbered" select="0"/>

<!-- 
    Our ulink stylesheet omits @url part if content was specified
-->
<xsl:template match="ulink">
  <xsl:variable name="content">
    <xsl:apply-templates/>
  </xsl:variable>
  <xsl:if test="$content = ''">
    <xsl:text>: </xsl:text>
  </xsl:if>
  <xsl:if test="$content != ''">
    <xsl:value-of select="$content" />
  </xsl:if>
  <xsl:if test="$content = ''">
    <xsl:apply-templates mode="italic" select="@url" />
  </xsl:if>
</xsl:template>

<xsl:template match="itemizedlist|orderedlist|procedure">
  <xsl:if test="title">
    <xsl:text>.PP&#10;</xsl:text>
    <xsl:call-template name="bold">
      <xsl:with-param name="node" select="title"/>
      <xsl:with-param name="context" select="."/>
    </xsl:call-template>
    <xsl:text>&#10;</xsl:text>
  </xsl:if>
  <!-- * DocBook allows just about any block content to appear in -->
  <!-- * lists before the actual list items, so we need to get that -->
  <!-- * content (if any) before getting the list items -->
  <xsl:apply-templates
    select="*[not(self::listitem) and not(self::title)]"/>
  <xsl:apply-templates select="listitem"/>
  <xsl:if test="(parent::para or parent::listitem) or following-sibling::node()">
    <xsl:text>.sp&#10;</xsl:text>
    <xsl:text>.RE&#10;</xsl:text>
  </xsl:if>
</xsl:template>

<xsl:template match="refsect3">
  <xsl:text>&#10;.SS "</xsl:text>
  <xsl:value-of select="title[1]"/>
  <xsl:text>"&#10;</xsl:text>
  <xsl:apply-templates/>
</xsl:template>

  <!-- ================================================================== -->
  <!-- These macros are from Docbook manpages XSLT development tree       -->
  <!-- help to maintain manpage generation clean when difference between  -->
  <!-- roff processors is important to note.                              -->

  <xsl:template name="roff-if-else-start">
    <xsl:param name="condition">n</xsl:param>
    <xsl:text>.ie </xsl:text>
    <xsl:value-of select="$condition"/>
    <xsl:text> \{\&#10;</xsl:text>
  </xsl:template>

  <xsl:template name="roff-if-start">
    <xsl:param name="condition">n</xsl:param>
    <xsl:text>.if </xsl:text>
    <xsl:value-of select="$condition"/>
    <xsl:text> \{\&#10;</xsl:text>
  </xsl:template>

  <xsl:template name="roff-else">
    <xsl:text>.\}&#10;</xsl:text>
    <xsl:text>.el \{\&#10;</xsl:text>
  </xsl:template>

  <xsl:template name="roff-if-end">
    <xsl:text>.\}&#10;</xsl:text>
  </xsl:template>

</xsl:stylesheet>
