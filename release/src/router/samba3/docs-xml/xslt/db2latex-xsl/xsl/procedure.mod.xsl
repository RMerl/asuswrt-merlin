<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: procedure.mod.xsl,v 1.12 2004/01/13 04:35:43 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
+ ############################################################################## -->

<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="procedure" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: procedure.mod.xsl,v 1.12 2004/01/13 04:35:43 j-devenish Exp $
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
				<doc:revision rcasver="1.10">&rev_2003_05;</doc:revision>
			</revhistory>
		</referenceinfo>
		<title>Procedures <filename>procedure.mod.xsl</filename></title>
		<partintro>
			<para>
			
			
			
			</para>
		</partintro>
	</doc:reference>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>procedure</doc:db> elements</refpurpose>
		<doc:description>
			<para>
			
			Format a titled, enumerated list of steps.
			
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.formal.title.placement"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:params>
			<variablelist>
				<varlistentry>
					<term>mode</term>
					<listitem><simpara>
						Although the <sgmltag class="attribute">mode</sgmltag>
						parameter is normally empty, this template recognises a
						special value of <quote>custom</quote>. This influences
						the type of environment and the method of labelling
						<doc:db basename="step">steps</doc:db>.
					</simpara></listitem>
				</varlistentry>
				<varlistentry>
					<term>environment</term>
					<listitem><simpara>
						This determines the &LaTeX; environment that will be
						used for each <doc:db>step</doc:db>'s <function
						condition="latex">item</function>. When the
						<literal>mode</literal> is <quote>custom</quote>, this
						parameter defaults to <quote>description</quote>.
						Otherwise, the default is <quote>enumerate</quote>.
					</simpara></listitem>
				</varlistentry>
			</variablelist>
		</doc:params>
		<doc:notes>
			<para>
			
			By default, the &LaTeX; <function
			condition="env">enumerate</function> environment is used and any
			<doc:db>step</doc:db>'s <doc:db>title</doc:db> will be typeset
			after its automatic step number. However, when the
			<literal>mode</literal> variable is equal to <quote>custom</quote>,
			the <function condition="env">description</function> environment
			will be used and step titles will be typeset
			<emphasis>instead</emphasis> of step numbers.
			
			</para>
			<para>

			Although the procedure is a formal, titled block, is is not typeset
			using <function condition="latex">subsection</function>.

			</para>
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_book;
				&test_chemistry;
				&test_procedure;
			</simplelist>
		</doc:samples>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="template.procedure/title"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="procedure" name="procedure">
		<xsl:param name="mode" select="''"/>
		<xsl:param name="environment">
			<xsl:choose>
				<xsl:when test="$mode='custom'">
					<xsl:text>description</xsl:text>
				</xsl:when>
				<xsl:otherwise>
					<xsl:text>enumerate</xsl:text>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:param>
		<xsl:variable name="placement">
			<xsl:call-template name="generate.formal.title.placement">
				<xsl:with-param name="object" select="local-name(.)" />
			</xsl:call-template>
		</xsl:variable>
		<xsl:variable name="preamble" select="node()[not(self::blockinfo or self::title or self::subtitle or self::titleabbrev or self::step)]"/>
		<xsl:choose>
			<xsl:when test="$placement='before' or $placement=''">
				<xsl:apply-templates select="title" mode="procedure.title"/>
				<xsl:apply-templates select="$preamble"/>
				<xsl:text>\begin{</xsl:text>
				<xsl:value-of select="$environment"/>
				<xsl:text>}&#10;</xsl:text>
				<xsl:apply-templates select="step">
					<xsl:with-param name="mode" select="$mode"/>
				</xsl:apply-templates>
				<xsl:text>\end{</xsl:text>
				<xsl:value-of select="$environment"/>
				<xsl:text>}&#10;</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<xsl:apply-templates select="$preamble"/>
				<xsl:text>\begin{</xsl:text>
				<xsl:value-of select="$environment"/>
				<xsl:text>}&#10;</xsl:text>
				<xsl:apply-templates select="step">
					<xsl:with-param name="mode" select="$mode"/>
				</xsl:apply-templates>
				<xsl:text>\end{</xsl:text>
				<xsl:value-of select="$environment"/>
				<xsl:text>}&#10;</xsl:text>
				<xsl:apply-templates select="title" mode="procedure.title"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process a <doc:db>procedure</doc:db>'s <doc:db>title</doc:db> </refpurpose>
		<doc:description>
			<para>
			
			Format a special bridgehead.
			
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.procedure.title.style"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.apply.title.templates"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:notes>
			<para>
			
			The title is typeset as a paragraph.
			
			</para>
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_book;
				&test_procedure;
			</simplelist>
		</doc:samples>
	</doc:template>
	<xsl:template match="procedure/title">
		<xsl:text>&#10;&#10;{</xsl:text>
		<xsl:value-of select="$latex.procedure.title.style"/>
		<xsl:text>{</xsl:text>
		<xsl:choose>
			<xsl:when test="$latex.apply.title.templates=1">
				<xsl:apply-templates/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="."/>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:text>}}&#10;</xsl:text>
	</xsl:template>

	<doc:template basename="step" xmlns="">
		<refpurpose>Process <doc:db>step</doc:db> elements </refpurpose>
		<doc:description>
			<para>
			
			Format steps and substeps as part of a procedure.
			
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.step.title.style"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:params>
			<variablelist>
				<varlistentry>
					<term>mode</term>
					<listitem><simpara>

						The <quote>mode</quote> from the parent
						<doc:db>procedure</doc:db>. This template
						needs to know when the <quote>custom</quote>
						mode is in use, because it needs to pass the
						step's title as an optional argument to the
						&LaTeX; <function condition="latex">item</function>
						command (see <xref linkend="template.procedure"/>).
						The mode is normally received from the enclosing
						<doc:db>procedure</doc:db> or <doc:db>substeps</doc:db>
						template.

					</simpara></listitem>
				</varlistentry>
				<varlistentry>
					<term>title</term>
					<listitem><simpara>
						The string (typically empty).
						See <xref linkend="template.generate.step.title"/>
					</simpara></listitem>
				</varlistentry>
			</variablelist>
		</doc:params>
		<doc:notes>
			<para>
			
			Each step is typeset using the &LaTeX; <function condition="latex">item</function> command.
			
			</para>
			<para>
				If there is no <doc:db>title</doc:db> element, the
				step will be numbered automatically by &LaTeX;.
			</para>
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_book;
				&test_chemistry;
				&test_procedure;
			</simplelist>
		</doc:samples>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="template.generate.step.title"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="step" name="step">
		<xsl:param name="mode" select="''"/>
		<xsl:param name="title">
			<xsl:call-template name="generate.step.title">
				<xsl:with-param name="mode" select="$mode"/>
			</xsl:call-template>
		</xsl:param>
		<xsl:choose>
			<xsl:when test="$title!='' and $mode='custom'">
				<xsl:text>&#10;\item[{</xsl:text>
				<xsl:value-of select="$latex.step.title.style"/> <!-- by default \sc -->
				<xsl:text>{</xsl:text>
				<xsl:value-of select="$title"/>
				<xsl:text>}}]&#10;{</xsl:text>
			</xsl:when>
			<xsl:when test="$title!=''">
				<xsl:text>&#10;\item{{</xsl:text>
				<xsl:value-of select="$latex.step.title.style"/> <!-- by default \sc -->
				<xsl:text>{</xsl:text>
				<xsl:value-of select="$title"/>
				<xsl:text>}}&#10;</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text>&#10;\item{</xsl:text>
			</xsl:otherwise>
		</xsl:choose>
		 <xsl:apply-templates select="node()[not(self::title)]"/>
		<xsl:text>}&#10;</xsl:text>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Generate a <doc:db>step</doc:db>'s title </refpurpose>
		<doc:description>
			<para>
			
			By default, simply applies templates for <doc:db>title</doc:db>
			elements.
			
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:params>
			<variablelist>
				<varlistentry>
					<term>mode</term>
					<listitem><simpara>

						See <xref linkend="template.procedure"/>. When the mode
						is <quote>custom</quote>, this template will use the
						XSL <literal>number</literal> element to format a title
						such as "1.", "2.", etc. Otherwise, any
						<doc:db>title</doc:db> elements will be used.

					</simpara></listitem>
				</varlistentry>
			</variablelist>
		</doc:params>
		<doc:notes>
			<para>

				If this template generates no content, the
				<doc:db>step</doc:db> will either be numbered automatically by
				&LaTeX; or left unlabelled (depending on the
				<quote>mode</quote>).

			</para>
		</doc:notes>
	</doc:template>
	<xsl:template name="generate.step.title">
		<xsl:param name="mode"/>
		<xsl:choose>
			<xsl:when test="title">
				<xsl:apply-templates select="title"/>
			</xsl:when>
			<xsl:when test="$mode='custom'">
				<xsl:number format="1."/>
			</xsl:when>
			<!-- otherwise, empty -->
		</xsl:choose>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>substep</doc:db> elements </refpurpose>
		<doc:description>
			<para>
			
			Format substeps as part of a step.
			
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:params>
			<variablelist>
				<varlistentry>
					<term>mode</term>
					<listitem><simpara>
						See <xref linkend="template.procedure"/>.
					</simpara></listitem>
				</varlistentry>
				<varlistentry>
					<term>environment</term>
					<listitem><simpara>
						See <xref linkend="template.procedure"/>.
					</simpara></listitem>
				</varlistentry>
			</variablelist>
		</doc:params>
		<doc:notes>
			<para>
			
			Substeps are typeset by nesting a &LaTeX;
			<function condition="env">enumerate</function> environment.
			
			</para>
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_book;
				&test_procedure;
			</simplelist>
		</doc:samples>
	</doc:template>
	<xsl:template match="substeps">
		<xsl:param name="mode" select="''"/>
		<xsl:param name="environment">
			<xsl:choose>
				<xsl:when test="$mode='custom'">
					<xsl:text>description</xsl:text>
				</xsl:when>
				<xsl:otherwise>
					<xsl:text>enumerate</xsl:text>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:param>
		<xsl:text>\begin{</xsl:text>
		<xsl:value-of select="$environment"/>
		<xsl:text>}&#10;</xsl:text>
		<xsl:apply-templates select="step">
			<xsl:with-param name="mode" select="$mode"/>
		</xsl:apply-templates>
		<xsl:text>\end{</xsl:text>
		<xsl:value-of select="$environment"/>
		<xsl:text>}&#10;</xsl:text>
	</xsl:template>

</xsl:stylesheet>

