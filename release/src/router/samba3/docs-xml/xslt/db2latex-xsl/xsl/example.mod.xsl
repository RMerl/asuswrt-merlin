<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: example.mod.xsl,v 1.9 2004/01/26 09:40:43 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
+ ############################################################################## -->

<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="example" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: example.mod.xsl,v 1.9 2004/01/26 09:40:43 j-devenish Exp $
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
				<doc:revision rcasver="1.7">&rev_2003_05;</doc:revision>
			</revhistory>
		</referenceinfo>
		<title>Examples <filename>example.mod.xsl</filename></title>
		<partintro>
			<para>The file <filename>example.mod.xsl</filename> contains the
			XSL templates for <doc:db>example</doc:db> and
			<doc:db>informalexample</doc:db>.</para>
		</partintro>
	</doc:reference>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>example</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Format a caption.
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.example.caption.style"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<!--
		<doc:notes>
		</doc:notes>
		-->
		<doc:samples>
			<simplelist type='inline'>
				&test_book;
				&test_program;
			</simplelist>
		</doc:samples>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="template.generate.formal.title.placement"/></simpara></listitem>
				<listitem><simpara>&mapping;</simpara></listitem>
				<listitem><simpara><xref linkend="template.title-caption.mode"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="example">
		<xsl:variable name="placement">
			<xsl:call-template name="generate.formal.title.placement">
				<xsl:with-param name="object" select="local-name(.)" />
			</xsl:call-template>
		</xsl:variable>
		<xsl:variable name="caption">
			<xsl:text>{</xsl:text>
			<xsl:value-of select="$latex.example.caption.style"/>
			<xsl:text>{\caption{</xsl:text>
			<xsl:apply-templates select="title" mode="caption.mode"/>
			<xsl:text>}</xsl:text>
			<xsl:call-template name="label.id"/>
			<xsl:text>}}&#10;</xsl:text>
		</xsl:variable>
		<xsl:call-template name="map.begin"/>
		<xsl:if test="$placement='before'">
			<xsl:text>\captionswapskip{}</xsl:text>
			<xsl:value-of select="$caption" />
			<xsl:text>\captionswapskip{}</xsl:text>
		</xsl:if>
		<xsl:call-template name="content-templates"/>
		<xsl:if test="$placement!='before'"><xsl:value-of select="$caption" /></xsl:if>
		<xsl:call-template name="map.end"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>informalexample</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Calls <xref linkend="template.informal.object"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<!--
		<doc:notes>
		</doc:notes>
		-->
		<doc:samples>
			<simplelist type='inline'>
				&test_book;
			</simplelist>
		</doc:samples>
	</doc:template>
	<xsl:template match="informalexample">
		<xsl:call-template name="informal.object"/>
	</xsl:template>

</xsl:stylesheet>
