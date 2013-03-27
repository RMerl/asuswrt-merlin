<xsl:stylesheet version="1.0"
		xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:variable name="total" select="count(//reg)"/>
  <xsl:template name="reg">
    <xsl:param name="which" select="1"/>
    <xsl:param name="nextnum" select="0"/>
    <xsl:variable name="thisnum" select="@regnum"/>
    <xsl:element name="reg">
      <xsl:attribute name="name"><xsl:value-of select="@name"/></xsl:attribute>
      <xsl:attribute name="bitsize"><xsl:value-of select="@bitsize"/></xsl:attribute>
      <xsl:choose>
	<xsl:when test="not(@regnum)">
	  <xsl:attribute name="regnum"><xsl:value-of select="$nextnum"/></xsl:attribute>
	</xsl:when>
	<xsl:otherwise>
	  <xsl:attribute name="regnum"><xsl:value-of select="@regnum"/></xsl:attribute>
	</xsl:otherwise>
      </xsl:choose>
    </xsl:element>
    <xsl:if test="$which &lt; $total">
      <xsl:for-each select="/descendant::reg[$which + 1]">
	<xsl:choose>
	  <xsl:when test="not($thisnum)">
	    <xsl:call-template name="reg">
	      <xsl:with-param name="which" select="$which + 1"/>
	      <xsl:with-param name="nextnum" select="$nextnum + 1"/>
	    </xsl:call-template>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:call-template name="reg">
	      <xsl:with-param name="which" select="$which + 1"/>
	      <xsl:with-param name="nextnum" select="$thisnum + 1"/>
	    </xsl:call-template>
	  </xsl:otherwise>
	</xsl:choose>
      </xsl:for-each>
    </xsl:if>
  </xsl:template>

  <xsl:template match="/">
    <target>
      <xsl:for-each select="/descendant::reg[1]">
	<xsl:call-template name="reg"/>
      </xsl:for-each>
    </target>
  </xsl:template>
</xsl:stylesheet>
