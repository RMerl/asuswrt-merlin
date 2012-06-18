<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################
|	$Id: param-common.mod.xsl,v 1.12 2004/01/26 13:25:17 j-devenish Exp $
+ ############################################################################## -->

<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="param-common" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: param-common.mod.xsl,v 1.12 2004/01/26 13:25:17 j-devenish Exp $
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
				<doc:revision rcasver="1.1">&rev_2003_05;</doc:revision>
			</revhistory>
		</referenceinfo>
		<title>Parameters: Common Options <filename>param-common.mod.xsl</filename></title>
		<partintro>
			<para>
			
			This file contains parameters that are shared with other XSL
			stylesheets such as those as <ulink
			url="http://docbook.sourceforge.net"/> (see <citetitle>Parameter
			References</citetitle> in the <ulink
			url="http://docbook.sourceforge.net/release/xsl/current/doc/reference.html">DocBook
			XSL Stylesheet Reference Documentation</ulink>). These are
			parameters are honoured so that you can coordinate your XHTML or FO
			stylesheets with &DB2LaTeX;.
			
			</para>
			<note>
				<para>
				
				In some stylesheets, tests of parameter values use
				<quote>!=0</quote> logic. However, &DB2LaTeX; uses
				<quote>=1</quote> logic. This means that empty parameters are
				<quote>off</quote> in &DB2LaTeX; but <quote>on</quote> in those
				other stylesheets. The <ulink
				url="http://www.w3.org">XPath</ulink> values
				<quote>true()</quote> and <quote>false()</quote> work as
				expected.
				
				</para>
			</note>
		</partintro>
	</doc:reference>

	<doc:param xmlns="">
		<refpurpose> &LaTeX; location for admonition graphics </refpurpose>
		<doc:description>
			<para>The file path that will be passed to &LaTeX; in order to find admonition graphics.</para>
			<para>An empty value suppresses the use of admonition graphics.</para>
			<para>If your figures are in <quote>the current directory</quote> then use a value of
			<quote>.</quote> (i.e. the full stop or period on its own) to signify this.</para>
		</doc:description>
	</doc:param>
	<xsl:param name="admon.graphics.path">
		<xsl:choose>
			<xsl:when test="$latex.admonition.path!=''">
				<xsl:message>Warning: $latex.admonition.path is deprecated: use $admon.graphics.path instead</xsl:message>
				<xsl:value-of select="$latex.admonition.path"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text>figures</xsl:text>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:param>
	<xsl:param name="latex.admonition.path"/>

	<doc:param xmlns="">
		<refpurpose> Control the escaping of <doc:db>alt</doc:db> text </refpurpose>
		<doc:description>
			<para>

			Text within <doc:db>alt</doc:db> elements <!--within equation-type
			elements--> is assumed to be valid &LaTeX; and is passed through
			unescaped by default (though you should set its value to
			<quote>plain</quote> or <quote>latex</quote>, which are considered
			confirmative and equivalent by &DB2LaTeX;). If this is not
			appropriate for your document, set this variable to the empty
			value. If you use an explicit <sgmltag
			class="attribute">role</sgmltag> attribute with the values
			<quote>latex</quote> or <quote>tex</quote>, you need not concern
			yourself with this variable. Alt text within equation-type
			elements is currently assumed to be valid &LaTeX; regardless
			of this variable (this is probably a bug!).

			</para>
		</doc:description>
	</doc:param>
	<xsl:param name="tex.math.in.alt">
		<xsl:if test="$latex.alt.is.latex!=''">
			<xsl:message>Warning: $latex.alt.is.latex is deprecated: use $tex.math.in.alt instead</xsl:message>
			<xsl:if test="$latex.alt.is.latex=1">
				<xsl:text>latex</xsl:text>
			</xsl:if>
		</xsl:if>
	</xsl:param>
	<xsl:param name="latex.alt.is.latex"/>

	<doc:param xmlns="">
		<refpurpose> Display <doc:db>remark</doc:db> and <doc:db>comment</doc:db> elements? </refpurpose>
		<doc:description>
			<para>
			
			Enables or disables the display of <doc:db basename="comment">comments</doc:db> and <doc:db basename="remark">remarks</doc:db>.
			By default, this is equal to <xref linkend="param.latex.is.draft"/>.
			
			</para>
		</doc:description>
	</doc:param>
	<xsl:param name="show.comments">
		<xsl:value-of select="$latex.is.draft"/>
	</xsl:param>

	<doc:param xmlns="">
		<refpurpose> Control the display of <doc:db>othername</doc:db> elements in <doc:db basename="author">authors</doc:db> </refpurpose>
		<doc:description>
			<para>
			
			When disabled, <doc:db>othername</doc:db> elements will be suppressed when
			<doc:db>author</doc:db> elements are formatted.
			
			</para>
		</doc:description>
	</doc:param>
	<xsl:param name="author.othername.in.middle" select="1"/>

	<doc:param xmlns="">
		<refpurpose> Separator for bibliography items </refpurpose>
		<doc:description>
			<para><doc:todo>This parameter is under review.</doc:todo></para>
		</doc:description>
	</doc:param>
	<xsl:param name="biblioentry.item.separator">, </xsl:param>

	<doc:param xmlns="">
		<refpurpose> Cull table-of-contents entries that are deeply nested </refpurpose>
		<doc:description>
			<para>Specifies the maximum depth before sections are omitted from the table of contents.</para>
		</doc:description>
	</doc:param>
	<xsl:param name="toc.section.depth">4</xsl:param>

	<doc:param xmlns="">
		<refpurpose> Control the automatic numbering of section, parts, and chapters </refpurpose>
		<doc:description>
			<para>
			Specifies the maximum depth before sections cease to be uniquely numbered.
			This is passed to &LaTeX; using the <literal>secnumdepth</literal> counter.
			Therefore, it is possible to use a value of <quote>0</quote> (zero) to disable section numbering.
			A value of <quote>-1</quote> will disable the numbering of parts and chapters, too.
			</para>
		</doc:description>
	</doc:param>
	<xsl:param name="section.depth">4</xsl:param>

	<doc:param xmlns="">
		<refpurpose> Default filename extension for <function condition="latex">includegraphics</function> </refpurpose>
		<doc:description>
			<para>
				Specify the &LaTeX; search parameters for graphics filenames.
				If empty, &DB2LaTeX; will specify some explicit defaults.
			</para>
		</doc:description>
	</doc:param>
	<xsl:param name="graphic.default.extension"/>

	<doc:param xmlns="">
		<refpurpose> Control <sgmltag class="element">mediaobject</sgmltag> selection methods </refpurpose>
		<doc:description>
			<para>
			
			This controls how &DB2LaTeX; behaves when a <doc:db>figure</doc:db>
			contains multiple <doc:db
			basename="mediaobject">mediaobjects</doc:db>. When enabled,
			&DB2LaTeX; will prefer the <sgmltag>mediaobject</sgmltag> with the
			<quote>latex</quote>, <quote>tex</quote> or <xref
			linkend="param.preferred.mediaobject.role"/> role, if any.
			
			</para>
		</doc:description>
	</doc:param>
	<xsl:param name="use.role.for.mediaobject">1</xsl:param>

	<doc:param xmlns="">
		<refpurpose> Control <sgmltag class="element">mediaobject</sgmltag> selection methods </refpurpose>
		<doc:description>
			<para>
			
			When <xref linkend="param.use.role.for.mediaobject"/> is enabled,
			this variable can be used to specify the
			<doc:db>mediaobject</doc:db> <sgmltag
			class="attribute">role</sgmltag> that your document uses for
			&LaTeX; output. &DB2LaTeX; will try to use this role before using
			the <quote>latex</quote> or <quote>tex</quote> roles. For example,
			some authors may choose to set this to
			<quote><literal>pdf</literal></quote>.
			
			</para>
		</doc:description>
	</doc:param>
	<xsl:param name="preferred.mediaobject.role"/>

	<doc:param xmlns="">
		<refpurpose> Specifies where formal component titles should occur </refpurpose>
		<doc:description>
			<para>

				Titles for the formal object types (figure, example, quation,
				table, and procedure) can be placed before or after those
				objects. The keyword <quote>before</quote> is recognised. All
				other strings qualify as <quote>after</quote>.

			</para>
		</doc:description>
	</doc:param>
	<xsl:param name="formal.title.placement">
		figure not_before
		example before
		equation not_before
		table before
		procedure before
	</xsl:param>

	<doc:param xmlns="">
		<refpurpose> Control the appearance of page numbers in cross references </refpurpose>
		<doc:description>
			<para>

				When enabled, <doc:db basename="xref">xrefs</doc:db> will
				include page numbers after their generated cross-reference
				text.

			</para>
		</doc:description>
		</doc:param>
	<xsl:param name="insert.xref.page.number">0</xsl:param>

	<doc:param xmlns="">
		<refpurpose> Control the display of URLs after <doc:db basename="ulink">ulinks</doc:db> </refpurpose>
		<doc:description>
			<para>

			When this option is enabled, and a ulink has a URL that is different
			from the displayed content, the URL will be typeset after the content.
			If the URL and content are identical, only one of them will appear.
			Otherwise, the URL is hyperlinked and the content is not.

			</para>
		</doc:description>
	</doc:param>
	<xsl:param name="ulink.show">1</xsl:param>

	<doc:param xmlns="">
		<refpurpose> Control the generation of footnotes for ulinks </refpurpose>
		<doc:description>
			<para>

			When this option is enabled, a <doc:db>ulink</doc:db> that has
			content different to its URL will have an associated footnote. The
			contents of the footnote will be the URL. If the ulink is within a
			<doc:db>footnote</doc:db>, the URL is shown after the content.

			</para>
		</doc:description>
	</doc:param>
	<xsl:param name="ulink.footnotes">0</xsl:param>

	<doc:param xmlns="">
		<refpurpose> Honour role as proxy for xrefstyle </refpurpose>
		<doc:description>
			<para>
			
			The <sgmltag class="attribute">xrefstyle</sgmltag> attribute is not
			yet part of &DocBook; so the <sgmltag
			class="attribute">role</sgmltag> attribute can be used until
			xrefstyle is available for <doc:db>xref</doc:db> elements.
			
			</para>
		</doc:description>
	</doc:param>
    <xsl:param name="use.role.as.xrefstyle">0</xsl:param>

	<xsl:variable name="default-classsynopsis-language">java</xsl:variable>
	<doc:param xmlns="">
		<refpurpose> Choose whether to include <doc:db>manvolnum</doc:db> in cross-references </refpurpose>
		<doc:description>
			<para>

				When this option is enabled, <doc:db
				basename="manvolnum">manvolnums</doc:db> will be displayed when
				cross-referencing <doc:db
				basename="refentry">refentries</doc:db>.

			</para>
		</doc:description>
	</doc:param>
	<xsl:param name="refentry.xref.manvolnum" select="1"/>
	<xsl:variable name="funcsynopsis.style">kr</xsl:variable>
	<xsl:variable name="funcsynopsis.decoration" select="1"/>
	<xsl:variable name="function.parens">0</xsl:variable>
	<doc:param xmlns="">
		<refpurpose> Control the use of NAME headers </refpurpose>
		<doc:description>
			<para>

				See <ulink url="http://docbook.sourceforge.net/release/xsl/current/doc/fo/refentry.generate.name.html"/>.

			</para>
		</doc:description>
	</doc:param>
	<xsl:param name="refentry.generate.name" select="1"/>
	<xsl:param name="glossentry.show.acronym" select="'no'"/>

	<xsl:variable name="section.autolabel" select="1"/>
	<xsl:variable name="section.label.includes.component.label" select="0"/>
	<xsl:variable name="chapter.autolabel" select="1"/>
	<xsl:variable name="preface.autolabel" select="0"/>
	<xsl:variable name="part.autolabel" select="1"/>
	<xsl:variable name="qandadiv.autolabel" select="1"/>
	<xsl:variable name="autotoc.label.separator" select="'. '"/>
	<xsl:variable name="qanda.inherit.numeration" select="1"/>
	<xsl:variable name="qanda.defaultlabel">number</xsl:variable>

	<xsl:param name="punct.honorific" select="'.'"/>
	<xsl:param name="stylesheet.result.type" select="'xhtml'"/>
	<xsl:param name="use.svg" select="0"/>
	<xsl:param name="formal.procedures" select="1"/>
	<xsl:param name="xref.with.number.and.title" select="1"/>
	<xsl:param name="xref.label-title.separator">: </xsl:param>
	<xsl:param name="xref.label-page.separator"><xsl:text> </xsl:text></xsl:param>
	<xsl:param name="xref.title-page.separator"><xsl:text> </xsl:text></xsl:param>
	<xsl:template name="is.graphic.extension">
		<xsl:message terminate="yes">Logic error: is.graphic.extension is unsupported.</xsl:message>
	</xsl:template>
	<xsl:template name="is.graphic.format">
		<xsl:message terminate="yes">Logic error: is.graphic.format is unsupported.</xsl:message>
	</xsl:template>
	<xsl:template name="lookup.key">
		<xsl:message terminate="yes">Logic error: lookup.key is unsupported.</xsl:message>
	</xsl:template>
    <xsl:variable name="check.idref">1</xsl:variable>

	<doc:param xmlns="">
		<refpurpose> Process only one element tree within a document </refpurpose>
		<doc:description>
			<para>

				When this variable is non-empty, it is interpreted as the ID of
				an element that should be typeset by &DB2LaTeX;. The element's
				children, but none of its siblings or ancestors, will be
				processed as per normal. When the root element is a
				<doc:db>book</doc:db> or <doc:db>article</doc:db>, that
				component will have its normal infrastructure (including
				<doc:db>bookinfo</doc:db> or <doc:db>articleinfo</doc:db>)
				processed before the <quote>rootid</quote> element.

			</para>
		</doc:description>
	</doc:param>
	<xsl:param name="rootid" select="''"/>

    <!--
    <xsl:variable name="link.mailto.url"></xsl:variable>
    <xsl:variable name="toc.list.type">dl</xsl:variable>
    -->

</xsl:stylesheet>
