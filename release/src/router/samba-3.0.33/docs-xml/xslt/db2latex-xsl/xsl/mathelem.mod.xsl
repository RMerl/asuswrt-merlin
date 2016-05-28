<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: mathelem.mod.xsl,v 1.4 2004/01/02 05:03:28 j-devenish Exp $		
|- #############################################################################
|	$Author: j-devenish $
+ ############################################################################## -->

<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="mathelems" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: mathelem.mod.xsl,v 1.4 2004/01/02 05:03:28 j-devenish Exp $
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
				<doc:revision rcasver="1.4">&rev_2003_05;</doc:revision>
			</revhistory>
		</referenceinfo>
		<title>Mathematics <filename>mathelems.mod.xsl</filename></title>
		<partintro>
			<para>
			
			Mathelements as theorems, lemmas, propositions, etc. Note: these
			elements are not part of the &DocBook; DTD. I have extended the
			&DocBook; in order to support this tags, so that's why I have these
			templates here.
			
			</para>
		</partintro>
	</doc:reference>

	<doc:template xmlns="">
		<refpurpose>Process <sgmltag>mathelement</sgmltag> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="mathelement">
		<xsl:apply-templates/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Typeset mathelement environments</refpurpose>
		<doc:description>
			<para>

				Applies templates within the specified &LaTeX; environment,
				with a title from any <doc:db>title</doc:db> child.
				<doc:todo>No hypertarget is generated.</doc:todo>

			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:params>
			<variablelist>
				<varlistentry>
					<term>environment</term>
					<listitem><simpara>The name of the &LaTeX; environment command.</simpara></listitem>
				</varlistentry>
			</variablelist>
		</doc:params>
	</doc:template>
	<xsl:template name="mathelement.environment">
		<xsl:param name="environment" select="'hypothesis'"/>
		<xsl:text>\begin{</xsl:text>
		<xsl:value-of select="$environment"/>
		<xsl:text>}[{</xsl:text>
		<xsl:call-template name="normalize-scape">
			<xsl:with-param name="string" select="title"/> 
		</xsl:call-template>
		<xsl:text>}]&#10;</xsl:text>
		<xsl:variable name="id"> <xsl:call-template name="label.id"/> </xsl:variable>
		<xsl:call-template name="content-templates"/>
		<xsl:text>\end{</xsl:text>
		<xsl:value-of select="$environment"/>
		<xsl:text>}&#10;</xsl:text>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <sgmltag>mathelement/mathhypothesis</sgmltag> elements</refpurpose>
		<doc:description>
			<para>
				Formats a hypothesis.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				Calls <xref linkend="template.mathelement.environment"/>
				for the <quote>hypothesis</quote> environment.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="mathelement/mathhypothesis">
		<xsl:call-template name="mathelement.environment">
			<xsl:with-param name="environment" select="'hypothesis'"/>
		</xsl:call-template>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <sgmltag>mathelement/mathremark</sgmltag> elements</refpurpose>
		<doc:description>
			<para>
				Formats a mathematical remark.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				Calls <xref linkend="template.mathelement.environment"/>
				for the <quote>rem</quote> environment.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="mathelement/mathremark">
		<xsl:call-template name="mathelement.environment">
			<xsl:with-param name="environment" select="'rem'"/>
		</xsl:call-template>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <sgmltag>mathelement/mathexample</sgmltag> elements</refpurpose>
		<doc:description>
			<para>
				Formats a mathematical example.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				Calls <xref linkend="template.mathelement.environment"/>
				for the <quote>exm</quote> environment.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="mathelement/mathexample">
		<xsl:call-template name="mathelement.environment">
			<xsl:with-param name="environment" select="'exm'"/>
		</xsl:call-template>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <sgmltag>mathelement/mathproposition</sgmltag> elements</refpurpose>
		<doc:description>
			<para>
				Formats a mathematical proposition.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				Calls <xref linkend="template.mathelement.environment"/>
				for the <quote>prop</quote> environment.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="mathelement/mathproposition">
		<xsl:call-template name="mathelement.environment">
			<xsl:with-param name="environment" select="'prop'"/>
		</xsl:call-template>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <sgmltag>mathelement/maththeorem</sgmltag> elements</refpurpose>
		<doc:description>
			<para>
				Formats a mathematical theorem.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				Calls <xref linkend="template.mathelement.environment"/>
				for the <quote>thm</quote> environment.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="mathelement/maththeorem">
		<xsl:call-template name="mathelement.environment">
			<xsl:with-param name="environment" select="'thm'"/>
		</xsl:call-template>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <sgmltag>mathelement/mathdefinition</sgmltag> elements</refpurpose>
		<doc:description>
			<para>
				Formats a mathematical definition.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				Calls <xref linkend="template.mathelement.environment"/>
				for the <quote>defn</quote> environment.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="mathelement/mathdefinition">
		<xsl:call-template name="mathelement.environment">
			<xsl:with-param name="environment" select="'defn'"/>
		</xsl:call-template>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <sgmltag>mathelement/mathlemma</sgmltag> elements</refpurpose>
		<doc:description>
			<para>
				Formats a mathematical lemma.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				Calls <xref linkend="template.mathelement.environment"/>
				for the <quote>lem</quote> environment.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="mathelement/mathlemma">
		<xsl:call-template name="mathelement.environment">
			<xsl:with-param name="environment" select="'lem'"/>
		</xsl:call-template>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <sgmltag>mathelement/mathproof</sgmltag> elements</refpurpose>
		<doc:description>
			<para>
				Formats a mathematical proof.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				Calls <xref linkend="template.mathelement.environment"/>
				for the <quote>proof</quote> environment.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="mathelement/mathproof">
		<xsl:call-template name="mathelement.environment">
			<xsl:with-param name="environment" select="'proof'"/>
		</xsl:call-template>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <sgmltag>mathphrase</sgmltag>, <sgmltag>mathcondition</sgmltag> and <sgmltag>mathassertion</sgmltag> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="mathphrase|mathcondition|mathassertion">
		<xsl:apply-templates/>
	</xsl:template>

</xsl:stylesheet>
