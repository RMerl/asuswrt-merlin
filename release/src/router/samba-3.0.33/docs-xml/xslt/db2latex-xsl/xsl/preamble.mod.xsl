<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: preamble.mod.xsl,v 1.82 2004/01/31 12:26:12 j-devenish Exp $		
|- #############################################################################
|	$Author: j-devenish $
|
|   PURPOSE: Variables and templates to manage LaTeX preamble.
+ ############################################################################## -->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="preamble" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: preamble.mod.xsl,v 1.82 2004/01/31 12:26:12 j-devenish Exp $
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
				<doc:revision rcasver="1.77">&rev_2003_05;</doc:revision>
			</revhistory>
		</referenceinfo>
		<title>Variables and Templates used in &LaTeX; Preamble Generation</title>
		<partintro>
			<para>
			
			This section described the variables and templates that are used in
			the generation of the output &LaTeX; preamble. Basically, the
			&LaTeX; preamble depends on the <acronym>XML</acronym> document, that is, on whether
			it is an <doc:db>article</doc:db> or a <doc:db>book</doc:db>.
			
			</para>
		</partintro>
	</doc:reference>

	<doc:template name="generate.latex.article.preamble" xmlns="">
		<refpurpose>

			Top level template, called by article template, responsible of
			generating the &LaTeX; preamble according to user
			<acronym>XSL</acronym> variables and templates.

		</refpurpose>
		<doc:description>
			<para>

			If <xref linkend="param.latex.override"/> is empty, the template
			outputs <xref linkend="param.latex.article.preamblestart"/> and
			<xref linkend="param.latex.article.preamble.pre"/>, then calls
			<xref linkend="template.generate.latex.common.preamble"/> and <xref
			linkend="template.generate.latex.essential.preamble"/> followed by
			the value of <xref linkend="param.latex.article.preamble.post"/>.
			Otherwise, it outputs the value of <xref linkend="param.latex.override"/>
			followed by <xref
			linkend="template.generate.latex.essential.preamble"/>.

			</para>
		</doc:description>
	</doc:template>
	<xsl:template name="generate.latex.article.preamble">
		<xsl:choose>
			<xsl:when test="$latex.override = ''">
				<xsl:value-of select="$latex.article.preamblestart"/>
				<xsl:value-of select="$latex.article.preamble.pre"/>
				<xsl:call-template name="label.id"/>
				<xsl:call-template name="generate.latex.common.preamble"/>
				<xsl:call-template name="generate.latex.essential.preamble"/>
				<xsl:value-of select="$latex.article.preamble.post"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="$latex.override"/>
				<xsl:call-template name="generate.latex.essential.preamble"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

	<doc:template name="generate.latex.book.preamble" xmlns="">
		<refpurpose>

			Top level template, called by book template, responsible of
			generating the &LaTeX; preamble according to user
			<acronym>XSL</acronym> variables and templates.

		</refpurpose>
		<doc:description>
			<para>

			If <xref linkend="param.latex.override"/> is empty, the template
			outputs <xref linkend="param.latex.book.preamblestart"/> and
			<xref linkend="param.latex.book.preamble.pre"/>, then calls
			<xref linkend="template.generate.latex.common.preamble"/> and <xref
			linkend="template.generate.latex.essential.preamble"/> followed by
			the value of <xref linkend="param.latex.book.preamble.post"/>.
			Otherwise, it outputs the value of <xref linkend="param.latex.override"/>
			followed by <xref
			linkend="template.generate.latex.essential.preamble"/>.

			</para>
		</doc:description>
	</doc:template>
	<xsl:template name="generate.latex.book.preamble">
		<xsl:choose>
			<xsl:when test="$latex.override = ''">
				<xsl:value-of select="$latex.book.preamblestart"/>
				<xsl:value-of select="$latex.book.preamble.pre"/>
				<xsl:call-template name="label.id"/>
				<xsl:call-template name="generate.latex.common.preamble"/>
				<xsl:call-template name="generate.latex.essential.preamble"/>
				<xsl:value-of select="$latex.book.preamble.post"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="$latex.override"/>
				<xsl:call-template name="generate.latex.essential.preamble"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

	<doc:param name="latex.article.preamblestart" xmlns="">
		<refpurpose> Generate <function condition="latex">documentclass</function> for <doc:db basename="article">articles</doc:db>  </refpurpose>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.documentclass.common"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.documentclass.article"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.documentclass.pdftex"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.documentclass.dvips"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.documentclass"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:description>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.article.preamblestart">
		<xsl:text>% --------------------------------------------&#10;</xsl:text>
		<xsl:text>% Autogenerated LaTeX file for articles&#10;</xsl:text>
		<xsl:text>% --------------------------------------------&#10;</xsl:text>
		<xsl:text>\ifx\pdfoutput\undefined&#10;</xsl:text>
		<xsl:text>\documentclass[</xsl:text>
		<xsl:value-of select='$latex.documentclass.common' />
		<xsl:text>,</xsl:text>
		<xsl:value-of select='$latex.documentclass.article' />
		<xsl:text>,</xsl:text>
		<xsl:value-of select='$latex.documentclass.pdftex' />
		<xsl:text>]{</xsl:text>
		<xsl:choose>
			<xsl:when test="$latex.documentclass!=''"><xsl:value-of select="$latex.documentclass" /></xsl:when>
			<xsl:otherwise><xsl:text>article</xsl:text></xsl:otherwise>
		</xsl:choose>
		<xsl:text>}&#10;</xsl:text>
		<xsl:text>\else&#10;</xsl:text>
		<xsl:text>\documentclass[pdftex,</xsl:text>
		<xsl:value-of select='$latex.documentclass.common' />
		<xsl:text>,</xsl:text>
		<xsl:value-of select='$latex.documentclass.article' />
		<xsl:text>,</xsl:text>
		<xsl:value-of select='$latex.documentclass.dvips' />
		<xsl:text>]{</xsl:text>
		<xsl:choose>
			<xsl:when test="$latex.documentclass!=''"><xsl:value-of select="$latex.documentclass" /></xsl:when>
			<xsl:otherwise><xsl:text>article</xsl:text></xsl:otherwise>
		</xsl:choose>
		<xsl:text>}&#10;</xsl:text>
		<xsl:text>\fi&#10;</xsl:text>
	</xsl:param>

	<doc:param name="latex.book.preamblestart" xmlns="">
		<refpurpose> Generate <function condition="latex">documentclass</function> for <doc:db basename="book">books</doc:db>  </refpurpose>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.documentclass.common"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.documentclass.book"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.documentclass.pdftex"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.documentclass.dvips"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.documentclass"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:description>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.book.preamblestart">
		<xsl:text>% ------------------------------------------------------------	&#10;</xsl:text>
		<xsl:text>% Autogenerated LaTeX file for books	&#10;</xsl:text>
		<xsl:text>% ------------------------------------------------------------	&#10;</xsl:text>
		<xsl:text>\ifx\pdfoutput\undefined&#10;</xsl:text>
		<xsl:text>\documentclass[</xsl:text>
		<xsl:value-of select='$latex.documentclass.common' />
		<xsl:text>,</xsl:text>
		<xsl:value-of select='$latex.documentclass.book' />
		<xsl:text>,</xsl:text>
		<xsl:value-of select='$latex.documentclass.pdftex' />
		<xsl:text>]{</xsl:text>
		<xsl:choose>
			<xsl:when test="$latex.documentclass!=''"><xsl:value-of select="$latex.documentclass" /></xsl:when>
			<xsl:otherwise><xsl:text>report</xsl:text></xsl:otherwise>
		</xsl:choose>
		<xsl:text>}&#10;</xsl:text>
		<xsl:text>\else&#10;</xsl:text>
		<xsl:text>\documentclass[pdftex,</xsl:text>
		<xsl:value-of select='$latex.documentclass.common' />
		<xsl:text>,</xsl:text>
		<xsl:value-of select='$latex.documentclass.book' />
		<xsl:text>,</xsl:text>
		<xsl:value-of select='$latex.documentclass.dvips' />
		<xsl:text>]{</xsl:text>
		<xsl:choose>
			<xsl:when test="$latex.documentclass!=''"><xsl:value-of select="$latex.documentclass" /></xsl:when>
			<xsl:otherwise><xsl:text>report</xsl:text></xsl:otherwise>
		</xsl:choose>
		<xsl:text>}&#10;</xsl:text>
		<xsl:text>\fi&#10;</xsl:text>
	</xsl:param>

	<doc:param xmlns="">
		<refpurpose> Select localisation language  </refpurpose>
		<doc:description>
			<para>
				Chooses the <quote>gentext</quote> language for the document.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>

				Selects the root <doc:db>set</doc:db>, <doc:db>book</doc:db> or
				<doc:db>article</doc:db> element and reads its <sgmltag
				class="attribute">lang</sgmltag> or <sgmltag
				class="attribute">xml:lang</sgmltag> attribute.

			</para>
		</doc:notes>
	</doc:param>
	<xsl:param name="document.xml.language">
		<xsl:call-template name="l10n.language">
			<xsl:with-param name="target" select="(/set|/book|/article)[1]"/>
			<!-- now, induce the use of $target rather than the current node: -->
			<xsl:with-param name="xref-context" select="true()"/>
		</xsl:call-template>
	</xsl:param>

	<doc:param xmlns="">
		<refpurpose> Select <productname>babel</productname> option </refpurpose>
		<doc:description>
			<para>
				If <xref linkend="param.latex.babel.language"/> is not set, this
				template will select a <productname>babel</productname> option
				based on the <xref linkend="param.document.xml.language"/>.
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.babel.language"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.document.xml.language"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:notes>
			<para>A special value of <quote><literal>none</literal></quote>
			can be used to disable <productname>babel</productname>.</para>
		</doc:notes>
	</doc:param>
	<xsl:param name="latex.language.option">
		<xsl:choose>
			<xsl:when test="$latex.babel.language!=''">
				<xsl:value-of select="$latex.babel.language"/>
			</xsl:when>
			<xsl:when test="starts-with($document.xml.language,'af')">afrikaans</xsl:when>
			<xsl:when test="starts-with($document.xml.language,'br')">breton</xsl:when>
			<xsl:when test="starts-with($document.xml.language,'ca')">catalan</xsl:when>
			<xsl:when test="starts-with($document.xml.language,'cs')">czech</xsl:when>
			<xsl:when test="starts-with($document.xml.language,'cy')">welsh</xsl:when>
			<xsl:when test="starts-with($document.xml.language,'da')">danish</xsl:when>
			<xsl:when test="starts-with($document.xml.language,'de')">ngerman</xsl:when><!-- ngerman, german or germanb? -->
			<xsl:when test="starts-with($document.xml.language,'el')">greek</xsl:when>
			<xsl:when test="starts-with($document.xml.language,'en')">
				<xsl:choose>
					<xsl:when test="starts-with($document.xml.language,'en-CA')">canadian</xsl:when>
					<xsl:when test="starts-with($document.xml.language,'en-GB')">british</xsl:when>
					<xsl:when test="starts-with($document.xml.language,'en-US')">USenglish</xsl:when>
					<xsl:otherwise>none</xsl:otherwise>
				</xsl:choose>
			</xsl:when>
			<xsl:when test="starts-with($document.xml.language,'eo')">esperanto</xsl:when>
			<xsl:when test="starts-with($document.xml.language,'es')">spanish</xsl:when>
			<xsl:when test="starts-with($document.xml.language,'et')">estonian</xsl:when>
			<xsl:when test="starts-with($document.xml.language,'fi')">finnish</xsl:when>
			<xsl:when test="starts-with($document.xml.language,'fr')">french</xsl:when><!-- francais, french, or frenchb? -->
			<xsl:when test="starts-with($document.xml.language,'ga')">irish</xsl:when>
			<xsl:when test="starts-with($document.xml.language,'gd')">scottish</xsl:when>
			<xsl:when test="starts-with($document.xml.language,'gl')">galician</xsl:when>
			<xsl:when test="starts-with($document.xml.language,'he')">hebrew</xsl:when>
			<xsl:when test="starts-with($document.xml.language,'hr')">croatian</xsl:when>
			<xsl:when test="starts-with($document.xml.language,'hu')">hungarian</xsl:when>
			<xsl:when test="starts-with($document.xml.language,'id')">bahasa</xsl:when>
			<xsl:when test="starts-with($document.xml.language,'it')">italian</xsl:when>
			<xsl:when test="starts-with($document.xml.language,'nl')">dutch</xsl:when>
			<xsl:when test="starts-with($document.xml.language,'nn')">norsk</xsl:when>
			<xsl:when test="starts-with($document.xml.language,'pl')">polish</xsl:when>
			<xsl:when test="starts-with($document.xml.language,'pt')">
				<xsl:choose>
					<xsl:when test="starts-with($document.xml.language,'pt-BR')">brazil</xsl:when>
					<xsl:otherwise>portugese</xsl:otherwise>
				</xsl:choose>
			</xsl:when>
			<xsl:when test="starts-with($document.xml.language,'ro')">romanian</xsl:when>
			<xsl:when test="starts-with($document.xml.language,'ru')">russian</xsl:when>
			<xsl:when test="starts-with($document.xml.language,'sk')">slovak</xsl:when>
			<xsl:when test="starts-with($document.xml.language,'sl')">slovene</xsl:when>
			<xsl:when test="starts-with($document.xml.language,'sv')">swedish</xsl:when>
			<xsl:when test="starts-with($document.xml.language,'tr')">turkish</xsl:when>
			<xsl:when test="starts-with($document.xml.language,'uk')">ukrainian</xsl:when>
		</xsl:choose>
	</xsl:param>

	<doc:template xmlns="">
		<refpurpose>

			Common &LaTeX; preamble shared by <doc:db>articles</doc:db> and
			<doc:db>books</doc:db>, and other document classes. Most of the
			packages and package options are managed here.

		</refpurpose>
		<doc:description>
			<para>

			The &LaTeX; preamble, after the <function
			condition="latex">documentclass</function> command but before the
			<function condition="env">document</function> environment.

			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.pdf.support"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.pdf.preamble"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.article.varsets"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.book.varsets"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.bridgehead.in.lot"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.use.fancyhdr"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.fancyhdr.style"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.fancyhdr.truncation.partition"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.fancyhdr.truncation.style"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.use.varioref"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.varioref.options"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.use.dcolumn"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.decimal.point"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.use.fancybox"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.use.fancyvrb"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.fancyvrb.tabsize"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.use.isolatin1"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.use.parskip"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.use.rotating"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.use.subfigure"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.use.tabularx"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.use.umoline"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.use.url"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.math.support"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.math.preamble"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.document.font"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.use.hyperref"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.admonition.environment"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.caption.swapskip"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.hyphenation.tttricks"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.language.option"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.is.draft"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="template.generate.latex.pagestyle"/></simpara></listitem>
				<listitem><simpara><xref linkend="template.gentext.dingbat"/></simpara></listitem>
				<listitem><simpara><xref linkend="template.latex.hyperref.preamble"/></simpara></listitem>
				<listitem><simpara><xref linkend="template.float.preamble"/></simpara></listitem>
				<listitem><simpara><xref linkend="template.latex.graphicext"/></simpara></listitem>
				<listitem><simpara><xref linkend="template.generate.latex.draft.preamble"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template name="generate.latex.common.preamble">
		<xsl:choose>
			<xsl:when test="$latex.pdf.support=1"><xsl:value-of select="$latex.pdf.preamble"/></xsl:when>
			<xsl:otherwise><xsl:text>\usepackage{graphicx}&#10;</xsl:text></xsl:otherwise>
		</xsl:choose>
		<xsl:if test="local-name(.)='article'">
			<xsl:value-of select="$latex.article.varsets"/>
		</xsl:if>
		<xsl:if test="local-name(.)='book'">
			<xsl:value-of select="$latex.book.varsets"/>
		</xsl:if>
		<xsl:if test="$latex.bridgehead.in.lot=1">
			<xsl:text><![CDATA[
\makeatletter
% redefine the listoffigures and listoftables so that the name of the chapter
% is printed whenever there are figures or tables from that chapter. encourage
% pagebreak prior to the name of the chapter (discourage orphans).
\let\save@@chapter\@chapter
\let\save@@l@figure\l@figure
\let\the@l@figure@leader\relax
\def\@chapter[#1]#2{\save@@chapter[{#1}]{#2}%
\addtocontents{lof}{\protect\def\the@l@figure@leader{\protect\pagebreak[0]\protect\contentsline{chapter}{\protect\numberline{\thechapter}#1}{}{\thepage}}}%
\addtocontents{lot}{\protect\def\the@l@figure@leader{\protect\pagebreak[0]\protect\contentsline{chapter}{\protect\numberline{\thechapter}#1}{}{\thepage}}}%
}
\renewcommand*\l@figure{\the@l@figure@leader\let\the@l@figure@leader\relax\save@@l@figure}
\let\l@table\l@figure
\makeatother
]]></xsl:text>
		</xsl:if>
		<xsl:if test="$latex.use.fancyhdr=1">
			<xsl:text>\usepackage{fancyhdr}&#10;</xsl:text>
			<xsl:text>\renewcommand{\headrulewidth}{0.4pt}&#10;</xsl:text>
			<xsl:text>\renewcommand{\footrulewidth}{0.4pt}&#10;</xsl:text>
			<xsl:if test="$latex.fancyhdr.truncation.partition!=''">
				<xsl:variable name="partition">
					<xsl:value-of select="round(number($latex.fancyhdr.truncation.partition))"/>
				</xsl:variable>
				<xsl:variable name="left.fraction">
					<xsl:choose>
						<xsl:when test="$partition&lt;1">
							<xsl:text>0</xsl:text>
						</xsl:when>
						<xsl:when test="$partition>97">
							<xsl:text>.98</xsl:text>
						</xsl:when>
						<xsl:otherwise>
							<!-- example: 60 becomes .59 -->
							<xsl:value-of select="($partition - 1) div 100"/>
						</xsl:otherwise>
					</xsl:choose>
				</xsl:variable>
				<xsl:variable name="right.fraction" select="0.98 - number($left.fraction)"/>
				<xsl:text>% Safeguard against long headers.&#10;</xsl:text>
				<xsl:text>\IfFileExists{truncate.sty}{&#10;</xsl:text>
				<xsl:text>\usepackage{truncate}&#10;</xsl:text>
				<xsl:text>% Use an ellipsis when text would be larger than x% of the text width.&#10;</xsl:text>
				<xsl:text>% Preserve left/right text alignment using \hfill (works for English).&#10;</xsl:text>
				<xsl:choose>
					<xsl:when test="$latex.fancyhdr.truncation.style='lr'">
						<!-- left vs. right -->
						<xsl:choose>
							<xsl:when test="$left.fraction &gt; 0.02">
								<xsl:text>\fancyhead[ol]{\truncate{</xsl:text><xsl:value-of select="$left.fraction"/><xsl:text>\textwidth}{\sl\leftmark}}&#10;</xsl:text>
								<xsl:text>\fancyhead[el]{\truncate{</xsl:text><xsl:value-of select="$left.fraction"/><xsl:text>\textwidth}{\sl\leftmark}}&#10;</xsl:text>
							</xsl:when>
							<xsl:otherwise>
								<xsl:text>\fancyhead[ol]{}&#10;</xsl:text>
								<xsl:text>\fancyhead[el]{}&#10;</xsl:text>
							</xsl:otherwise>
						</xsl:choose>
						<xsl:choose>
							<xsl:when test="$right.fraction &gt; 0.02">
								<xsl:text>\fancyhead[or]{\truncate{</xsl:text><xsl:value-of select="$right.fraction"/><xsl:text>\textwidth}{\hfill\sl\rightmark}}&#10;</xsl:text>
								<xsl:text>\fancyhead[er]{\truncate{</xsl:text><xsl:value-of select="$right.fraction"/><xsl:text>\textwidth}{\hfill\sl\rightmark}}&#10;</xsl:text>
							</xsl:when>
							<xsl:otherwise>
								<xsl:text>\fancyhead[or]{}&#10;</xsl:text>
								<xsl:text>\fancyhead[er]{}&#10;</xsl:text>
							</xsl:otherwise>
						</xsl:choose>
					</xsl:when>
					<xsl:otherwise>
						<!-- inside vs. outside -->
						<xsl:choose>
							<xsl:when test="$left.fraction &gt; 0.02">
								<xsl:text>\fancyhead[ol]{\truncate{</xsl:text><xsl:value-of select="$left.fraction"/><xsl:text>\textwidth}{\sl\leftmark}}&#10;</xsl:text>
								<xsl:text>\fancyhead[er]{\truncate{</xsl:text><xsl:value-of select="$left.fraction"/><xsl:text>\textwidth}{\hfill\sl\rightmark}}&#10;</xsl:text>
							</xsl:when>
							<xsl:otherwise>
								<xsl:text>\fancyhead[ol]{}&#10;</xsl:text>
								<xsl:text>\fancyhead[er]{}&#10;</xsl:text>
							</xsl:otherwise>
						</xsl:choose>
						<xsl:choose>
							<xsl:when test="$right.fraction &gt; 0.02">
								<xsl:text>\fancyhead[el]{\truncate{</xsl:text><xsl:value-of select="$right.fraction"/><xsl:text>\textwidth}{\sl\leftmark}}&#10;</xsl:text>
								<xsl:text>\fancyhead[or]{\truncate{</xsl:text><xsl:value-of select="$right.fraction"/><xsl:text>\textwidth}{\hfill\sl\rightmark}}&#10;</xsl:text>
							</xsl:when>
							<xsl:otherwise>
								<xsl:text>\fancyhead[el]{}&#10;</xsl:text>
								<xsl:text>\fancyhead[or]{}&#10;</xsl:text>
							</xsl:otherwise>
						</xsl:choose>
					</xsl:otherwise>
				</xsl:choose>
				<xsl:text>}{\typeout{WARNING: truncate.sty wasn't available and functionality was skipped.}}&#10;</xsl:text>
			</xsl:if>
			<xsl:choose>
				<xsl:when test="$latex.fancyhdr.style='natural'">
					<xsl:text><![CDATA[
\makeatletter
% Override the default from fancyhdr (which would be to have all-caps headings).
\newcommand{\dblatex@chaptermark}[1]{\markboth{{\ifnum \c@secnumdepth>\m@ne \@chapapp\ \thechapter. \ \fi #1}}{}}
\def\dblatex@chaptersmark#1{\markboth{{#1}}{}}
\newcommand{\dblatex@sectionmark}[1]{\markright{{\ifnum \c@secnumdepth >\z@ \thesection. \ \fi #1}}}
\let\dblatex@ps@fancy\ps@fancy
\def\ps@fancy{
	\dblatex@ps@fancy
	\let\chaptermark\dblatex@chaptermark
	\let\sectionmark\dblatex@sectionmark
}
\makeatother
]]></xsl:text>
				</xsl:when>
			</xsl:choose>
			<xsl:call-template name="generate.latex.pagestyle"/>
			<!-- 
			Add dollar...
			<xsl:if test="latex.fancyhdr.lh !=''"><xsl:text>\lhead{</xsl:text><xsl:value-of select="$latex.fancyhdr.lh"/><xsl:text>}&#10;</xsl:text></xsl:if>
			<xsl:if test="latex.fancyhdr.ch !=''"><xsl:text>\chead{</xsl:text><xsl:value-of select="$latex.fancyhdr.ch"/><xsl:text>}&#10;</xsl:text></xsl:if>
			<xsl:if test="latex.fancyhdr.rh !=''"><xsl:text>\rhead{</xsl:text><xsl:value-of select="$latex.fancyhdr.rh"/><xsl:text>}&#10;</xsl:text></xsl:if>
			<xsl:if test="latex.fancyhdr.lf !=''"><xsl:text>\lfoot{</xsl:text><xsl:value-of select="$latex.fancyhdr.lf"/><xsl:text>}&#10;</xsl:text></xsl:if>
			<xsl:if test="latex.fancyhdr.cf !=''"><xsl:text>\cfoot{</xsl:text><xsl:value-of select="$latex.fancyhdr.cf"/><xsl:text>}&#10;</xsl:text></xsl:if>
			<xsl:if test="latex.fancyhdr.rf !=''"><xsl:text>\rfoot{</xsl:text><xsl:value-of select="$latex.fancyhdr.rf"/><xsl:text>}&#10;</xsl:text></xsl:if> 
			-->
		</xsl:if>
		<xsl:text>% ---------------------- &#10;</xsl:text>
		<xsl:text>% Most Common Packages   &#10;</xsl:text>
		<xsl:text>% ---------------------- &#10;</xsl:text>
		<xsl:if test="$latex.use.varioref=1">
			<xsl:text>\usepackage[</xsl:text>
			<xsl:value-of select="$latex.varioref.options"/>
			<xsl:text>]{varioref} &#10;</xsl:text>
		</xsl:if>
		<xsl:text>\usepackage{latexsym}         &#10;</xsl:text>
		<xsl:if test="$latex.use.dcolumn=1">
			<xsl:text>\usepackage{dcolumn}      &#10;</xsl:text>
			<xsl:text>% Default decimal point-style column&#10;</xsl:text>
			<xsl:text>\newcolumntype{d}{D{</xsl:text>
			<xsl:call-template name="gentext.dingbat">
				<xsl:with-param name="dingbat">decimalpoint</xsl:with-param>
			</xsl:call-template>
			<xsl:text>}{</xsl:text>
			<xsl:choose>
				<xsl:when test="$latex.decimal.point!=''">
					<xsl:value-of select="$latex.decimal.point"/>
				</xsl:when>
				<xsl:otherwise>
					<xsl:call-template name="gentext.dingbat">
						<xsl:with-param name="dingbat">latexdecimal</xsl:with-param>
					</xsl:call-template>
				 </xsl:otherwise>
			</xsl:choose>
			<xsl:text>}{-1}}&#10;</xsl:text>
		</xsl:if>
		<xsl:text>\usepackage{enumerate}         &#10;</xsl:text>
		<xsl:if test="$latex.use.fancybox=1">
			<!-- must be before \usepackage{fancyvrb} -->
			<xsl:text>\usepackage{fancybox}      &#10;</xsl:text>
		</xsl:if>
		<xsl:text>\usepackage{float}       &#10;</xsl:text>
		<xsl:text>\usepackage{ragged2e}       &#10;</xsl:text>
		<xsl:if test="$latex.use.fancyvrb=1">
			<!-- must be after \usepackage{fancybox} -->
			<xsl:text>\usepackage{fancyvrb}         &#10;</xsl:text>
			<xsl:text>\makeatletter\@namedef{FV@fontfamily@default}{\def\FV@FontScanPrep{}\def\FV@FontFamily{}}\makeatother&#10;</xsl:text>
			<xsl:if test="$latex.fancyvrb.tabsize!=''">
				<xsl:text>\fvset{obeytabs=true,tabsize=</xsl:text>
				<xsl:value-of select="$latex.fancyvrb.tabsize"/>
				<xsl:text>}&#10;</xsl:text>
			</xsl:if>
		</xsl:if>
		<xsl:if test="$latex.use.isolatin1=1">
			<xsl:message>Please use $latex.inputenc='latin1' instead of $latex.use.isolatin1='1'.</xsl:message>
			<xsl:text>\usepackage{isolatin1}         &#10;</xsl:text>
		</xsl:if>
		<xsl:choose>
			<xsl:when test="$latex.use.parskip=1">
				<xsl:text>\usepackage{parskip}         &#10;</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<!-- hack from parksip to stop excess whitespace after figure captions -->
				<xsl:text><![CDATA[\makeatletter
\let\dblatex@center\center\let\dblatex@endcenter\endcenter
\def\dblatex@nolistI{\leftmargin\leftmargini\topsep\z@ \parsep\parskip \itemsep\z@}
\def\center{\let\@listi\dblatex@nolistI\@listi\dblatex@center\let\@listi\@listI\@listi}
\def\endcenter{\dblatex@endcenter}
\makeatother
]]></xsl:text>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:if test="$latex.use.rotating=1"><xsl:text>\usepackage{rotating}         &#10;</xsl:text></xsl:if>
		<xsl:if test="$latex.use.subfigure=1"><xsl:text>\usepackage{subfigure}         &#10;</xsl:text></xsl:if>
		<xsl:if test="$latex.use.tabularx=1"><xsl:text>\usepackage{tabularx}         &#10;</xsl:text></xsl:if>
		<xsl:if test="$latex.use.ltxtable=1 or $latex.use.longtable=1"><xsl:text>\usepackage{longtable}         &#10;</xsl:text></xsl:if>
		<xsl:if test="$latex.use.umoline=1"><xsl:text>\usepackage{umoline}         &#10;</xsl:text></xsl:if>
		<xsl:if test="$latex.use.url=1"><xsl:text>\usepackage{url}         &#10;</xsl:text></xsl:if>
		<xsl:if test="$latex.math.support=1"><xsl:value-of select="$latex.math.preamble"/></xsl:if>

		<!-- Configure document font. -->
		<xsl:if test="$latex.document.font != 'default'">
			<xsl:text>% ---------------&#10;</xsl:text>
			<xsl:text>% Document Font  &#10;</xsl:text>
			<xsl:text>% ---------------&#10;</xsl:text>
			<xsl:text>\usepackage{</xsl:text><xsl:value-of select="$latex.document.font"/><xsl:text>}&#10;</xsl:text>
		</xsl:if>

		<xsl:if test="$latex.use.hyperref=1">
			<xsl:call-template name="latex.hyperref.preamble"/>
		</xsl:if>
		<xsl:value-of select="$latex.admonition.environment"/>
		<xsl:call-template name="latex.float.preamble"/>
		<xsl:call-template name="latex.graphicext"/>
		<xsl:choose>
			<xsl:when test='$latex.caption.swapskip=1'>
				<xsl:text>% --------------------------------------------&#10;</xsl:text>
				<xsl:text>% $latex.caption.swapskip enabled for $formal.title.placement support&#10;</xsl:text>
				<xsl:text>\newlength{\docbooktolatextempskip}&#10;</xsl:text>
				<xsl:text>\newcommand{\captionswapskip}{\setlength{\docbooktolatextempskip}{\abovecaptionskip}</xsl:text>
				<xsl:text>\setlength{\abovecaptionskip}{\belowcaptionskip}</xsl:text>
				<xsl:text>\setlength{\belowcaptionskip}{\docbooktolatextempskip}}&#10;</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text>\newcommand{\captionswapskip}{}&#10;</xsl:text>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:if test='$latex.hyphenation.tttricks=1'>
			<xsl:text>% --------------------------------------------&#10;</xsl:text>
			<xsl:text>% Better linebreaks&#10;</xsl:text>
			<xsl:text>\newcommand{\docbookhyphenatedot}[1]{{\hyphenchar\font=`\.\relax #1\hyphenchar\font=`\-}}&#10;</xsl:text>
			<xsl:text>\newcommand{\docbookhyphenatefilename}[1]{{\hyphenchar\font=`\.\relax #1\hyphenchar\font=`\-}}&#10;</xsl:text>
			<xsl:text>\newcommand{\docbookhyphenateurl}[1]{{\hyphenchar\font=`\/\relax #1\hyphenchar\font=`\-}}&#10;</xsl:text>
		</xsl:if>
		<!--
		<xsl:message>$document.xml.language: '<xsl:value-of select="$document.xml.language"/>'</xsl:message>
		<xsl:message>$latex.language.option: '<xsl:value-of select="$latex.language.option"/>'</xsl:message>
		-->
		<xsl:if test="$latex.language.option!='none'">
			<xsl:text>\usepackage[</xsl:text><xsl:value-of select="$latex.language.option" /><xsl:text>]{babel} &#10;</xsl:text>
		</xsl:if>
		<xsl:if test="$latex.use.hyperref='1'">
			<xsl:text>% Guard against a problem with old package versions.&#10;</xsl:text>
			<xsl:text>\makeatletter&#10;</xsl:text>
			<xsl:text>\AtBeginDocument{&#10;</xsl:text>
			<xsl:text>\DeclareRobustCommand\ref{\@refstar}&#10;</xsl:text>
			<xsl:text>\DeclareRobustCommand\pageref{\@pagerefstar}&#10;</xsl:text>
			<xsl:text>}&#10;</xsl:text>
			<xsl:text>\makeatother&#10;</xsl:text>
		</xsl:if>
		<xsl:choose>
			<xsl:when test="$latex.is.draft!=''">
				<xsl:if test="$latex.is.draft=1">
					<xsl:call-template name="generate.latex.draft.preamble"/>
				</xsl:if>
			</xsl:when>
			<xsl:otherwise>
				<xsl:if test="(/set|/book|/article)[1]/@status='draft'">
					<xsl:call-template name="generate.latex.draft.preamble"/>
				</xsl:if>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>

			Common &LaTeX; preamble shared by <doc:db basename="article">article</doc:db> and
			<doc:db basename="book">book</doc:db> when their <sgmltag
			class="attribute">status</sgmltag> is <quote>draft</quote>.

		</refpurpose>
		<doc:description>
			<para>

			

			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.is.draft"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.use.varioref"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:samples>
			<simplelist type='inline'>
				&test_index_draft;
				&test_draft;
			</simplelist>
		</doc:samples>
	</doc:template>
	<xsl:template name="generate.latex.draft.preamble">
		<xsl:choose>
			<xsl:when test="$latex.use.varioref='1'">
				<xsl:message>Combining varioref with showkeys (and hyperref?) is not supported.</xsl:message>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text>\usepackage[color]{showkeys}&#10;</xsl:text>
				<xsl:text>\definecolor{refkey}{gray}{0.5}&#10;</xsl:text>
				<xsl:text>\definecolor{labelkey}{gray}{0.5}&#10;</xsl:text>
				<xsl:text>% Rip off things from showkeys to highlight index references&#10;</xsl:text>
				<xsl:text>\definecolor{indexkey}{gray}{.5}%&#10;</xsl:text>
				<xsl:text>\makeatletter&#10;</xsl:text>
				<xsl:text>\def\SK@indexcolor{\color{indexkey}}&#10;</xsl:text>
				<xsl:text>\def\SK@@@index#1{\@bsphack\SK@\SK@@index{#1}\begingroup\SK@index{#1}\endgroup\@esphack}&#10;</xsl:text>
				<xsl:text>\def\SK@@index#1>#2\SK@{\leavevmode\vbox to\z@{\vss \SK@indexcolor \rlap{\vrule\raise .75em\hbox{}{\circle*{5}}}}}&#10;</xsl:text>
				<xsl:text>\AtBeginDocument{\let\SK@index\index&#10;\let\index\SK@@@index}&#10;</xsl:text>
				<xsl:text>\makeatother&#10;</xsl:text>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose> Unavoidable &LaTeX; preamble shared by <doc:db
		basename="article">articles</doc:db> and <doc:db
		basename="book">books</doc:db>  </refpurpose>
		<doc:description>
			<para>Contains custom commands <emphasis>that you just can't get rid of!</emphasis></para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.use.hyperref"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.suppress.blank.page.headers"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.use.ucs"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.entities"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.ucs.options"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.inputenc"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.fontenc"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="template.latex.preamble.essential.biblio"/></simpara></listitem>
				<listitem><simpara><xref linkend="template.latex.preamble.essential.callout"/></simpara></listitem>
				<listitem><simpara><xref linkend="template.latex.preamble.essential.citation"/></simpara></listitem>
				<listitem><simpara><xref linkend="template.latex.preamble.essential.footnote"/></simpara></listitem>
				<listitem><simpara><xref linkend="template.latex.preamble.essential.glossary"/></simpara></listitem>
				<listitem><simpara><xref linkend="template.latex.preamble.essential.index"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template name="generate.latex.essential.preamble">
		<xsl:text>% --------------------------------------------&#10;</xsl:text>
		<xsl:text>\makeatletter&#10;</xsl:text>
		<xsl:text>\newcommand{\dbz}{\penalty \z@}&#10;</xsl:text>
		<xsl:text>\newcommand{\docbooktolatexpipe}{\ensuremath{|}\dbz}&#10;</xsl:text>
		<xsl:text>\newskip\docbooktolatexoldparskip&#10;</xsl:text>
		<xsl:text>\newcommand{\docbooktolatexnoparskip}{\docbooktolatexoldparskip=\parskip\parskip=0pt plus 1pt}&#10;</xsl:text>
		<xsl:text>\newcommand{\docbooktolatexrestoreparskip}{\parskip=\docbooktolatexoldparskip}&#10;</xsl:text>
		<xsl:if test="$latex.use.hyperref!='1'">
			<xsl:text>\newcommand{\href}[1]{{}}&#10;</xsl:text>
			<xsl:text>\newcommand{\hyperlink}[1]{{}}&#10;</xsl:text>
			<xsl:text>\newcommand{\hypertarget}[2]{#2}&#10;</xsl:text>
		</xsl:if>
		<!-- don't print headings on blank pages -->
		<xsl:if test="$latex.suppress.blank.page.headers=1">
			<xsl:text>\def\cleardoublepage{\clearpage\if@twoside \ifodd\c@page\else\hbox{}\thispagestyle{empty}\newpage\if@twocolumn\hbox{}\newpage\fi\fi\fi}&#10;</xsl:text>
		</xsl:if>
		<xsl:if test="$latex.use.ucs='1'">
			<xsl:text>\usepackage[</xsl:text>
			<xsl:value-of select="$latex.ucs.options"/>
			<xsl:text>]{ucs}&#10;</xsl:text>
		</xsl:if>
		<xsl:if test="$latex.entities='catcode'">
			<xsl:text>\catcode`\&amp;=\active\def&amp;{\@ifnextchar##{\begingroup\@sanitize\@docbooktolatexunicode\@gobble}{\&amp;}}&#10;</xsl:text>
			<xsl:if test="$latex.use.ucs!='1'">
				<xsl:text><![CDATA[
% A few example Unicode characters.
% For full support, use the unicode pacakge from Dominique Unruh/CTAN.
%	\else\ifnum#1=8216{\textquoteleft}%
%	\else\ifnum#1=8217{\textquoteright}%
\newcommand{\unichar}[1]{%
	\ifnum#1=8211{--}%
	\else\ifnum#1=8212{---}%
	\else\ifnum#1=8216{`}%
	\else\ifnum#1=8217{'}%
	\else\ifnum#1=8218{\glq}%
	\else\ifnum#1=8220{``}%
	\else\ifnum#1=8221{''}%
	\else\ifnum#1=8222{\glqq}%
	\else\&\##1;\fi%
	\fi\fi\fi\fi\fi%
	\fi\fi%
}
]]></xsl:text>
			</xsl:if>
			<xsl:text>\def\@docbooktolatexunicode#1;{\endgroup\edef\@dbtemp{#1}\unichar{\@dbtemp}}&#10;</xsl:text>
		</xsl:if>
		<xsl:if test="$latex.inputenc!=''">
			<xsl:text>\usepackage[</xsl:text>
			<xsl:value-of select="$latex.inputenc"/>
			<xsl:text>]{inputenc}&#10;</xsl:text>
		</xsl:if>
		<xsl:if test="$latex.fontenc!=''">
			<xsl:text>\usepackage[</xsl:text>
			<xsl:value-of select="$latex.fontenc"/>
			<xsl:text>]{fontenc}&#10;</xsl:text>
		</xsl:if>
		<!-- make proper headers for unnumbered chapter-level components -->
		<!-- TODO make proper headers for unnumbered section-level components -->
		<xsl:text><![CDATA[
\ifx\dblatex@chaptersmark\@undefined\def\dblatex@chaptersmark#1{\markboth{\MakeUppercase{#1}}{}}\fi
\let\save@makeschapterhead\@makeschapterhead
\def\dblatex@makeschapterhead#1{\vspace*{-80pt}\save@makeschapterhead{#1}}
\def\@makeschapterhead#1{\dblatex@makeschapterhead{#1}\dblatex@chaptersmark{#1}}
]]></xsl:text>
		<xsl:call-template name="latex.preamble.essential.biblio"/>
		<xsl:call-template name="latex.preamble.essential.callout"/>
		<xsl:call-template name="latex.preamble.essential.citation"/>
		<xsl:call-template name="latex.preamble.essential.footnote"/>
		<xsl:call-template name="latex.preamble.essential.glossary"/>
		<xsl:call-template name="latex.preamble.essential.index"/>
		<xsl:text><![CDATA[
\def\docbooktolatexgobble{\expandafter\@gobble}
% Prevent multiple openings of the same aux file
% (happens when backref is used with multiple bibliography environments)
\ifx\AfterBeginDocument\undefined\let\AfterBeginDocument\AtBeginDocument\fi
\AfterBeginDocument{
   \let\latex@@starttoc\@starttoc
   \def\@starttoc#1{%
      \@ifundefined{docbooktolatex@aux#1}{%
         \global\@namedef{docbooktolatex@aux#1}{}%
         \latex@@starttoc{#1}%
      }{}
   }
}
% --------------------------------------------
% Hacks for honouring row/entry/@align
% (\hspace not effective when in paragraph mode)
% Naming convention for these macros is:
% 'docbooktolatex' 'align' {alignment-type} {position-within-entry}
% where r = right, l = left, c = centre
\newcommand{\docbooktolatex@align}[2]{\protect\ifvmode#1\else\ifx\LT@@tabarray\@undefined#2\else#1\fi\fi}
\newcommand{\docbooktolatexalignll}{\docbooktolatex@align{\raggedright}{}}
\newcommand{\docbooktolatexalignlr}{\docbooktolatex@align{}{\hspace*\fill}}
\newcommand{\docbooktolatexaligncl}{\docbooktolatex@align{\centering}{\hfill}}
\newcommand{\docbooktolatexaligncr}{\docbooktolatex@align{}{\hspace*\fill}}
\newcommand{\docbooktolatexalignrl}{\protect\ifvmode\raggedleft\else\hfill\fi}
\newcommand{\docbooktolatexalignrr}{}
\ifx\captionswapskip\@undefined\newcommand{\captionswapskip}{}\fi
]]></xsl:text>
		<xsl:text>\makeatother&#10;</xsl:text>
	</xsl:template>

	<doc:template>
		<refpurpose> Preamble for certain floats </refpurpose>
	</doc:template>
	<xsl:template name="latex.float.preamble">
		<xsl:text>% --------------------------------------------&#10;</xsl:text>
		<xsl:text>% Commands to manage/style/create floats      &#10;</xsl:text>
		<xsl:text>% figures, tables, algorithms, examples, eqn  &#10;</xsl:text>
		<xsl:text>% --------------------------------------------&#10;</xsl:text>
		<xsl:text> \floatstyle{ruled}&#10;</xsl:text>
		<xsl:text> \restylefloat{figure}&#10;</xsl:text>
		<xsl:text> \floatstyle{ruled}&#10;</xsl:text>
		<xsl:text> \restylefloat{table}&#10;</xsl:text>
		<xsl:text> \floatstyle{ruled}&#10;</xsl:text>
		<xsl:text> \newfloat{program}{ht}{lop}[section]&#10;</xsl:text>
		<xsl:text> \floatstyle{ruled}&#10;</xsl:text>
		<xsl:text> \newfloat{example}{ht}{loe}[section]&#10;</xsl:text>
		<xsl:text> \floatname{example}{</xsl:text>
		<xsl:call-template name="gentext.element.name">
			<xsl:with-param name="element.name">example</xsl:with-param>
		</xsl:call-template>
		<xsl:text>}&#10;</xsl:text>
		<xsl:text> \floatstyle{ruled}&#10;</xsl:text>
		<xsl:text> \newfloat{dbequation}{ht}{loe}[section]&#10;</xsl:text>
		<xsl:text> \makeatletter\def\toclevel@dbequation{0}\makeatother&#10;</xsl:text>
		<xsl:text> \floatname{dbequation}{</xsl:text>
		<xsl:call-template name="gentext.element.name">
			<xsl:with-param name="element.name">equation</xsl:with-param>
		</xsl:call-template>
		<xsl:text>}&#10;</xsl:text>
		<xsl:text> \floatstyle{boxed}&#10;</xsl:text>
		<xsl:text> \newfloat{algorithm}{ht}{loa}[section]&#10;</xsl:text>
		<xsl:text> \floatname{algorithm}{Algorithm}&#10;</xsl:text>
	</xsl:template>

	<doc:param name="latex.pdf.preamble" xmlns="">
		<doc:description>
			<screen>
				\usepackage{ifthen}
				% --------------------------------------------
				% Check for PDFLaTeX/LaTeX 
				% --------------------------------------------
				\newif\ifpdf
				\ifx\pdfoutput\undefined
				\pdffalse % we are not running PDFLaTeX
				\else
				\pdfoutput=1 % we are running PDFLaTeX
				\pdftrue
				\fi
				% --------------------------------------------
				% Load graphicx package with pdf if needed 
				% --------------------------------------------
				\ifpdf
				\usepackage[pdftex]{graphicx}
				\pdfcompresslevel=9
				\else
				\usepackage{graphicx}
				\fi
			</screen>
		</doc:description>
	</doc:param>
	<xsl:param name="latex.pdf.preamble">
		<xsl:text>\usepackage{ifthen}&#10;</xsl:text>
		<xsl:text>% --------------------------------------------&#10;</xsl:text>
		<xsl:text>% Check for PDFLaTeX/LaTeX &#10;</xsl:text>
		<xsl:text>% --------------------------------------------&#10;</xsl:text>
		<xsl:text>\newif\ifpdf&#10;</xsl:text>
		<xsl:text>\ifx\pdfoutput\undefined&#10;</xsl:text>
		<xsl:text>\pdffalse % we are not running PDFLaTeX&#10;</xsl:text>
		<xsl:text>\else&#10;</xsl:text>
		<xsl:text>\pdfoutput=1 % we are running PDFLaTeX&#10;</xsl:text>
		<xsl:text>\pdftrue&#10;</xsl:text>
		<xsl:text>\fi&#10;</xsl:text>
		<xsl:text>% --------------------------------------------&#10;</xsl:text>
		<xsl:text>% Load graphicx package with pdf if needed &#10;</xsl:text>
		<xsl:text>% --------------------------------------------&#10;</xsl:text>
		<xsl:text>\ifpdf&#10;</xsl:text>
		<xsl:text>\usepackage[pdftex]{graphicx}&#10;</xsl:text>
		<xsl:text>\pdfcompresslevel=9&#10;</xsl:text>
		<xsl:text>\else&#10;</xsl:text>
		<xsl:text>\usepackage{graphicx}&#10;</xsl:text>
		<xsl:text>\fi&#10;</xsl:text>
	</xsl:param>

	<doc:template name="latex.hyperref.preamble" xmlns="">
		<refpurpose> Manage the part of the preamble that handles the hyperref package.</refpurpose>
		<doc:description>
			<para> This template outputs the LaTeX code <literal>\usepackage[...]{hyperref}</literal>
			in order to use hyperlinks, backrefs and other goodies. If PDF support is activated, 
			outputs laTeX code to detect whether the document is being compiled with 
			<filename>pdflatex</filename> or <filename>latex</filename> to supply the
			right parameters (pdftex, dvips, etc).
			<doc:todo> The package options should be optained
			from XSL variables.</doc:todo>
			</para>
			<para>Default Value with PDF support:
			<screen>
				% --------------------------------------------
				% Load hyperref package with pdf if needed 
				% --------------------------------------------
				\ifpdf
				\usepackage[pdftex,bookmarksnumbered,colorlinks,backref, bookmarks, breaklinks, linktocpage]{hyperref}
				\else
				\usepackage[bookmarksnumbered,colorlinks,backref, bookmarks, breaklinks, linktocpage]{hyperref}
				\fi
				% --------------------------------------------
			</screen>
			</para>
			<para>Default Value without PDF support:
			<screen>
				% --------------------------------------------
				% Load hyperref package 
				% --------------------------------------------
				\usepackage[bookmarksnumbered,colorlinks,backref, bookmarks, breaklinks, linktocpage]{hyperref}
			</screen>
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.pdf.support"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.hyperref.param.common"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.hyperref.param.pdftex"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.hyperref.param.dvips"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
	</doc:template>
	<xsl:template name="latex.hyperref.preamble">
		<xsl:choose>
		<xsl:when test="$latex.pdf.support=1">
			<xsl:text>% --------------------------------------------&#10;</xsl:text>
			<xsl:text>% Load hyperref package with pdf if needed &#10;</xsl:text>
			<xsl:text>% --------------------------------------------&#10;</xsl:text>
			<xsl:text>\ifpdf&#10;</xsl:text>
			<xsl:text>\usepackage[pdftex,</xsl:text>
			<xsl:value-of select="$latex.hyperref.param.common" />
			<xsl:text>,</xsl:text>
			<xsl:value-of select="$latex.hyperref.param.pdftex" />
			<xsl:text>]{hyperref}&#10;</xsl:text>
			<xsl:text>\else&#10;</xsl:text>
			<xsl:text>\usepackage[</xsl:text>
			<xsl:value-of select="$latex.hyperref.param.common" />
			<xsl:text>,</xsl:text>
			<xsl:value-of select="$latex.hyperref.param.dvips" />
			<xsl:text>]{hyperref}&#10;</xsl:text>
			<!--
			<xsl:text>\makeatletter\def\pdfmark@[#1]#2{}\makeatother&#10;</xsl:text>
			-->
			<xsl:text>\fi&#10;</xsl:text>
			<xsl:text>% --------------------------------------------&#10;</xsl:text>
		</xsl:when>
		<xsl:otherwise>
			<xsl:text>% --------------------------------------------&#10;</xsl:text>
			<xsl:text>% Load hyperref package &#10;</xsl:text>
			<xsl:text>% --------------------------------------------&#10;</xsl:text>
			<xsl:text>\usepackage[</xsl:text>
			<xsl:value-of select="$latex.hyperref.param.common" />
			<xsl:text>,</xsl:text>
			<xsl:value-of select="$latex.hyperref.param.dvips" />
			<xsl:text>]{hyperref}&#10;</xsl:text>
			<!--
			<xsl:text>\makeatletter\def\pdfmark@[#1]#2{}\makeatother&#10;</xsl:text>
			-->
		</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

	<doc:param xmlns="">
		<refpurpose> &LaTeX; mathematics support  </refpurpose>
	</doc:param>
	<xsl:param name="latex.math.preamble">
		<xsl:text>% --------------------------------------------&#10;</xsl:text>
		<xsl:text>% Math support                                &#10;</xsl:text>
		<xsl:text>% --------------------------------------------&#10;</xsl:text>
		<xsl:text>\usepackage{amsmath,amsthm, amsfonts, amssymb, amsxtra,amsopn}&#10;</xsl:text>
		<xsl:text>%\newtheorem{thm}{Theorem}[section]&#10;</xsl:text>
		<xsl:text>%\newtheorem{cor}[section]{Corollary}&#10;</xsl:text>
		<xsl:text>%\newtheorem{lem}[section]{Lemma}&#10;</xsl:text>
		<xsl:text>%\newtheorem{defn}[section]{Definition}&#10;</xsl:text>
		<xsl:text>%\newtheorem{prop}[section]{Proposition}&#10;</xsl:text>
		<xsl:text>%\newtheorem{ax}{Axiom}&#10;</xsl:text>
		<xsl:text>%\newtheorem{theorem}[section]{Theorem}&#10;</xsl:text>
		<xsl:text>%\newtheorem{corollary}{Corollary}&#10;</xsl:text>
		<xsl:text>%\newtheorem{lemma}{Lemma}&#10;</xsl:text>
		<xsl:text>%\newtheorem{proposition}{Proposition}&#10;</xsl:text>
		<xsl:text>%\theoremstyle{definition}&#10;</xsl:text>
		<xsl:text>%\newtheorem{definition}{Definition}&#10;</xsl:text>
		<xsl:text>%\theoremstyle{remark}&#10;</xsl:text>
		<xsl:text>%\newtheorem{rem}{Remark}&#10;</xsl:text>
		<xsl:text>%\newtheorem*{notation}{Notation}&#10;</xsl:text>
		<xsl:text>%\newcommand{\ntt}{\normalfont\ttfamily}&#10;</xsl:text>
		<xsl:text>%\newcommand{\thmref}[1]{Theorem~\ref{#1}}&#10;</xsl:text>
		<xsl:text>%\newcommand{\secref}[1]{\S\ref{#1}}&#10;</xsl:text>
		<xsl:text>%\newcommand{\lemref}[1]{Lemma~\ref{#1}}&#10;</xsl:text>
		<xsl:text> \newcommand{\bysame}{\mbox{\rule{3em}{.4pt}}\,}&#10;</xsl:text>
		<xsl:text> \newcommand{\A}{\mathcal{A}}&#10;</xsl:text>
		<xsl:text> \newcommand{\B}{\mathcal{B}}&#10;</xsl:text>
		<xsl:text> \newcommand{\XcY}{{(X,Y)}}&#10;</xsl:text>
		<xsl:text> \newcommand{\SX}{{S_X}}&#10;</xsl:text>
		<xsl:text> \newcommand{\SY}{{S_Y}}&#10;</xsl:text>
		<xsl:text> \newcommand{\SXY}{{S_{X,Y}}}&#10;</xsl:text>
		<xsl:text> \newcommand{\SXgYy}{{S_{X|Y}(y)}}&#10;</xsl:text>
		<xsl:text> \newcommand{\Cw}[1]{{\hat C_#1(X|Y)}}&#10;</xsl:text>
		<xsl:text> \newcommand{\G}{{G(X|Y)}}&#10;</xsl:text>
		<xsl:text> \newcommand{\PY}{{P_{\mathcal{Y}}}}&#10;</xsl:text>
		<xsl:text> \newcommand{\X}{\mathcal{X}}&#10;</xsl:text>
		<xsl:text> \newcommand{\wt}{\widetilde}&#10;</xsl:text>
		<xsl:text> \newcommand{\wh}{\widehat}&#10;</xsl:text>
		<xsl:text> % --------------------------------------------&#10;</xsl:text>
		<xsl:text> %\DeclareMathOperator{\per}{per}&#10;</xsl:text>
		<xsl:text> \DeclareMathOperator{\cov}{cov}&#10;</xsl:text>
		<xsl:text> \DeclareMathOperator{\non}{non}&#10;</xsl:text>
		<xsl:text> \DeclareMathOperator{\cf}{cf}&#10;</xsl:text>
		<xsl:text> \DeclareMathOperator{\add}{add}&#10;</xsl:text>
		<xsl:text> \DeclareMathOperator{\Cham}{Cham}&#10;</xsl:text>
		<xsl:text> \DeclareMathOperator{\IM}{Im}&#10;</xsl:text>
		<xsl:text> \DeclareMathOperator{\esssup}{ess\,sup}&#10;</xsl:text>
		<xsl:text> \DeclareMathOperator{\meas}{meas}&#10;</xsl:text>
		<xsl:text> \DeclareMathOperator{\seg}{seg}&#10;</xsl:text>
		<xsl:text>% --------------------------------------------&#10;</xsl:text>
	</xsl:param>

	<doc:template xmlns="">
		<refpurpose> Declared graphic extensions </refpurpose>
		<doc:description>
		<para>
		This template checks whether the user has overridden <command>grafic.default.extension</command>
		Otherwise, declares .pdf, .png, .jpg if using pdflatex and .eps if using latex.
		</para>
		<programlisting><![CDATA[
<xsl:template name="latex.graphicext">
<xsl:choose>
<xsl:when test="$graphic.default.extension !=''">
	<xsl:text>\DeclareGraphicsExtensions{</xsl:text>
	<xsl:if test="not(contains($graphic.default.extension,'.'))">
		<xsl:text>.</xsl:text>
	</xsl:if>
	<xsl:value-of select="$graphic.default.extension"/>
	<xsl:text>}&#10;</xsl:text>
</xsl:when>
<xsl:otherwise>
	<xsl:choose>
	<xsl:when test="$latex.pdf.support=1">
		<xsl:text>\ifpdf&#10;</xsl:text>
		<xsl:text>\DeclareGraphicsExtensions{.pdf,.png,.jpg}&#10;</xsl:text>
		<xsl:text>\else&#10;</xsl:text>
		<xsl:text>\DeclareGraphicsExtensions{.eps}&#10;</xsl:text>
		<xsl:text>\fi&#10;</xsl:text>
	</xsl:when>
	<xsl:otherwise>
		<xsl:text>\DeclareGraphicsExtensions{.eps}&#10;</xsl:text>
	</xsl:otherwise>
	</xsl:choose>
</xsl:otherwise>
</xsl:choose>
</xsl:template>
]]></programlisting>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.graphic.default.extension"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.pdf.support"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
	</doc:template>
	<xsl:template name="latex.graphicext">
		<xsl:choose>
			<xsl:when test="$graphic.default.extension !=''">
				<xsl:text>\DeclareGraphicsExtensions{</xsl:text>
				<xsl:if test="not(contains($graphic.default.extension,'.'))">
					<xsl:text>.</xsl:text>
				</xsl:if>
				<xsl:value-of select="$graphic.default.extension"/>
				<xsl:text>}&#10;</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<xsl:choose>
				<xsl:when test="$latex.pdf.support=1">
					<xsl:text>\ifpdf&#10;</xsl:text>
					<xsl:text>\DeclareGraphicsExtensions{.pdf,.png,.jpg}&#10;</xsl:text>
					<xsl:text>\else&#10;</xsl:text>
					<xsl:text>\DeclareGraphicsExtensions{.eps}&#10;</xsl:text>
					<xsl:text>\fi&#10;</xsl:text>
				</xsl:when>
				<xsl:otherwise>
					<xsl:text>\DeclareGraphicsExtensions{.eps}&#10;</xsl:text>
				</xsl:otherwise>
				</xsl:choose>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>
</xsl:stylesheet>

