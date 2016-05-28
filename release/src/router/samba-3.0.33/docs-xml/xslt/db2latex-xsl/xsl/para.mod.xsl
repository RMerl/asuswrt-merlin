<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: para.mod.xsl,v 1.16 2004/01/13 14:17:45 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
|														
|   PURPOSE:
+ ############################################################################## -->
<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>


	<!--############################################################################# -->
	<!-- DOCUMENTATION                                                                -->
	<doc:reference id="para" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: para.mod.xsl,v 1.16 2004/01/13 14:17:45 j-devenish Exp $
			</releaseinfo>
			<authorgroup>
				&ramon;
				&james;
			</authorgroup>
			<copyright>
				<year>2000</year> <year>2001</year> <year>2002</year> <year>2003</year>
				<holder>Ramon Casellas</holder>
			</copyright>
			<revhistory>
				<doc:revision rcasver="1.8">&rev_2003_05;</doc:revision>
			</revhistory>
		</referenceinfo>
		<title>Paragraphs <filename>para.mod.xsl</filename></title>
		<partintro>
			<para>The file <filename>para.mod.xsl</filename> contains the
			XSL template for <doc:db>para</doc:db>, <doc:db>simpara</doc:db> and <doc:db>formalpara</doc:db>.</para>
			<doc:variables>
				<itemizedlist>
					<listitem><simpara><xref linkend="param.latex.use.parskip"/></simpara></listitem>
					<listitem><simpara><xref linkend="param.latex.formalpara.title.style"/></simpara></listitem>
				</itemizedlist>
			</doc:variables>
		</partintro>
	</doc:reference>

	<doc:template xmlns="">
		<refpurpose>Use normal paragraph spacing instead of parskip spacing</refpurpose>
		<doc:description>
			<para>
				Uses <function condition="latex">docbooktolatexnoparskip</function>.
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.use.parskip"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:notes>
			&essential_preamble;
		</doc:notes>
	</doc:template>
	<xsl:template name="latex.noparskip">
		<xsl:if test="$latex.use.parskip=1">
			<xsl:text>\docbooktolatexnoparskip&#10;</xsl:text>
		</xsl:if>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Use parkip spacing, if user desires it</refpurpose>
		<doc:description>
			<para>
				Uses <function condition="latex">docbooktolatexrestoreparskip</function>.
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.use.parskip"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:notes>
			&essential_preamble;
		</doc:notes>
	</doc:template>
	<xsl:template name="latex.restoreparskip">
		<xsl:if test="$latex.use.parskip=1">
			<xsl:text>\docbooktolatexrestoreparskip&#10;</xsl:text>
		</xsl:if>
	</xsl:template>

	<doc:template basename="para" match="para|simpara" xmlns="">
		<refpurpose>Process <doc:db>para</doc:db> and <doc:db>simpara</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Starts new lines above and below its contents.
				Thus, consecutive <doc:db basename="para">paras</doc:db> will have
				one blank line between them.
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.use.parskip"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:notes>
			<para>In &latex;, there is no distinction between <doc:db>para</doc:db> and <doc:db>simpara</doc:db>.</para>
			<para>The accuracy of block elements within <sgmltag basename="para">paras</sgmltag> is unknown.</para>
			<para><doc:todo>The use of <sgmltag>para</sgmltag> within <doc:db basename="footnote">footnotes</doc:db> is unproven.</doc:todo></para>
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_blocks;
			</simplelist>
		</doc:samples>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="template.para-noline"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="para|simpara">
		<xsl:text>&#10;</xsl:text>
		<xsl:apply-templates/>
		<xsl:text>&#10;</xsl:text>
	</xsl:template>

	<doc:template match="formalpara" xmlns="">
		<refpurpose>Process <doc:db>formalpara</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Starts new lines above and below its contents.
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara>The <doc:db>title</doc:db> is typeset using <xref linkend="param.latex.formalpara.title.style"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:notes>
			<para>The accuracy of block elements within <doc:db basename="formalpara">formalparas</doc:db> is unknown.</para>
			<para><doc:todo>The use of <sgmltag>formalpara</sgmltag> within <doc:db basename="footnote">footnotes</doc:db> is unproven.</doc:todo></para>
			<para>Calls <xref linkend="template.generate.formalpara.title.delimiter"/>.</para>
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_blocks;
			</simplelist>
		</doc:samples>
	</doc:template>
	<xsl:template match="formalpara">
		<xsl:text>&#10;{</xsl:text>
		<xsl:value-of select="$latex.formalpara.title.style"/>
		<xsl:text>{{</xsl:text>
		<xsl:apply-templates select="title"/>
		<xsl:text>}</xsl:text>
		<xsl:call-template name="generate.formalpara.title.delimiter"/>
		<xsl:text>}}\ </xsl:text>
		<xsl:apply-templates select="node()[not(self::title)]"/>
		<xsl:text>&#10;</xsl:text>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Delimite a <doc:db>formalpara</doc:db>'s <doc:db>title</doc:db> from its <doc:db>para</doc:db></refpurpose>
		<doc:description>
			<para>
				Emits a full stop (period).
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>

				This template can be used to emit any &LaTeX; sequence that you
				desire. You can make the appearance be conditional upon some
				attribute or document location, for instance. However, a
				'space' will <emphasis>always</emphasis> be generated between
				this delimiter and the subsequent <doc:db>para</doc:db>
				contents.

			</para>
		</doc:notes>
	</doc:template>
	<xsl:template name="generate.formalpara.title.delimiter">
		<xsl:text>.</xsl:text>
	</xsl:template>

	<doc:template basename="para" xmlns="">
		<refpurpose>Suppressed paragraphs</refpurpose>
		<doc:description>
			<para>
				These paragraphs are not separated like normal paragraphs.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>This template exists to handle &latex; problems with
			<function condition="latex">par</function> in certain contexts. <doc:todo>These
			problems should be periodically reviewed by the &db2latex; team.</doc:todo></para>
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_blocks;
			</simplelist>
		</doc:samples>
	</doc:template>
	<xsl:template match="textobject/para|step/para|entry/para|question/para" name="para-noline">
		<xsl:if test="position()&gt;1">
			<xsl:text> </xsl:text>
		</xsl:if>
		<xsl:apply-templates/>
		<xsl:if test="position()&lt;last()">
			<xsl:text> </xsl:text>
		</xsl:if>
	</xsl:template>

</xsl:stylesheet>
