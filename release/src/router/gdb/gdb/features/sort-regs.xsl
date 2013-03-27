<xsl:stylesheet version="1.0"
		xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:template match="/">
    <target>
      <xsl:for-each select="//reg">
	<xsl:sort select="@regnum" data-type="number"/>
	<xsl:copy-of select="."/>
      </xsl:for-each>
    </target>
  </xsl:template>
</xsl:stylesheet>
