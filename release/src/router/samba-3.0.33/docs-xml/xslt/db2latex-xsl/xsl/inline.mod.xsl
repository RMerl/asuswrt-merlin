<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: inline.mod.xsl,v 1.21 2004/01/05 09:58:47 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
+ ############################################################################## -->
<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="inline" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: inline.mod.xsl,v 1.21 2004/01/05 09:58:47 j-devenish Exp $
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
				<doc:revision rcasver="1.16">&rev_2003_05;</doc:revision>
			</revhistory>
		</referenceinfo>
		<title>Inline Elements <filename>inline.mod.xsl</filename></title>
		<partintro>
			<para>
			
			
			
			</para>
		</partintro>
	</doc:reference>

	<doc:template xmlns="">
		<refpurpose>Process regular text</refpurpose>
		<doc:description>
			<para>
				Applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="normalize-scape"/></simpara></listitem>
				<listitem><simpara><xref linkend="scape"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template name="inline.charseq">
		<xsl:param name="content">
			<xsl:apply-templates/>
		</xsl:param>
		<xsl:copy-of select="$content"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process monospace text</refpurpose>
		<doc:description>
			<para>
				Applies templates within a &LaTeX;
				<function condition="latex">texttt</function> command.
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.hyphenation.tttricks"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="normalize-scape"/></simpara></listitem>
				<listitem><simpara><xref linkend="scape"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template name="inline.monoseq">
		<xsl:param name="hyphenation">\docbookhyphenatedot</xsl:param>
		<xsl:param name="content">
			<xsl:apply-templates/>
		</xsl:param>
		<xsl:text>{\texttt{</xsl:text>
		<xsl:if test="$latex.hyphenation.tttricks='1'"><xsl:value-of select="$hyphenation" /></xsl:if>
		<xsl:text>{</xsl:text>
		<xsl:copy-of select="$content"/>
		<xsl:text>}}}</xsl:text>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process bold text</refpurpose>
		<doc:description>
			<para>
				Applies templates within a &LaTeX;
				<function condition="latex">bfseries</function> command.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="normalize-scape"/></simpara></listitem>
				<listitem><simpara><xref linkend="scape"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template name="inline.boldseq">
		<xsl:param name="content">
			<xsl:apply-templates/>
		</xsl:param>
		<xsl:text>{\bfseries{</xsl:text>
		<xsl:copy-of select="$content"/>
		<xsl:text>}}</xsl:text>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process italic text</refpurpose>
		<doc:description>
			<para>
				Applies templates within a &LaTeX;
				<function condition="latex">em</function> command.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="normalize-scape"/></simpara></listitem>
				<listitem><simpara><xref linkend="scape"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template name="inline.italicseq">
		<xsl:param name="content">
			<xsl:apply-templates/>
		</xsl:param>
		<xsl:text>{\em{</xsl:text>
		<xsl:copy-of select="$content"/>
		<xsl:text>}}</xsl:text>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process bold monospace text</refpurpose>
		<doc:description>
			<para>
				Applies templates within &LaTeX;
				<function condition="latex">ttfamily</function>
				and
				<function condition="latex">bfseries</function>
				commands.
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.hyphenation.tttricks"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="normalize-scape"/></simpara></listitem>
				<listitem><simpara><xref linkend="scape"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template name="inline.boldmonoseq">
		<xsl:param name="hyphenation">\docbookhyphenatedot</xsl:param>
		<xsl:param name="content">
			<xsl:apply-templates/>
		</xsl:param>
		<xsl:text>{\ttfamily\bfseries{</xsl:text>
		<xsl:if test="$latex.hyphenation.tttricks='1'"><xsl:value-of select="$hyphenation" /></xsl:if>
		<xsl:text>{</xsl:text>
		<xsl:copy-of select="$content"/>
		<xsl:text>}}}</xsl:text>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process italic monospace text</refpurpose>
		<doc:description>
			<para>
				Applies templates within &LaTeX;
				<function condition="latex">ttfamily</function>
				and
				<function condition="latex">itshape</function>
				commands.
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.hyphenation.tttricks"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="normalize-scape"/></simpara></listitem>
				<listitem><simpara><xref linkend="scape"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template name="inline.italicmonoseq">
		<xsl:param name="hyphenation">\docbookhyphenatedot</xsl:param>
		<xsl:param name="content">
			<xsl:apply-templates/>
		</xsl:param>
		<xsl:text>{\ttfamily\itshape{</xsl:text>
		<xsl:if test="$latex.hyphenation.tttricks='1'"><xsl:value-of select="$hyphenation" /></xsl:if>
		<xsl:text>{</xsl:text>
		<xsl:copy-of select="$content"/>
		<xsl:text>}}}</xsl:text>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process superscript text</refpurpose>
		<doc:description>
			<para>
				Applies templates within a &LaTeX;
				<function condition="latex">text</function>
				command within an inline mathematics environment.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="normalize-scape"/></simpara></listitem>
				<listitem><simpara><xref linkend="scape"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template name="inline.superscriptseq">
		<xsl:param name="content">
			<xsl:apply-templates/>
		</xsl:param>
		<xsl:text>$^\text{</xsl:text>
		<xsl:copy-of select="$content"/>
		<xsl:text>}$</xsl:text>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process subscript text</refpurpose>
		<doc:description>
			<para>
				Applies templates within a &LaTeX;
				<function condition="latex">text</function>
				command within an inline mathematics environment.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="normalize-scape"/></simpara></listitem>
				<listitem><simpara><xref linkend="scape"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template name="inline.subscriptseq">
		<xsl:param name="content">
			<xsl:apply-templates/>
		</xsl:param>
		<xsl:text>$_\text{</xsl:text>
		<xsl:copy-of select="$content"/>
		<xsl:text>}$</xsl:text>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>accel</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.charseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="accel">
		<xsl:call-template name="inline.charseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>action</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.charseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="action">
		<xsl:call-template name="inline.charseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process name-type elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.charseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="honorific|firstname|surname|lineage|othername">
		<xsl:call-template name="inline.charseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>application</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.charseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="application">
		<xsl:call-template name="inline.charseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>classname</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.monoseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="classname">
		<xsl:call-template name="inline.monoseq"/>
	</xsl:template>

	<doc:template basename="copyright" xmlns="">
		<refpurpose>Process <doc:db>copyright</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates with a copyright dingbat.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="copyright" name="copyright">
		<xsl:call-template name="gentext.element.name"/>
		<xsl:call-template name='gentext.space'/>
		<xsl:call-template name="dingbat">
			<xsl:with-param name="dingbat">copyright</xsl:with-param>
		</xsl:call-template>
		<xsl:call-template name='gentext.space'/>
		<xsl:apply-templates select="year"/>
		<xsl:call-template name='gentext.space'/>
		<xsl:apply-templates select="holder"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process a <doc:db>copyright</doc:db>'s <doc:db>holder</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="copyright/holder">
		<xsl:apply-templates />
	</xsl:template>

	<xsl:template match="copyright/year[position()&lt;last()-1]">
		<xsl:apply-templates />
		<xsl:text>, </xsl:text>
	</xsl:template>

	<!-- RCAS 2003/03/11 FIXME : "and" -->
	<xsl:template match="copyright/year[position()=last()-1]">
		<xsl:apply-templates />
		<xsl:text>, </xsl:text>
	</xsl:template>

	<xsl:template match="copyright/year[position()=last()]">
		<xsl:apply-templates />
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>exceptionname</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.monoseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="exceptionname">
		<xsl:call-template name="inline.monoseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>interfacename</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.monoseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="interfacename">
		<xsl:call-template name="inline.monoseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>methodname</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.monoseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="methodname">
		<xsl:call-template name="inline.monoseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>command</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.boldseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="command">
		<xsl:call-template name="inline.boldseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>computeroutput</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.monoseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="computeroutput">
		<xsl:call-template name="inline.monoseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>constant</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.monoseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="constant">
		<xsl:call-template name="inline.monoseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>database</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.charseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="database">
		<xsl:call-template name="inline.charseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>errorcode</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.charseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="errorcode">
		<xsl:call-template name="inline.charseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>errorname</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.charseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="errorname">
		<xsl:call-template name="inline.charseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>errortype</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.charseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="errortype">
		<xsl:call-template name="inline.charseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>envar</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.monoseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="envar">
		<xsl:call-template name="inline.monoseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>filename</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.monoseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="filename">
		<xsl:call-template name="inline.monoseq">
			<xsl:with-param name="hyphenation">\docbookhyphenatefilename</xsl:with-param>
		</xsl:call-template>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>function</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.monoseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				<doc:todo>Insert documentation here.</doc:todo>
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="function">
		<xsl:choose>
			<xsl:when test="$function.parens = 1 or parameter or function or replaceable">
				<xsl:variable name="nodes" select="text()|*"/>
				<xsl:call-template name="inline.monoseq">
					<xsl:with-param name="content">
					<xsl:apply-templates select="$nodes[1]"/>
					</xsl:with-param>
				</xsl:call-template>
				<xsl:text>(</xsl:text>
				<xsl:apply-templates select="$nodes[position()>1]"/>
				<xsl:text>)</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<xsl:call-template name="inline.monoseq"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process a <doc:db>function</doc:db>'s <doc:db>parameter</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.italicmonoseq"/> and
				separates subsequent <doc:db
				basename="replaceable">replaceables</doc:db> with commas.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="function/parameter" priority="2">
		<xsl:call-template name="inline.italicmonoseq"/>
		<xsl:if test="following-sibling::*">
			<xsl:text>, </xsl:text>
		</xsl:if>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process a <doc:db>function</doc:db>'s <doc:db>replaceable</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.italicmonoseq"/> and
				separates subsequent <doc:db
				basename="replaceable">replaceables</doc:db> with commas.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="function/replaceable" priority="2">
		<xsl:call-template name="inline.italicmonoseq"/>
		<xsl:if test="following-sibling::*">
			<xsl:text>, </xsl:text>
		</xsl:if>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process GUI-type elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.charseq"/>
				within a &LaTeX; <function condition="latex">sffamily</function>
				and <function condition="latex">bfseries</function> commands.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="guibutton|guiicon|guilabel|guimenu|guimenuitem|guisubmenu|interface">
		<xsl:text>{\sffamily \bfseries </xsl:text>
		<xsl:call-template name="inline.charseq" />
		<xsl:text>}</xsl:text>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>hardware</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.charseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="hardware">
		<xsl:call-template name="inline.charseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>interfacedefinition</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.charseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="interfacedefinition">
		<xsl:call-template name="inline.charseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>keycap</doc:db> and <doc:db>keysym</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.boldseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="keycap|keysym">
		<xsl:call-template name="inline.boldseq" />
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>keycode</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.charseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="keycode">
		<xsl:call-template name="inline.charseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>literal</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.monoseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="literal">
		<xsl:call-template name="inline.monoseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>medialabel</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.italicseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="medialabel">
		<xsl:call-template name="inline.italicseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>shortcut</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="shortcut">
		<xsl:apply-templates/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>mousebutton</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.charseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="mousebutton">
		<xsl:call-template name="inline.charseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>option</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.monoseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="option">
		<xsl:call-template name="inline.monoseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>parameter</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.italicmonoseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="parameter" priority="1">
		<xsl:call-template name="inline.italicmonoseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>property</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.charseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="property">
		<xsl:call-template name="inline.charseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>prompt</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.monoseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="prompt">
		<xsl:call-template name="inline.monoseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>replaceable</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.italicmonoseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="replaceable" priority="1">
		<xsl:call-template name="inline.italicmonoseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>returnvalue</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.charseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="returnvalue">
		<xsl:call-template name="inline.charseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>structfield</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.italicmonoseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="structfield">
		<xsl:call-template name="inline.italicmonoseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>structname</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.charseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="structname">
		<xsl:call-template name="inline.charseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>symbol</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.charseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="symbol">
		<xsl:call-template name="inline.charseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>systemitem</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.monoseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="systemitem">
		<xsl:call-template name="inline.monoseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>token</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.charseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="token">
		<xsl:call-template name="inline.charseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>type</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.charseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="type">
		<xsl:call-template name="inline.charseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>userinput</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.boldmonoseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="userinput">
		<xsl:call-template name="inline.boldmonoseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>abbrev</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.charseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				<doc:todo>It would be useful if a terminating full stop
				were not to induce sentence-end whitespace spacing.</doc:todo>
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="abbrev">
		<xsl:call-template name="inline.charseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>acronym</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.charseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="acronym">
		<xsl:call-template name="inline.charseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>citerefentry</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.charseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="citerefentry">
		<xsl:call-template name="inline.charseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>citetitle</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.italicseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="citetitle">
		<xsl:call-template name="inline.italicseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>corpauthor</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="corpauthor">
		<xsl:apply-templates/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>emphasis</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.italicseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="emphasis">
		<xsl:call-template name="inline.italicseq"/>
	</xsl:template>

	<doc:template xmlns="" basename="emphasis">
		<refpurpose>Process <doc:db>emphasis</doc:db> elements with <quote>bold</quote> role</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.boldseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="emphasis[@role='bold']">
		<xsl:call-template name="inline.boldseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>foreignphrase</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.italicseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="foreignphrase">
		<xsl:call-template name="inline.italicseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>jobtitle</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="jobtitle">
		<xsl:apply-templates/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>markup</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies template using the <literal>latex.verbatim</literal>
				mode.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="markup">
		<xsl:apply-templates mode="latex.verbatim"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>orgdiv</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="orgdiv">
		<xsl:apply-templates/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>orgname</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="orgname">
		<xsl:apply-templates/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>phrase</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.charseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				Normally, <xref linkend="template.inline.charseq"/> is used.
				However, the use of <quote>latex</quote> or <quote>tex</quote>
				for the <sgmltag class="attribute">role</sgmltag> attribute
				will convert the contents to plain text without &LaTeX;
				active-character escaping.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="phrase">
		<xsl:choose>
			<xsl:when test="@role='tex' or @role='latex'">
				<xsl:value-of select="."/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:call-template name="inline.charseq"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>quote</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.charseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				The contents will be enclosed in quotation marks,
				using <literal>gentext.startquote</literal>
				and <literal>gentext.nestedstartquote</literal>
				alternating according to the <doc:db>quote</doc:db>
				nesting level.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="quote">
		<xsl:choose>
			<xsl:when test="count(ancestor::quote) mod 2=0">
				<xsl:call-template name="gentext.startquote"/>
				<xsl:call-template name="inline.charseq"/>
				<xsl:call-template name="gentext.endquote"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:call-template name="gentext.nestedstartquote"/>
				<xsl:call-template name="inline.charseq"/>
				<xsl:call-template name="gentext.nestedendquote"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>varname</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.monoseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="varname">
		<xsl:call-template name="inline.monoseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>wordasword</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.italicseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="wordasword">
		<xsl:call-template name="inline.italicseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>lineannotation</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.charseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="lineannotation">
		<xsl:call-template name="inline.charseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>superscript</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.superscriptseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="superscript">
		<xsl:call-template name="inline.superscriptseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>subscript</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.subscriptseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="subscript">
		<xsl:call-template name="inline.subscriptseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>trademark</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.charseq"/>,
				then appends a <quote>trademark</quote> dingbat.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="template.dingbat"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="trademark">
		<xsl:call-template name="inline.charseq"/>
		<xsl:call-template name="dingbat">
			<xsl:with-param name="dingbat">trademark</xsl:with-param>
		</xsl:call-template>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>firstterm</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.italicseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
    <xsl:template match="firstterm">
	<xsl:call-template name="inline.italicseq"/>
    </xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>glossterm</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.charseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="glossterm">
		<xsl:call-template name="inline.charseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>keycombo</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Formats a key combination using conjugation characters.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				Templates are applied for all children, with comjugation
				characters based upon the <sgmltag
				class="attribute">action</sgmltag> attribute.
			</para>
			<para>
				For <quote>seq</quote> actions, a space character is used.
				For <quote>simul</quote> actions, a plus sign (+) is used.
				For <quote>other</quote> actions, no conjugation character is
				used (children will be abutting).
				For all other actions, an en-dash is used.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="keycombo">
		<xsl:variable name="action" select="@action"/>
		<xsl:variable name="joinchar">
			<xsl:choose>
			<xsl:when test="$action='seq'"><xsl:text> </xsl:text></xsl:when>
			<xsl:when test="$action='simul'">+</xsl:when>
			<xsl:when test="$action='press'">--</xsl:when>
			<xsl:when test="$action='click'">--</xsl:when>
			<xsl:when test="$action='double-click'">--</xsl:when>
			<xsl:when test="$action='other'"></xsl:when>
			<xsl:otherwise>--</xsl:otherwise>
			</xsl:choose>
		</xsl:variable>
		<xsl:for-each select="./*">
			<xsl:if test="position()>1"><xsl:value-of select="$joinchar"/></xsl:if>
			<xsl:apply-templates select="."/>
		</xsl:for-each>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>menuchoice</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes templates for non-shortcut children, then formats any
				<doc:db basename="shortcut">shortcuts</doc:db> in parentheses.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="template.process.menuchoice"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="menuchoice">
		<xsl:variable name="shortcut" select="./shortcut"/>
		<xsl:call-template name="process.menuchoice"/>
		<xsl:if test="$shortcut">
			<xsl:text> (</xsl:text>
			<xsl:apply-templates select="$shortcut"/>
			<xsl:text>)</xsl:text>
		</xsl:if>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>menuchoice</doc:db> children (not shortcut) </refpurpose>
		<doc:description>
			<para>
				Applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				Selects children of the current node (does not select <doc:db>shortcut</doc:db> elements).
				Subsequent children are delimited by a plug sign, in general, or an arrow,
				for <doc:db>guimenuitem</doc:db> and <doc:db>guisubmenu</doc:db>.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template name="process.menuchoice">
		<xsl:param name="nodelist" select="guibutton|guiicon|guilabel|guimenu|guimenuitem|guisubmenu|interface"/><!-- not(shortcut) -->
		<xsl:param name="count" select="1"/>
		<xsl:choose>
			<xsl:when test="$count>count($nodelist)"/>
			<xsl:when test="$count=1">
				<xsl:apply-templates select="$nodelist[$count=position()]"/>
				<xsl:call-template name="process.menuchoice">
					<xsl:with-param name="nodelist" select="$nodelist"/>
					<xsl:with-param name="count" select="$count+1"/>
				</xsl:call-template>
			</xsl:when>
			<xsl:otherwise>
				<xsl:variable name="node" select="$nodelist[$count=position()]"/>
				<xsl:choose>
					<xsl:when test="name($node)='guimenuitem' or name($node)='guisubmenu'">
					<xsl:text> $\to$ </xsl:text>
					</xsl:when>
					<xsl:otherwise>+</xsl:otherwise>
				</xsl:choose>
				<xsl:apply-templates select="$node"/>
				<xsl:call-template name="process.menuchoice">
					<xsl:with-param name="nodelist" select="$nodelist"/>
					<xsl:with-param name="count" select="$count+1"/>
				</xsl:call-template>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>optional</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.charseq"/>,
				surrounded by $arg.choice.opt.open.str and
				$arg.choice.opt.close.str.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="optional">
		<xsl:value-of select="$arg.choice.opt.open.str"/>
		<xsl:call-template name="inline.charseq"/>
		<xsl:value-of select="$arg.choice.opt.close.str"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>remark</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates as a margin note.
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.show.comments"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:notes>
			<para>When <xref linkend="param.show.comments"/> is set,
			templates will be applied within a &LaTeX;
			<function condition="latex">marginpar</function> command,
			using <function condition="latex">footnotesize</function>.</para>
			<para>If <xref linkend="param.show.comments"/> is not set,
			then content is suppressed.</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="comment|remark">
		<xsl:if test="$show.comments=1">
			<xsl:text>\marginpar{\footnotesize{</xsl:text>
			<xsl:apply-templates/>
			<xsl:text>}}</xsl:text>
		</xsl:if>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>productname</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.charseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="productname">
		<xsl:call-template name="inline.charseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>productnumber</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.charseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="productnumber">
		<xsl:call-template name="inline.charseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process address-like elements</refpurpose>
		<doc:description>
			<para>
				Invokes <xref linkend="template.inline.charseq"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="pob|street|city|state|postcode|country|phone|fax|otheraddr">
		<xsl:call-template name="inline.charseq"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>beginpage</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Suppressed.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="beginpage"/>

</xsl:stylesheet>

