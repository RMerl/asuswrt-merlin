<?xml version='1.0'?>

<!-- This file wraps around the DocBook XSL manpages stylesheet to customise
   - some parameters; add the CSS stylesheet, etc.
 -->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version='1.0'
                xmlns="http://www.w3.org/TR/xhtml1/transitional"
                exclude-result-prefixes="#default">

<xsl:import href="http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl"/>

<!-- for sections with id attributes, use the id in the filename.
  This produces meaningful (and persistent) URLs for the sections. -->
<xsl:variable name="man.output.quietly" select="1"/>

<!-- suppress the copyright section since the /book/bookinfo stuff isn't
     getting used -->
<xsl:variable name="man.output.copyright.info" select="0"/>

<xsl:variable name="refentry.meta.get.quietly" select="1"/>

</xsl:stylesheet>
