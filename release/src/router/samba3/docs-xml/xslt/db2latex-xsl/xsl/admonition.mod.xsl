<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: admonition.mod.xsl,v 1.14 2004/01/03 09:48:34 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
+ ############################################################################## -->
<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference name="admonition" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: admonition.mod.xsl,v 1.14 2004/01/03 09:48:34 j-devenish Exp $
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
				<doc:revision rcasver="1.6">&rev_2003_05;</doc:revision>
			</revhistory>
		</referenceinfo>
		<title>Admonitions <filename>admonition.mod.xsl</filename></title>
		<partintro>
			<para>
			
			&DocBook; includes admonitions (<doc:db>warning</doc:db>,
			<doc:db>tip</doc:db>, <doc:db>important</doc:db>,
			<doc:db>caution</doc:db>, <doc:db>note</doc:db>), which are set off
			from the main text. &DB2LaTeX; places these in the normal text flow
			but surrounds them with a box border.
			
			</para>
			<doc:variables>
				<itemizedlist>
					<listitem><simpara><xref linkend="param.latex.use.fancybox"/></simpara></listitem>
					<listitem><simpara><xref linkend="param.admon.graphics.path"/></simpara></listitem>
					<listitem><simpara><xref linkend="param.latex.admonition.imagesize"/></simpara></listitem>
					<listitem><simpara><xref linkend="param.latex.apply.title.templates.admonitions"/></simpara></listitem>
				</itemizedlist>
			</doc:variables>
		</partintro>
	</doc:reference>

	<doc:param xmlns="">
		<refpurpose>
			Declares a new environment to be used for admonitions
		</refpurpose>
		<doc:description>
			<para>

			This &LaTeX; environment is emitted during the preamble. That
			environment has two mandatory parameters. Instances of the
			environment are customised for each admonition via those
			parameters. Instances will be typeset as boxed areas in the
			document flow.

			</para>
			<para>

			The first argument is the filename for graphics (e.g.
			<filename>$admon.graphics.path/warning</filename>).
			The second argument is the admonition title or the associated
			generic text.

			</para>

			<example>
				<title>Processing the <doc:db>warning</doc:db> admonition</title>
				<para> When processing the admonition, the following code is generated: </para>
				<programlisting>
				<![CDATA[\begin{admonition}{figures/warning}{My WARNING}
...
\end{admonition}]]>
				</programlisting>
			</example>
		</doc:description>
		<doc:notes>
			<!-- notes about spacing? -->
			<para>
				The environment uses graphics by default. This may generate errors or warnings
				if &LaTeX; cannot find the graphics. If necessary, graphics may be disabled
				via <xref linkend="param.admon.graphics.path"/>.
			</para>
		</doc:notes>
		<!--
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="template.para"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
		-->
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.use.fancybox"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.admon.graphics.path"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.admonition.imagesize"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
	</doc:param>

	<xsl:param name="latex.admonition.environment">
		<xsl:text>% ----------------------------------------------&#10;</xsl:text>
		<xsl:text>% Define a new LaTeX environment (adminipage)&#10;</xsl:text>
		<xsl:text>% ----------------------------------------------&#10;</xsl:text>
		<xsl:text>\newenvironment{admminipage}%&#10;</xsl:text>
		<xsl:text>{ % this code corresponds to the \begin{adminipage} command&#10;</xsl:text>
		<xsl:text> \begin{Sbox}%&#10;</xsl:text>
		<xsl:text> \begin{minipage}%&#10;</xsl:text>
		<xsl:text>} %done&#10;</xsl:text>
		<xsl:text>{ % this code corresponds to the \end{adminipage} command&#10;</xsl:text>
		<xsl:text> \end{minipage}&#10;</xsl:text>
		<xsl:text> \end{Sbox}&#10;</xsl:text>
		<xsl:text> \fbox{\TheSbox}&#10;</xsl:text>
		<xsl:text>} %done&#10;</xsl:text>
		<xsl:text>% ----------------------------------------------&#10;</xsl:text>
		<xsl:text>% Define a new LaTeX length (admlength)&#10;</xsl:text>
		<xsl:text>% ----------------------------------------------&#10;</xsl:text>
		<xsl:text>\newlength{\admlength}&#10;</xsl:text>
		<xsl:text>% ----------------------------------------------&#10;</xsl:text>
		<xsl:text>% Define a new LaTeX environment (admonition)&#10;</xsl:text>
		<xsl:text>% With 2 parameters:&#10;</xsl:text>
		<xsl:text>% #1 The file (e.g. note.pdf)&#10;</xsl:text>
		<xsl:text>% #2 The caption&#10;</xsl:text>
		<xsl:text>% ----------------------------------------------&#10;</xsl:text>
		<xsl:text>\newenvironment{admonition}[2] &#10;</xsl:text>
		<xsl:text>{ % this code corresponds to the \begin{admonition} command&#10;</xsl:text>
		<xsl:text> \hspace{0mm}\newline\hspace*\fill\newline&#10;</xsl:text>
		<xsl:text> \noindent&#10;</xsl:text>
		<xsl:text> \setlength{\fboxsep}{5pt}&#10;</xsl:text>
		<xsl:text> \setlength{\admlength}{\linewidth}&#10;</xsl:text>
		<xsl:text> \addtolength{\admlength}{-10\fboxsep}&#10;</xsl:text>
		<xsl:text> \addtolength{\admlength}{-10\fboxrule}&#10;</xsl:text>
		<xsl:text> \admminipage{\admlength}&#10;</xsl:text>
		<xsl:text> {\bfseries \sc\large{#2}}</xsl:text>
		<xsl:text> \newline&#10;</xsl:text>
		<xsl:text> \\[1mm]&#10;</xsl:text>
		<xsl:text> \sffamily&#10;</xsl:text>
		<!--
		If we cannot find the admon.graphics.path;
		Comment out the next line (\includegraphics).
		This tactic is to avoid deleting the \includegraphics
		altogether, as that could confuse a person trying to
		find the use of parameter #1 in the environment.
		-->
		<xsl:if test="$admon.graphics.path=''">
			<xsl:text>%</xsl:text>
		</xsl:if>
		<xsl:text> \includegraphics[</xsl:text> <xsl:value-of select="$latex.admonition.imagesize" /> <xsl:text>]{#1}&#10;</xsl:text>
		<xsl:text> \addtolength{\admlength}{-1cm}&#10;</xsl:text>
		<xsl:text> \addtolength{\admlength}{-20pt}&#10;</xsl:text>
		<xsl:text> \begin{minipage}[lt]{\admlength}&#10;</xsl:text>
		<xsl:text> \parskip=0.5\baselineskip \advance\parskip by 0pt plus 2pt&#10;</xsl:text>
		<xsl:text>} %done&#10;</xsl:text>
		<xsl:text>{ % this code corresponds to the \end{admonition} command&#10;</xsl:text>
		<xsl:text> \vspace{5mm} &#10;</xsl:text>
		<xsl:text> \end{minipage}&#10;</xsl:text>
		<xsl:text> \endadmminipage&#10;</xsl:text>
		<xsl:text> \vspace{.5em}&#10;</xsl:text>
		<xsl:text> \par&#10;</xsl:text>
		<xsl:text>}&#10;</xsl:text>
	</xsl:param>

	<doc:template xmlns="">
		<refpurpose> Choose an admonition graphic </refpurpose>
		<doc:description>
			<para>

			For each admonition element
			(<doc:db>warning</doc:db>, <doc:db>tip</doc:db>, <doc:db>important</doc:db>, <doc:db>caution</doc:db>, <doc:db>note</doc:db>),
			this template chooses the graphics filename. If the admonition element is
			not known, the <doc:db>note</doc:db> graphic is used.

			</para>
		</doc:description>
		<doc:params>
			<variablelist>
				<varlistentry>
					<term>name</term>
					<listitem>
						<para>

							The name of the adminition. &DB2LaTeX; includes
							different graphics for different adminitions. By
							default, <literal>name</literal> defaults to the
							XSLT <function
							condition="xslt">local-name</function> of the
							current node.

						</para>
					</listitem>
				</varlistentry>
			</variablelist>
		</doc:params>
	</doc:template>

	<xsl:template name="admon.graphic">
		<xsl:param name="name" select="local-name(.)"/>
		<xsl:choose>
			<xsl:when test="$name='note'">note</xsl:when>
			<xsl:when test="$name='warning'">warning</xsl:when>
			<xsl:when test="$name='caution'">caution</xsl:when>
			<xsl:when test="$name='tip'">tip</xsl:when>
			<xsl:when test="$name='important'">important</xsl:when>
			<xsl:otherwise>note</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose> Process admonitions </refpurpose>
		<doc:description>
			<para>

			Invokes the environment provided by <xref
			linkend="param.latex.admonition.environment"/>
			and applies templates.

			</para>
			<note><para>An admonition will look something like this <doc:db>note</doc:db>.</para></note>
		</doc:description>
		<doc:notes>
			<para>

			There can be <quote>excessive</quote> whitespace between
			the bottom of the admonition area and a subsequent paragraph.

			</para>
		</doc:notes>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.admon.graphics.path"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.apply.title.templates.admonitions"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:samples>
			<simplelist type='inline'>
				&test_adm;
				&test_bind;
				&test_book;
			</simplelist>
		</doc:samples>
		<doc:seealso>
			<itemizedlist>
				<listitem><para>&mapping;</para></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>

	<xsl:template match="note|important|warning|caution|tip">
		<xsl:call-template name="map.begin">
			<xsl:with-param name="keyword">admonition</xsl:with-param>
			<xsl:with-param name="string">
				<xsl:text>{</xsl:text>
				<xsl:value-of select="$admon.graphics.path"/><xsl:text>/</xsl:text>
				<xsl:call-template name="admon.graphic"/>
				<xsl:text>}{</xsl:text>
				<xsl:choose>
					<xsl:when test="title and $latex.apply.title.templates.admonitions='1'">
						<xsl:call-template name="extract.object.title">
							<xsl:with-param name="object" select="."/>
						</xsl:call-template>
					</xsl:when>
					<xsl:otherwise>
						<xsl:call-template name="gentext.element.name"/>
					</xsl:otherwise>
				</xsl:choose>
				<xsl:text>}</xsl:text>
			</xsl:with-param>
		</xsl:call-template>
		<xsl:call-template name="content-templates"/>
		<xsl:call-template name="map.end">
			<xsl:with-param name="keyword">admonition</xsl:with-param>
		</xsl:call-template>
	</xsl:template>

</xsl:stylesheet>
