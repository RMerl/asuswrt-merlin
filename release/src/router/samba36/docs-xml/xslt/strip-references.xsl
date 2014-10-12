<?xml version='1.0'?>
<!-- Removes particular (unuseful for the book) elements from references -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	version="1.1">
	<xsl:template match="reference/refentry/refsect1[title='VERSION' or title='AUTHOR']"/>
	<xsl:template match="reference/refentry/refmeta"/>
	<xsl:template match="reference/refentry/refnamediv"/>
</xsl:stylesheet>
