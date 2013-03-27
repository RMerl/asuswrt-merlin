<?xml version='1.0'?>

<!-- This file wraps around the DocBook HTML XSL stylesheet to customise
   - some parameters; add the CSS stylesheet, etc.
 -->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version='1.0'
                xmlns="http://www.w3.org/TR/xhtml1/transitional"
                exclude-result-prefixes="#default">

<xsl:import href="http://docbook.sourceforge.net/release/xsl/current/html/chunk.xsl"/>

<xsl:variable name="html.stylesheet">../manual.css</xsl:variable>

<!-- for sections with id attributes, use the id in the filename.
  This produces meaningful (and persistent) URLs for the sections. -->
<xsl:variable name="use.id.as.filename" select="1"/>

<!-- enable the shaded verbatim (programlisting etc) blocks. -->
<!-- <xsl:variable name="shade.verbatim" select="1"/> -->

<!-- add class="programlisting" attribute on the <pre>s from
  <programlisting>s so that the CSS can style them nicely. -->
<xsl:attribute-set name="shade.verbatim.style">
  <xsl:attribute name="class">programlisting</xsl:attribute>
</xsl:attribute-set>

<!-- use sane ANSI C function prototypes -->
<xsl:variable name="funcsynopsis.style" select="'ansi'"/>

<!-- split each sect1 into a separate chunk -->
<xsl:variable name="chunk.first.sections" select="1"/>

<!-- don't generate table of contents within each chapter chunk. -->
<xsl:variable name="generate.chapter.toc" select="0"/>

<xsl:variable name="generate.appendix.toc" select="0"/>

<!-- don't include manual page numbers in refentry cross-references, they
  look weird -->
<xsl:variable name="refentry.xref.manvolnum" select="0"/>

<!-- do generate variablelist's as tables -->
<xsl:variable name="variablelist.as.table" select="1"/>

<!-- and css'ize the tables so they can look pretty -->
<xsl:variable name="table.borders.with.css" select="1"/>

<!-- disable ugly tabular output -->
<xsl:variable name="funcsynopsis.tabular.threshold" select="99999"/>

<!-- change some presentation choices --> 
<xsl:template match="type">
  <xsl:call-template name="inline.italicseq"/>
</xsl:template>

<xsl:template match="parameter">
  <xsl:call-template name="inline.monoseq"/>
</xsl:template>

<!-- enclose the whole funcprototype in <code> -->
<xsl:template match="funcprototype" mode="ansi-nontabular">
  <div class="funcprototype"><code>
     <xsl:apply-templates mode="ansi-nontabular"/>
  </code></div>
</xsl:template>

</xsl:stylesheet>

