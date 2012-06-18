<?xml version='1.0'?>
<!-- 
	DocBook to yodl converter
	
	Lacks support for a few docbook tags, but pretty much all 
	yodl macros are used

	(C) Jelmer Vernooij 					2004
-->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:exsl="http://exslt.org/common"
	version="1.1">

	<xsl:output method="text" encoding="iso-8859-1" standalone="yes" indent="no"/>
	<xsl:strip-space elements="*"/>

	<xsl:template match="refentry">
		<xsl:text>manpage(</xsl:text>
		<xsl:value-of select="refmeta/refentrytitle"/>
		<xsl:text>)()(</xsl:text>
		<xsl:value-of select="refmeta/manvolnum"/>
		<xsl:text>)(package)()&#10;</xsl:text>

		<xsl:apply-templates/>
	</xsl:template>

	<xsl:template match="article">
		<xsl:text>article(</xsl:text>
		<xsl:value-of select="title"/>
		<xsl:text>)(</xsl:text>
		<xsl:text>FIXME</xsl:text>
		<xsl:text>)(</xsl:text>
		<xsl:value-of select="articleinfo/pubdate"/>
		<xsl:text>)</xsl:text>
		<xsl:apply-templates/>
	</xsl:template>

	<xsl:template match="report">
		<xsl:text>report(</xsl:text>
		<xsl:value-of select="title"/>
		<xsl:text>)(</xsl:text>
		<xsl:text>FIXME</xsl:text>
		<xsl:text>)(</xsl:text>
		<xsl:value-of select="articleinfo/pubdate"/>
		<xsl:text>)</xsl:text>
		<xsl:apply-templates/>
	</xsl:template>

	<xsl:template match="book">
		<xsl:text>book(</xsl:text>
		<xsl:value-of select="title"/>
		<xsl:text>)(</xsl:text>
		<xsl:text>FIXME</xsl:text>
		<xsl:text>)(</xsl:text>
		<xsl:value-of select="articleinfo/pubdate"/>
		<xsl:text>)</xsl:text>
		<xsl:apply-templates/>
	</xsl:template>

	<xsl:template match="chapter">
		<xsl:choose>
			<xsl:when test="@id = ''">
				<xsl:text>chapter(</xsl:text>
				<xsl:value-of select="title"/>
				<xsl:text>)&#10;</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text>lchapter(</xsl:text>
				<xsl:value-of select="@id"/>
				<xsl:text>)(</xsl:text>
				<xsl:value-of select="title"/>
				<xsl:text>)&#10;</xsl:text>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:apply-templates/>
	</xsl:template>

	<xsl:template match="citerefentry">
		<xsl:value-of select="refentrytitle"/>
		<xsl:text>(</xsl:text>
		<xsl:value-of select="manvolnum"/>
		<xsl:text>)</xsl:text>
	</xsl:template>

	<xsl:template match="para">
		<xsl:apply-templates/>
		<xsl:text>&#10;&#10;</xsl:text>
	</xsl:template>

	<xsl:template match="formalpara">
		<xsl:text>paragraph(</xsl:text>
		<xsl:value-of select="title"/>
		<xsl:text>)&#10;</xsl:text>
		<xsl:apply-templates/>
	</xsl:template>

	<xsl:template match="part">
		<xsl:text>part(</xsl:text>
		<xsl:value-of select="title"/>
		<xsl:text>)&#10;</xsl:text>
		<xsl:apply-templates/>
	</xsl:template>
	
	<xsl:template match="preface">
		<xsl:text>nchapter(</xsl:text>
		<xsl:value-of select="title"/>
		<xsl:text>)&#10;</xsl:text>
		<xsl:apply-templates/>
	</xsl:template>

	<xsl:template match="quote">
		<xsl:text>"</xsl:text>
		<xsl:apply-templates/>
		<xsl:text>"</xsl:text>
	</xsl:template>

	<xsl:template match="parameter|filename">
		<xsl:text>code(</xsl:text>
		<xsl:apply-templates/>
		<xsl:text>)</xsl:text>
	</xsl:template>

	<xsl:template match="emphasis">
		<xsl:text>em(</xsl:text>
		<xsl:apply-templates/>
		<xsl:text>)</xsl:text>
	</xsl:template>

	<xsl:template match="command">
		<xsl:text>bf(</xsl:text>
		<xsl:apply-templates/>
		<xsl:text>)</xsl:text>
	</xsl:template>

	<xsl:template match="refnamediv">
		<xsl:text>manpagename(</xsl:text>
		<xsl:value-of select="refname"/>
		<xsl:text>)(</xsl:text>
		<xsl:value-of select="refpurpose"/>
		<xsl:text>)&#10;</xsl:text>
	</xsl:template>

	<xsl:template match="refsynopsisdiv">
		<xsl:text>manpagesynopsis()</xsl:text>
	</xsl:template>

	<xsl:template match="refsect1|refsect2">
		<xsl:choose>
			<xsl:when test="title='DESCRIPTION'">
				<xsl:text>&#10;manpagedescription()&#10;&#10;</xsl:text>
			</xsl:when>
			<xsl:when test="title='OPTIONS'">
				<xsl:text>&#10;manpageoptions()&#10;&#10;</xsl:text>
			</xsl:when>
			<xsl:when test="title='FILES'">
				<xsl:text>&#10;manpagefiles()&#10;&#10;</xsl:text>
			</xsl:when>
			<xsl:when test="title='SEE ALSO'">
				<xsl:text>&#10;manpageseealso()&#10;&#10;</xsl:text>
			</xsl:when>
			<xsl:when test="title='DIAGNOSTICS'">
				<xsl:text>&#10;manpagediagnostics()&#10;&#10;</xsl:text>
			</xsl:when>
			<xsl:when test="title='BUGS'">
				<xsl:text>&#10;manpagebugs()&#10;&#10;</xsl:text>
			</xsl:when>
			<xsl:when test="title='AUTHOR'">
				<xsl:text>&#10;manpageauthor()&#10;&#10;</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text>&#10;manpagesection(</xsl:text>
				<xsl:value-of select="title"/>
				<xsl:text>)&#10;&#10;</xsl:text>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:apply-templates/>
	</xsl:template>

	<xsl:template match="orderedlist">
		<xsl:text>startdit()&#10;</xsl:text>
		<xsl:for-each select="listitem">
			<xsl:text>dit() </xsl:text>
			<xsl:copy>
				<xsl:apply-templates/>
			</xsl:copy>
			<xsl:text>&#10;</xsl:text>
		</xsl:for-each>
		<xsl:text>enddit()&#10;</xsl:text>
	</xsl:template>

	<xsl:template match="itemizedlist">
		<xsl:text>startit()&#10;</xsl:text>
		<xsl:for-each select="listitem">
			<xsl:text>it() </xsl:text>
			<xsl:copy>
				<xsl:apply-templates/>
			</xsl:copy>
			<xsl:text>&#10;</xsl:text>
		</xsl:for-each>
		<xsl:text>endit()&#10;</xsl:text>
	</xsl:template>

	<xsl:template match="variablelist">
		<xsl:text>startdit()&#10;</xsl:text>
		<xsl:for-each select="varlistentry">
			<xsl:text>dit(</xsl:text>
			<xsl:copy-of select="term">
				<xsl:apply-templates/>
			</xsl:copy-of>
			<xsl:text>) </xsl:text>
			<xsl:apply-templates select="listitem/para"/>
			<xsl:text>&#10;</xsl:text>
		</xsl:for-each>
		<xsl:text>enddit()&#10;</xsl:text>
	</xsl:template>

	<xsl:template match="anchor">
		<xsl:text>label(</xsl:text>
		<xsl:value-of select="@id"/>
		<xsl:text>)&#10;</xsl:text>
	</xsl:template>

	<xsl:template match="footnote">
		<xsl:text>footnote(</xsl:text>
		<xsl:apply-templates/>
		<xsl:text>)</xsl:text>
	</xsl:template>

	<xsl:template match="toc">
		<xsl:text>gettocstring()&#10;</xsl:text>
	</xsl:template>

	<xsl:template match="ulink">
		<xsl:text>&#10;</xsl:text>
		<xsl:text>url(</xsl:text>
		<xsl:value-of select="url"/>
		<xsl:text>)(</xsl:text>
		<xsl:apply-templates/>
		<xsl:text>)</xsl:text>
	</xsl:template>

	<xsl:template match="link">
		<xsl:text>lref(</xsl:text>
		<xsl:apply-templates/>
		<xsl:text>)(</xsl:text>
		<xsl:value-of select="@linkend"/>
		<xsl:text>)</xsl:text>
	</xsl:template>

	<xsl:template match="index">
		<xsl:text>printindex()&#10;</xsl:text>
	</xsl:template>

	<xsl:template match="sect1">
		<xsl:choose>
			<xsl:when test="@id = ''">
				<xsl:text>sect(</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text>lsect(</xsl:text>
				<xsl:value-of select="@id"/>
				<xsl:text>)(</xsl:text>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:value-of select="title"/>
		<xsl:text>)&#10;</xsl:text>
		<xsl:apply-templates/>
	</xsl:template>

	<xsl:template match="sect2">
		<xsl:choose>
			<xsl:when test="@id = ''">
				<xsl:text>subsect(</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text>lsubsect(</xsl:text>
				<xsl:value-of select="@id"/>
				<xsl:text>)(</xsl:text>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:value-of select="title"/>
		<xsl:text>)&#10;</xsl:text>
		<xsl:apply-templates/>
	</xsl:template>

	<xsl:template match="sect3">
		<xsl:choose>
			<xsl:when test="@id = ''">
				<xsl:text>subsubsect(</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text>lsubsubsect(</xsl:text>
				<xsl:value-of select="@id"/>
				<xsl:text>)(</xsl:text>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:value-of select="title"/>
		<xsl:text>)&#10;</xsl:text>
		<xsl:apply-templates/>
	</xsl:template>

	<xsl:template match="sect4">
		<xsl:choose>
			<xsl:when test="@id = ''">
				<xsl:text>subsubsubsect(</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text>lsubsubsubsect(</xsl:text>
				<xsl:value-of select="@id"/>
				<xsl:text>)(</xsl:text>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:value-of select="title"/>
		<xsl:text>)&#10;</xsl:text>
		<xsl:apply-templates/>
	</xsl:template>

	<xsl:template match="*"/>

</xsl:stylesheet>
