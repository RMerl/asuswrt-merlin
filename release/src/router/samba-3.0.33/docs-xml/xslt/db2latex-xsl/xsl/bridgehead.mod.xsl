<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: bridgehead.mod.xsl,v 1.12 2004/01/11 11:35:25 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
+ ############################################################################## -->

<xsl:stylesheet 
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="bridgehead" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: bridgehead.mod.xsl,v 1.12 2004/01/11 11:35:25 j-devenish Exp $
			</releaseinfo>
			<authorgroup>
				&ramon;
				&james;
			</authorgroup>
			<copyright>
				<year>2000</year><year>2001</year><year>2002</year><year>2003</year>
				<holder>Ramon Casellas</holder>
			</copyright>
			<revhistory>
				<doc:revision rcasver="1.10">&rev_2003_05;</doc:revision>
			</revhistory>
		</referenceinfo>
		<title>Free-floating Headings <filename>bridgehead.mod.xsl</filename></title>
		<partintro>
			<para>The file <filename>bridgehead.mod.xsl</filename> contains the
			XSL template for <doc:db>bridgehead</doc:db>.</para>
		</partintro>
	</doc:reference>

	<doc:template basename="bridgehead" xmlns="">
		<refpurpose>Process <doc:db>bridgehead</doc:db> elements</refpurpose>
		<doc:description>
			<para>

				Free-floating headings for <doc:db
				basename="bridgehead">bridgeheads</doc:db> elements. Renders
				un-numbered section headings.

			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:params>
			<variablelist>
				<varlistentry>
					<term>renderas</term>
					<listitem><simpara>The sectioning level to be emulated (e.g. <quote>1</quote> for <doc:db>sect1</doc:db>). Chooses the current node's <literal>@renderas</literal> attribute by default.</simpara></listitem>
				</varlistentry>
				<varlistentry>
					<term>content</term>
					<listitem><simpara>The content that forms the bridgehead text. By default, normal templates will be applied for the current node.</simpara></listitem>
				</varlistentry>
			</variablelist>
		</doc:params>
		<doc:notes>
			<para>
				
				The emulation of section headings is achieved through the
				normal &latex; section commands such as
				<function condition="latex">section</function>, <function condition="latex">subsection</function>,
				and so forth. However, levels above three (3) will be typeset
				with the &latex; <function condition="latex">paragraph*</function> command.
				
			</para>
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_book;
				&test_subfig;
				&test_tables;
			</simplelist>
		</doc:samples>
		<!--
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara>&mapping;</simpara></listitem>
			</itemizedlist>
		</doc:seealso>
		-->
	</doc:template>
	<xsl:template match="bridgehead" name="bridgehead">
		<xsl:param name="renderas" select="@renderas"/>
		<xsl:param name="content"><xsl:apply-templates/></xsl:param>
		<xsl:choose>
			<xsl:when test="$renderas='sect1' or $renderas='sect2' or $renderas='sect3'">
				<xsl:text>&#10;\</xsl:text>
				<xsl:if test="$renderas='sect2'"><xsl:text>sub</xsl:text></xsl:if>
				<xsl:if test="$renderas='sect3'"><xsl:text>subsub</xsl:text></xsl:if>
				<xsl:text>section*{</xsl:text>
				<xsl:copy-of select="$content"/>
				<xsl:text>}</xsl:text>
				<xsl:call-template name="label.id"/>
				<xsl:text>&#10;</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<!--
				<xsl:text>&#10;&#10;</xsl:text>
				<xsl:text>\vspace{1em}\noindent{\bfseries </xsl:text><xsl:copy-of select="$content"/><xsl:text>}</xsl:text>
				<xsl:call-template name="label.id"/>
				<xsl:text>\par\noindent&#10;</xsl:text>
				-->
				<xsl:text>&#10;\paragraph*{</xsl:text>
				<xsl:copy-of select="$content"/>
				<xsl:text>}</xsl:text>
				<xsl:call-template name="label.id"/>
				<xsl:text>&#10;&#10;\noindent&#10;</xsl:text>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

</xsl:stylesheet>
