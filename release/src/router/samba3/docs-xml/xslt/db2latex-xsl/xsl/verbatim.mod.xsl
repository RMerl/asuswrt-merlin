<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: verbatim.mod.xsl,v 1.16 2004/01/31 11:53:14 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
+ ############################################################################## -->

<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="verbatim" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: verbatim.mod.xsl,v 1.16 2004/01/31 11:53:14 j-devenish Exp $
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
				<doc:revision rcasver="1.12">&rev_2003_05;</doc:revision>
			</revhistory>
		</referenceinfo>
		<title>Verbatim <filename>verbatim.mod.xsl</filename></title>
		<partintro>
			<para>
			
			
			
			</para>
		</partintro>
	</doc:reference>

	<doc:template xmlns="">
		<refpurpose> Auxiliary template to output verbatim &LaTeX; code in verbatim mode </refpurpose>
		<doc:description>
			<para>Typeset verbatim, monospace text in which whitespace is significant.</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.use.fancyvrb"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:notes>
			<para>This template is called by a number of element-related templates.</para>
			<para> Takes into account whether the user is using fancyvrb or not. It allows
			veratim line numbering and other fancy stuff. </para>
			<para> In order to use a small or large font, you may also want to use 
			implement <xref linkend="template.latex.fancyvrb.options"/>.
			</para>
			<!--
			the <literal>role</literal> attribute: </para>
	<screen><![CDATA[
<programlisting role="small">
</programlisting>
<programlisting role="large">
</programlisting>
]]></screen>
-->
				<para>
					This template will apply further templates using the
					<literal>latex.verbatim</literal> mode.
				</para>
			<warning>
				<para>
					This doesn't work inside <doc:db basename="table">tables</doc:db>.
					Also, if the element appears within a <doc:db>varlistentry</doc:db>,
					some &LaTeX; code will be emitted so that the verbatim environment
					starts on a new line.
				</para>
			</warning>
		</doc:notes>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="template.latex.fancyvrb.options"/></simpara></listitem>
				<listitem><simpara><xref linkend="template.text()-latex.verbatim"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template name="verbatim.apply.templates">
		<xsl:if test="ancestor::varlistentry">
			<!-- start the environment on a new line -->
			<xsl:text>\null{}</xsl:text>
		</xsl:if>
		<xsl:choose>
			<xsl:when test="ancestor::entry">
				<xsl:message>Problem with <xsl:value-of select="local-name(.)"/> inside table entries.</xsl:message>
				<xsl:text>\texttt{</xsl:text>
				<xsl:apply-templates mode="latex.verbatim"/>
				<xsl:text>}</xsl:text>
			</xsl:when>
			<xsl:when test="$latex.use.fancyvrb='1'">
				<xsl:variable name="not_monospaced" select="local-name(.)='literallayout' and @format!='monospaced'"/>
				<xsl:text>&#10;\begin{Verbatim}[</xsl:text>
				<xsl:if test="@linenumbering='numbered'">
					<xsl:text>,numbers=left</xsl:text>
				</xsl:if>
				<xsl:if test="$not_monospaced">
					<xsl:text>,fontfamily=default</xsl:text>
				</xsl:if>
				<xsl:call-template name="latex.fancyvrb.options"/>
				<xsl:text>]&#10;</xsl:text>
				<xsl:choose>
					<xsl:when test="$not_monospaced">
						<!-- Needs to be changed to cope with regular characterset! -->
						<xsl:apply-templates mode="latex.verbatim"/>
					</xsl:when>
					<xsl:otherwise>
						<xsl:apply-templates mode="latex.verbatim"/>
					</xsl:otherwise>
				</xsl:choose>
				<xsl:text>&#10;\end{Verbatim}&#10;</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text>&#10;\begin{verbatim}&#10;</xsl:text>
				<xsl:apply-templates mode="latex.verbatim"/>
				<xsl:text>&#10;\end{verbatim}&#10;</xsl:text>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose> Process <quote>verbatim</quote> environment where whitespace is significant </refpurpose>
		<doc:description>
			<para>

			Calls <xref linkend="template.verbatim.apply.templates"/>.

			</para>
		</doc:description>
		<doc:variables>
			<variablelist>
				<varlistentry>
					<term><xref linkend="param.latex.trim.verbatim"/></term>
					<listitem><simpara>
						See <xref linkend="text()-latex.verbatim"/>.
						<!-- note: that is not a valid ID! -->
					</simpara></listitem>
				</varlistentry>
			</variablelist>
		</doc:variables>
		<doc:samples>
			<simplelist type='inline'>
				&test_article;
				&test_bind;
				&test_book;
				&test_ddh;
				&test_program;
			</simplelist>
		</doc:samples>
	</doc:template>
	<xsl:template match="address|screen|programlisting|literallayout">
		<xsl:call-template name="verbatim.apply.templates"/>
	</xsl:template>

	<xsl:template name="next.is.verbatim">
		<xsl:param name="object" select="following-sibling::*[1]"/>
		<xsl:value-of select="count($object[self::address or self::screen or self::programlisting or self::literallayout])"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>literal</doc:db> elements</refpurpose>
		<doc:description>
			<para>
			
			Applies templates in the <quote>template.latex.verbatim</quote>
			mode within a &LaTeX; <function condition="latex">verb</function>
			command.
			
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="literal" mode="latex.verbatim">
		<xsl:text>{\verb </xsl:text>
		<xsl:apply-templates mode="latex.verbatim"/>
		<xsl:text>}</xsl:text>
	</xsl:template>

</xsl:stylesheet>
