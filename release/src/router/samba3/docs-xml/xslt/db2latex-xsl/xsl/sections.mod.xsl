<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: sections.mod.xsl,v 1.8 2004/01/03 12:19:15 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
+ ############################################################################## -->

<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="sections" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: sections.mod.xsl,v 1.8 2004/01/03 12:19:15 j-devenish Exp $
			</releaseinfo>
			<authorgroup>
				&ramon;
				&james;
			</authorgroup>
			<copyright>
				<year>2000</year><year>2001</year><year>2002</year><year>2003</year><year>2004</year>
				<holder>Ramon Casellas</holder>
			</copyright>
			<revhistory>
				<doc:revision rcasver="1.6">&rev_2003_05;</doc:revision>
			</revhistory>
		</referenceinfo>
		<title>Sections <filename>sections.mod.xsl</filename></title>
		<partintro>
			<para>The file <filename>sections.mod.xsl</filename> contains the
			XSL templates for <doc:db>section</doc:db>, <doc:db>simplesect</doc:db>,
			and <doc:db>sect1</doc:db>, etc.</para>
		</partintro>
	</doc:reference>

	<doc:template xmlns="">
		<refpurpose>Process explicitly-nested sections</refpurpose>
		<doc:description>
			<para>
				Formats a section's title (including numbering, if applicable)
				then allows all child elements to be processed. The title
				is formatted the using &LaTeX; mapping identified by the
				element name (e.g. <doc:db>sect1</doc:db>, etc.).
			</para>
		</doc:description>
		<doc:params>
			<variablelist>
				<varlistentry>
					<term>bridgehead</term>
					<listitem>
						<para>

							If <literal>true()</literal>, the title is
							processed like a <doc:db>bridgehead</doc:db> (that
							is, the section is unnumbered and does not appear
							in the <doc:db>toc</doc:db>). This parameter
							defaults to <literal>false()</literal> unless the
							section appears within a preface.

						</para>
					</listitem>
				</varlistentry>
			</variablelist>
		</doc:params>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>

			<para>Currently, <quote>info</quote> elements (e.g.
			<doc:db>sect1info</doc:db>), <doc:db>subtitle</doc:db> and
			<doc:db>titleabbrev</doc:db> are not honoured.</para>

			<para>The use of special components such as <doc:db>toc</doc:db>,
			<doc:db>lot</doc:db>, <doc:db>index</doc:db> and
			<doc:db>glossary</doc:db> is unlikely to be successful.</para>

		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_article;
				&test_book;
			</simplelist>
		</doc:samples>
		<doc:seealso>
			<itemizedlist>
				<listitem><para>&mapping;</para></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>

	<xsl:template match="sect1|sect2|sect3|sect4|sect5">
		<xsl:param name="bridgehead" select="ancestor::preface|ancestor::colophon|ancestor::dedication|ancestor::partintro"/>
		<xsl:variable name="template">
			<xsl:value-of select="local-name(.)"/>
			<xsl:if test="$bridgehead"><xsl:text>*</xsl:text></xsl:if>
		</xsl:variable>
		<xsl:call-template name="map.begin">
			<xsl:with-param name="keyword" select="$template"/>
		</xsl:call-template>
		<xsl:call-template name="content-templates"/>
		<xsl:call-template name="map.end">
			<xsl:with-param name="keyword" select="$template"/>
		</xsl:call-template>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process recursive and leaf sections</refpurpose>
		<doc:description>
			<para>
				Formats a section's title (including numbering, if applicable)
				then allows all child elements to be processed. The title
				is formatted the using &LaTeX; mapping identified by the
				equivalent explicit element (e.g. <doc:db>sect1</doc:db>, etc.).
			</para>
		</doc:description>
		<doc:params>
			<variablelist>
				<varlistentry>
					<term>bridgehead</term>
					<listitem>
						<para>

							If <literal>true()</literal>, the title is
							processed like a <doc:db>bridgehead</doc:db> (that
							is, the section is unnumbered and does not appear
							in the <doc:db>toc</doc:db>). This parameter
							defaults to <literal>false()</literal> unless the
							section appears within a preface.

						</para>
					</listitem>
				</varlistentry>
				<varlistentry>
					<term>level</term>
					<listitem>
						<para>

						The numeric nesting level of the section. This is
						automatically calculated as "one greater than the
						number of ancestor <doc:db>section</doc:db>s".

						</para>
					</listitem>
				</varlistentry>
			</variablelist>
		</doc:params>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>

			<para>Titles for sections nested beyond a depth of five are
			formatted like <doc:db>sect5</doc:db>.</para>

			<para>Currently, <quote>info</quote> elements (e.g.
			<doc:db>sectioninfo</doc:db>), <doc:db>subtitle</doc:db> and
			<doc:db>titleabbrev</doc:db> are not honoured.</para>

			<para>The use of special components such as <doc:db>toc</doc:db>,
			<doc:db>lot</doc:db>, <doc:db>index</doc:db> and
			<doc:db>glossary</doc:db> is unlikely to be successful.</para>

			<para>&LaTeX; makes no semantic distinction between
			<doc:db>section</doc:db> and <doc:db>simplesect</doc:db>.</para>

		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_article;
				&test_book;
			</simplelist>
		</doc:samples>
		<doc:seealso>
			<itemizedlist>
				<listitem><para>&mapping;</para></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="section|simplesect">
		<xsl:param name="bridgehead" select="ancestor::preface|ancestor::colophon|ancestor::dedication"/>
		<xsl:param name="level" select="count(ancestor::section)+1"/>
		<xsl:variable name="template">
			<xsl:choose>
				<xsl:when test='$level&lt;6'>
					<xsl:text>sect</xsl:text>
					<xsl:value-of select="$level"/>
				</xsl:when>
				<xsl:otherwise>
					<xsl:message>DB2LaTeX: recursive section|simplesect &gt; 5 not well supported.</xsl:message>
					<xsl:text>sect6</xsl:text>
				</xsl:otherwise>
			</xsl:choose>
			<xsl:if test="$bridgehead"><xsl:text>*</xsl:text></xsl:if>
		</xsl:variable>
		<xsl:text>&#10;</xsl:text>
		<xsl:call-template name="map.begin">
			<xsl:with-param name="keyword" select="$template"/>
		</xsl:call-template>
		<xsl:call-template name="content-templates"/>
		<xsl:call-template name="map.end">
			<xsl:with-param name="keyword" select="$template"/>
		</xsl:call-template>
	</xsl:template>

</xsl:stylesheet>
