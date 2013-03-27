<xsl:stylesheet version="1.0"
		xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:output method="text"/>

  <xsl:variable name="total" select="count(//reg)"/>

  <xsl:template name="pad">
    <xsl:param name="count" select="0"/>
    <xsl:text>0:
</xsl:text>
    <xsl:if test="$count > 1">
      <xsl:call-template name="pad">
	<xsl:with-param name="count" select="$count - 1"/>
      </xsl:call-template>
    </xsl:if>
  </xsl:template>

  <xsl:template name="reg">
    <xsl:param name="which" select="1"/>
    <xsl:param name="nextnum" select="0"/>
    <xsl:variable name="thisnum" select="@regnum"/>
    <xsl:if test="$nextnum &lt; number(@regnum)">
      <xsl:call-template name="pad">
	<xsl:with-param name="count" select="@regnum - $nextnum"/>
      </xsl:call-template>
    </xsl:if>
    <xsl:value-of select="@bitsize"/>
    <xsl:text>:</xsl:text>
    <xsl:value-of select="@name"/>
    <xsl:text>
</xsl:text>
    <xsl:if test="$which &lt; $total">
      <xsl:for-each select="/descendant::reg[$which + 1]">
	<xsl:call-template name="reg">
	  <xsl:with-param name="which" select="$which + 1"/>
	  <xsl:with-param name="nextnum" select="$thisnum + 1"/>
	</xsl:call-template>
      </xsl:for-each>
    </xsl:if>
  </xsl:template>

  <xsl:template match="/">
    <xsl:for-each select="/descendant::reg[1]">
      <xsl:call-template name="reg"/>
    </xsl:for-each>
  </xsl:template>
</xsl:stylesheet>
