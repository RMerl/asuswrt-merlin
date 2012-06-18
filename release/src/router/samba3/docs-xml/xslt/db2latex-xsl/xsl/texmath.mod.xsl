<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: texmath.mod.xsl,v 1.12 2004/01/03 03:19:08 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
+ ############################################################################## -->

<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="texmath" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: texmath.mod.xsl,v 1.12 2004/01/03 03:19:08 j-devenish Exp $
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
				<doc:revision rcasver="1.11">&rev_2003_05;</doc:revision>
			</revhistory>
		</referenceinfo>
		<title>&LaTeX;-only Commands <filename>texmath.mod.xsl</filename></title>
		<partintro>
			<para>
			
			
			
			</para>
		</partintro>
	</doc:reference>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>alt</doc:db> elements</refpurpose>
		<doc:description>
			<para>
			
			
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara>
					<xref linkend="param.tex.math.in.alt"/>
				</simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:notes>
			<para>

				This template will modify its behaviour based upon its context
				within a &DocBook; document.

				If it is within an <doc:db>inlineequation</doc:db> and it has a
				<sgmltag class="attribute">role</sgmltag> with the value of
				<quote>latex</quote> or <quote>tex</quote>, then it will
				typeset its contents inline as raw &LaTeX; input in mathematics
				mode (using <function condition="latex">ensuremath</function>
				command) if the variable <xref
				linkend="param.tex.math.in.alt"/> is set.

				If it is within an <doc:db>equation</doc:db> or
				<doc:db>informalequation</doc:db> and it has a <sgmltag
				class="attribute">role</sgmltag> with the value of
				<quote>latex</quote> or <quote>tex</quote>, then it will
				typeset its contents as raw &LaTeX; input in a <function
				condition="env">displaymath</function> block environment if the
				variable <xref linkend="param.tex.math.in.alt"/> is set.

				Otherwise, if <xref linkend="param.tex.math.in.alt"/> is
				set, the contents will be typeset as raw &LaTeX; input inline
				(not in maths mode).

				Otherwise, templates will be applied normally (not as raw
				&LaTeX; input).

			</para>
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_book;
				&test_theorem;
			</simplelist>
		</doc:samples>
	</doc:template>
	<xsl:template match="alt">
		<xsl:choose>
			<xsl:when test="ancestor::inlineequation and (@role='tex' or @role='latex' or $tex.math.in.alt='plain' or $tex.math.in.alt='latex')">
				<xsl:text>\ensuremath{</xsl:text>
				<xsl:value-of select="."/>
				<xsl:text>}</xsl:text>
			</xsl:when>
			<xsl:when test="(ancestor::equation|ancestor::informalequation) and (@role='tex' or @role='latex' or $tex.math.in.alt='plain' or $tex.math.in.alt='latex')">
				<xsl:text>\begin{displaymath}</xsl:text>
				<xsl:call-template name="label.id"/>
				<xsl:value-of select="."/>
				<xsl:text>\end{displaymath}&#10;</xsl:text>
			</xsl:when>
			<xsl:when test="$tex.math.in.alt='plain' or $tex.math.in.alt='latex'">
				<xsl:value-of select="."/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:apply-templates/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <sgmltag>latex</sgmltag> and <sgmltag>tex</sgmltag> elements</refpurpose>
		<doc:description>
			<para>
				Passes contents through as raw &LaTeX; text.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				The <sgmltag>latex</sgmltag> and <sgmltag>tex</sgmltag> elements are not
				part of &DocBook;.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="latex|tex">
		<xsl:value-of select="."/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <sgmltag class="attribute">fileref</sgmltag> elements</refpurpose>
		<doc:description>
			<para>
				Inputs a &LaTeX; file.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				For <sgmltag>latex</sgmltag> and <sgmltag>tex</sgmltag> elements with
				<sgmltag class="attribute">fileref</sgmltag> attributes, their contents
				are ignored and their fileref attributes are used as file paths for
				the <function condition="latex">input</function> command.
			</para>
			<para>
				The <sgmltag>latex</sgmltag> and <sgmltag>tex</sgmltag> elements are not
				part of &DocBook;.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="latex[@fileref]|tex[@fileref]">
		<xsl:text>\input{</xsl:text><xsl:value-of select="@fileref"/><xsl:text>}&#10;</xsl:text>
	</xsl:template>

	<xsl:template match="tm[@fileref]">
		<xsl:text>\input{</xsl:text><xsl:value-of select="@fileref"/><xsl:text>}&#10;</xsl:text>
	</xsl:template>

	<xsl:template match="tm[@tex]">
		<xsl:value-of select="@tex"/>
	</xsl:template>

	<xsl:template match="inlinetm[@fileref]">
		<xsl:text>\input{</xsl:text><xsl:value-of select="@fileref"/><xsl:text>}&#10;</xsl:text>
	</xsl:template>

	<xsl:template match="inlinetm[@tex]">
		<xsl:value-of select="@tex"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>inlineequation</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Process inline equations.
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara>
					<xref linkend="param.latex.alt.is.preferred"/>
				</simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:notes>
			<para>

				This template can accommodate raw &LaTeX; mathematics.

				If the element has a
				<sgmltag class="attribute">role</sgmltag> with a value of
				<quote>latex</quote> or <quote>tex</quote>, or contains
				an <doc:db>inlinemediaobject</doc:db>/<doc:db>textobject</doc:db>
				or 
				an <doc:db>inlinemediaobject</doc:db>/<doc:db>textobject</doc:db>/<doc:db>phrase</doc:db>
				with such as attribute,
				then it will
				typeset then in preference to all other content.

			</para>
			<para>
				
				If such elements were not found, the template will search for
				generic <doc:db>inlinemediaobject</doc:db> or
				<doc:db>alt</doc:db> children. If <xref
				linkend="param.latex.alt.is.preferred"/> is set, <sgmltag
				basename="alt">alts</sgmltag> will be preferred over <sgmltag
				basename="inlinemediaobject">inlinemediaobjects</sgmltag>. If
				none of these elements was found, the template will format any
				<doc:db>graphic</doc:db> children.
				
			</para>
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_book;
				<!--
				&test_pavlov;
				-->
			</simplelist>
		</doc:samples>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="template.alt"/></simpara></listitem>
				<listitem><simpara><xref linkend="template.phrase"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="inlineequation">
		<xsl:variable name="tex" select="alt[@role='tex' or @role='latex']|inlinemediaobject/textobject[@role='tex' or @role='latex']|inlinemediaobject/textobject/phrase[@role='tex' or @role='latex']" />
		<xsl:choose>
			<xsl:when test="$tex">
				<xsl:apply-templates select="$tex"/>
			</xsl:when>
			<xsl:when test="alt and $latex.alt.is.preferred='1'">
				<xsl:apply-templates select="alt"/>
			</xsl:when>
			<xsl:when test="inlinemediaobject">
				<xsl:apply-templates select="inlinemediaobject"/>
			</xsl:when>
			<xsl:when test="alt">
				<xsl:apply-templates select="alt"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:apply-templates select="graphic"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

</xsl:stylesheet>
