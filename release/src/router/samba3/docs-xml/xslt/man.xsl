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

<xsl:template match="itemizedlist/listitem">
  <!-- * We output a real bullet here (rather than, "\(bu", -->
  <!-- * the roff bullet) because, when we do character-map -->
  <!-- * processing before final output, the character-map will -->
  <!-- * handle conversion of the &#x2022; to "\(bu" for us -->
  <xsl:text>&#10;</xsl:text>
  <xsl:text>.sp</xsl:text>
  <xsl:text>&#10;</xsl:text>
  <xsl:text>.RS</xsl:text>
  <xsl:if test="not($list-indent = '')">
    <xsl:text> </xsl:text>
    <xsl:value-of select="$list-indent"/>
  </xsl:if>
  <xsl:text>&#10;</xsl:text>
  <!-- * if "n" then we are using "nroff", which means the output is for -->
  <!-- * TTY; so we do some fixed-width-font hackery with \h to make a -->
  <!-- * hanging indent (instead of using .IP, which has some -->
  <!-- * undesirable side effects under certain circumstances) -->
  <xsl:call-template name="roff-if-else-start"/>
  <xsl:text>\h'-</xsl:text>
  <xsl:choose>
    <xsl:when test="not($list-indent = '')">
      <xsl:text>0</xsl:text>
      <xsl:value-of select="$list-indent"/>
    </xsl:when>
    <xsl:otherwise>
      <xsl:text>\n(INu</xsl:text>
    </xsl:otherwise>
  </xsl:choose>
  <xsl:text>'</xsl:text>
  <xsl:text>&#x2022;</xsl:text>
  <xsl:text>\h'+</xsl:text>
  <xsl:choose>
    <xsl:when test="not($list-indent = '')">
      <xsl:text>0</xsl:text>
      <xsl:value-of select="$list-indent - 1"/>
    </xsl:when>
    <xsl:otherwise>
      <xsl:text>\n(INu-1</xsl:text>
    </xsl:otherwise>
  </xsl:choose>
  <xsl:text>'\c&#10;</xsl:text>
  <!-- * else, we are not using for "nroff", but instead "troff" - which -->
  <!-- * means not for TTY, but for PS or whatever; so we’re not using a -->
  <!-- * fixed-width font, so use a real .IP instead -->
  <xsl:call-template name="roff-else"/>
  <!-- * .IP generates a blank like of space, so let’s go backwards one -->
  <!-- * line up to compensate for that -->
  <xsl:text>.sp -1&#10;</xsl:text>
  <xsl:text>.IP \(bu 2.3&#10;</xsl:text>
  <!-- * The value 2.3 is the amount of indentation; we use 2.3 instead -->
  <!-- * of 2 because when the font family is New Century Schoolbook it -->
  <!-- * seems to require the extra space. -->
  <xsl:call-template name="roff-if-end"/>
  <xsl:apply-templates/>
  <xsl:if test=" following-sibling::listitem">
	 <xsl:text>&#10;.RE&#10;</xsl:text>
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
