<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: xref.mod.xsl,v 1.41 2004/01/28 02:07:08 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
+ ############################################################################## -->

<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="xref" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: xref.mod.xsl,v 1.41 2004/01/28 02:07:08 j-devenish Exp $
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
		<title>Cross References <filename>xref.mod.xsl</filename></title>
		<partintro>
			<para>
			
				Portions (c) Norman Walsh, official DocBook XSL stylesheets. See docbook.sf.net
			
			</para>
		</partintro>
	</doc:reference>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>anchor</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Identify a place in the document for cross references.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				Inserts a &LaTeX; <function condition="latex">hypertarget</function>
				with the current node's id value and no content.
				<doc:todo>Check to see whether this causes typesetting problems
				with <command>latex</command> (as opposed to
				<command>pdflatex</command>) due to insertion of errant
				<literal>pdfmark</literal>s.</doc:todo>
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="anchor">
		<xsl:param name="id" select="@id"/>
		<xsl:text>\hypertarget{</xsl:text>
		<xsl:value-of select="$id"/>
		<xsl:text>}{}</xsl:text>
	</xsl:template>

	<xsl:key name="cross-refs" match="xref|link" use="@linkend"/>
	<doc:template name="id.is.xrefed" xmlns="">
		<refpurpose>Auxiliary named template</refpurpose>
		<doc:description>
		<para>This template returns 1 if there exists somewhere an xref
		or link whose linkend is the target's id.</para>
		</doc:description>
	</doc:template>
	<xsl:template name="id.is.xrefed">
		<xsl:param name="target" select="."/>
		<xsl:variable name="id">
			<xsl:call-template name="generate.label.id">
				<xsl:with-param name="object" select="$target"/>
			</xsl:call-template>
		</xsl:variable>
		<xsl:choose>
			<xsl:when test="count(key('cross-refs', $id))&gt;0">
				<xsl:text>1</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text>0</xsl:text>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

<doc:template name="xref.xreflabel" xmlns="">
  <refpurpose>Auxiliary named template</refpurpose>
  <doc:description>
    <para> Called to process an xreflabel...you might use this to make 
     xreflabels come out in the right font for different targets, 
	 for example.</para>
  </doc:description>
</doc:template>
    <xsl:template name="xref.xreflabel">
	<xsl:param name="target" select="."/>
	<xsl:value-of select="$target/@xreflabel"/>
    </xsl:template>

	<doc:template match="xref|link" xmlns="">
		<refpurpose>Xref and Link XSL Template</refpurpose>
		<doc:description>
			<para>
				<doc:todo>Undocumented.</doc:todo>
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.insert.xref.page.number"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.use.varioref"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
	</doc:template>
    <xsl:template match="xref|link">
	<xsl:variable name="targets" select="key('id',@linkend)"/>
	<xsl:variable name="target" select="$targets[1]"/>
	<xsl:variable name="refelem" select="local-name($target)"/>
	<xsl:call-template name="check.id.unique"><xsl:with-param name="linkend" select="@linkend"/></xsl:call-template>
	<xsl:choose>
	    <xsl:when test="$refelem=''">
		<xsl:message><xsl:text>XRef to nonexistent id: </xsl:text><xsl:value-of select="@linkend"/></xsl:message>
		<xsl:text>XrefId[?</xsl:text>
		<xsl:if test="local-name(.)='link'"><xsl:apply-templates/></xsl:if>
		<xsl:text>?]</xsl:text>
	    </xsl:when>

	    <!-- This is a link with content ... -->
		<xsl:when test="local-name(.)='link' and .!=''">
		<xsl:call-template name="generate.hyperlink">
			<xsl:with-param name="target" select="$target"/>
			<xsl:with-param name="text"><xsl:apply-templates/></xsl:with-param>
		</xsl:call-template>
		</xsl:when>
		
	    <xsl:otherwise>
		<xsl:choose>
		    <xsl:when test="@endterm">
			<xsl:variable name="etargets" select="key('id',@endterm)"/>
			<xsl:variable name="etarget" select="$etargets[1]"/>
			<xsl:choose>
			    <xsl:when test="count($etarget) = 0">
				<xsl:message>
				    <xsl:value-of select="count($etargets)"/>
				    <xsl:text>Endterm points to nonexistent ID: </xsl:text>
				    <xsl:value-of select="@endterm"/>
				</xsl:message>
				<xsl:text>[NONEXISTENT ID]</xsl:text>
			    </xsl:when>
			    <xsl:otherwise>
				<xsl:call-template name="generate.hyperlink">
					<xsl:with-param name="target" select="$target"/>
					<xsl:with-param name="text">
						<xsl:call-template name="generate.xref.text">
							<xsl:with-param name="target" select="$etarget"/>
						</xsl:call-template>
					</xsl:with-param>
				</xsl:call-template>
			    </xsl:otherwise>
			</xsl:choose>
		    </xsl:when>
			<!-- If an xreflabel has been specified for the target ... -->
			<xsl:when test="local-name(.)='xref' and $target/@xreflabel">
			<xsl:call-template name="generate.hyperlink">
				<xsl:with-param name="target" select="$target"/>
				<xsl:with-param name="text">
					<xsl:text>{[</xsl:text>
					<xsl:call-template name="xref.xreflabel">
						<xsl:with-param name="target" select="$target"/>
					</xsl:call-template>
					<xsl:text>]}</xsl:text>
				</xsl:with-param>
			</xsl:call-template>
			</xsl:when>
		    <xsl:otherwise>
				<xsl:call-template name="generate.hyperlink">
					<xsl:with-param name="target" select="$target"/>
					<xsl:with-param name="text">
						<xsl:call-template name="generate.xref.text">
							<xsl:with-param name="target" select="$target"/>
						</xsl:call-template>
					</xsl:with-param>
				</xsl:call-template>
		    </xsl:otherwise>
		</xsl:choose>
	    </xsl:otherwise>
	</xsl:choose>
	<xsl:if test="$insert.xref.page.number=1 and not($latex.use.varioref='1') and $refelem!='' and local-name(.)='xref'">
		<xsl:variable name="xref.text">
			<xsl:call-template name="gentext.template">
				<xsl:with-param name="context" select="'xref'"/>
				<xsl:with-param name="name" select="'page.citation'"/>
			</xsl:call-template>
		</xsl:variable>
		<xsl:for-each select="$target">
			<xsl:call-template name="substitute-markup">
				<xsl:with-param name="template" select="$xref.text"/>
			</xsl:call-template>
		</xsl:for-each>
	</xsl:if>
</xsl:template>

	<doc:template>
		<refpurpose> Generate xref text </refpurpose>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.use.role.as.xrefstyle"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
	</doc:template>
	<xsl:template name="generate.xref.text">
		<xsl:param name="target"/>
        <xsl:apply-templates select="$target" mode="xref-to">
          <xsl:with-param name="referrer" select="."/>
          <xsl:with-param name="xrefstyle">
            <xsl:choose>
              <xsl:when test="@role and not(@xrefstyle) and $use.role.as.xrefstyle != 0">
                <xsl:value-of select="@role"/>
              </xsl:when>
              <xsl:when test="@xrefstyle">
                <xsl:value-of select="@xrefstyle"/>
              </xsl:when>
              <xsl:when test="local-name($target)='title' or local-name($target)='subtitle'">
               <xsl:value-of select="concat(local-name($target), '-unnumbered')"/>
              </xsl:when>
              <xsl:otherwise>
               <xsl:text>xref-number</xsl:text>
              </xsl:otherwise>
            </xsl:choose>
          </xsl:with-param>
        </xsl:apply-templates>
	</xsl:template>

	<doc:template name="generate.hyperlink" xmlns="">
	<refpurpose> Choose hyperlink syntax </refpurpose>
	<doc:description>
		<para>Will use hyperref, if it is available. Otherwise, just outputs
		unlinked text. If the destination is a citation, a backreference is
		emitted (even though it is technically a hyperlink, not a citation).
		If the 'text' arises from an @endterm, then the 'optional argument'
		syntax of <literal>\cite</literal> is used.</para>
	</doc:description>
	</doc:template>
	<xsl:template name="generate.hyperlink">
		<xsl:param name="target"/>
		<xsl:param name="text"/>
		<xsl:variable name="element" select="local-name($target)"/>
		<xsl:variable name="citation" select="$element='biblioentry' or $element='bibliomixed'"/>
		<xsl:choose>
			<xsl:when test="$citation and @endterm!=''">
				<xsl:text>\docbooktolatexcite</xsl:text>
				<xsl:text>{</xsl:text>
				<xsl:value-of select="$target/@id"/>
				<xsl:text>}{</xsl:text>
				<xsl:call-template name="scape-optionalarg">
					<xsl:with-param name="string" select="$text"/>
				</xsl:call-template>
				<xsl:text>}</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<xsl:if test="$latex.use.hyperref=1 and not(ancestor::title)">
					<xsl:text>\hyperlink{</xsl:text>
					<xsl:value-of select="$target/@id"/>
					<xsl:text>}</xsl:text>
				</xsl:if>
				<xsl:text>{</xsl:text>
				<xsl:if test="$citation">
					<xsl:text>\docbooktolatexbackcite{</xsl:text>
					<xsl:value-of select="$target/@id"/>
					<xsl:text>}</xsl:text>
				</xsl:if>
				<xsl:value-of select="$text"/>
				<xsl:text>}</xsl:text>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

	<doc:template basename="insert.label.markup" xmlns="">
		<refpurpose>Numbering template</refpurpose>
		<doc:description>
			<para>
			Let &LaTeX; manage the numbering. Otherwise sty files that
			do specify another numberic (e.g I,II) get messed.
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.insert.xref.page.number"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.use.varioref"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
	</doc:template>
    <xsl:template match="*" mode="insert.label.markup" name="insert.label.markup">
		<xsl:param name="id" select="@id"/>
		<xsl:choose>
			<xsl:when test="$insert.xref.page.number=1 and $latex.use.varioref='1'">
				<xsl:text>{\vref{</xsl:text><xsl:value-of select="$id"/><xsl:text>}}</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text>{\ref{</xsl:text><xsl:value-of select="$id"/><xsl:text>}}</xsl:text>
			</xsl:otherwise>
		</xsl:choose>
    </xsl:template>

	<doc:template basename="insert.label.markup" xmlns="">
		<refpurpose>Numbering template -- uses parent's @id</refpurpose>
		<doc:description>
			<para>
			Calls <xref linkend="template.insert.label.markup"/> using parent's @id.
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.insert.xref.page.number"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.use.varioref"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
	</doc:template>
    <xsl:template match="title" mode="insert.label.markup">
		<xsl:call-template name="insert.label.markup">
			<xsl:with-param name="id">
				<xsl:choose>
					<xsl:when test="contains(local-name(..), 'info')">
						<xsl:call-template name="generate.label.id">
							<xsl:with-param name="object" select="../.."/>
						</xsl:call-template>
					</xsl:when>
					<xsl:otherwise>
						<xsl:call-template name="generate.label.id">
							<xsl:with-param name="object" select=".."/>
						</xsl:call-template>
					</xsl:otherwise>
				</xsl:choose>
			</xsl:with-param>
		</xsl:call-template>
    </xsl:template>

	<doc:template xmlns="">
		<refpurpose> Format titles in xref text </refpurpose>
		<doc:params>
			<variablelist>
				<varlistentry>
					<term>title</term>
					<listitem><simpara>The text. This is expected to
					be received from gentext.xsl, in which case it will
					contain no deliberate &LaTeX; commands and must be
					escaped.</simpara></listitem>
				</varlistentry>
				<varlistentry>
					<term>is.component</term>
					<listitem><simpara>Whether the node is considered
					to be a <quote>component</quote> in the sense of &DocBook;.
					If so, the formatting of the title may be different. By default,
					the determination of component elements is performed by the
					<literal>is.component</literal> template in
					<filename>common.xsl</filename>.</simpara></listitem>
				</varlistentry>
			</variablelist>
		</doc:params>
		<doc:description>
			<para>
				Calls <xref linkend="template.normalize-scape"/>. If the node
				is a component type (e.g. appendix, article, chapter, preface,
				bibliography, glossary or index) then gentext.startquote and
				gentext.endquote are placed around the title.
			</para>
		</doc:description>
	</doc:template>
	<xsl:template match="*" mode="insert.title.markup" name="generate.title.markup">
		<xsl:param name="title"/>
		<xsl:param name="is.component">
			<xsl:call-template name="is.component"/>
		</xsl:param>
		<xsl:choose>
			<xsl:when test="$is.component=1">
				<xsl:call-template name="gentext.startquote"/>
				<xsl:call-template name="normalize-scape">
					<xsl:with-param name="string" select="$title"/>
				</xsl:call-template>
				<xsl:call-template name="gentext.endquote"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:call-template name="normalize-scape">
					<xsl:with-param name="string" select="$title"/>
				</xsl:call-template>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose> Format titles in xref text </refpurpose>
		<doc:params>
			<variablelist>
				<varlistentry>
					<term>title</term>
					<listitem><simpara>The text.</simpara></listitem>
				</varlistentry>
				<varlistentry>
					<term>is.component</term>
					<listitem><simpara>Whether the node's parent is considered
					to be a <quote>component</quote> in the sense of &DocBook;.
					</simpara></listitem>
				</varlistentry>
			</variablelist>
		</doc:params>
		<doc:description>
			<para>
				Calls <xref linkend="template.generate.title.markup"/>.
			</para>
		</doc:description>
	</doc:template>
	<xsl:template match="title" mode="insert.title.markup">
		<xsl:param name="title"/>
		<xsl:param name="is.component">
			<xsl:choose>
				<xsl:when test="contains(local-name(..), 'info')">
					<xsl:call-template name="is.component">
						<xsl:with-param name="node" select="../.."/>
					</xsl:call-template>
				</xsl:when>
				<xsl:otherwise>
					<xsl:call-template name="is.component">
						<xsl:with-param name="node" select=".."/>
					</xsl:call-template>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:param>
		<xsl:call-template name="generate.title.markup">
			<xsl:with-param name="title" select="$title"/>
			<xsl:with-param name="is.component" select="$is.component"/>
		</xsl:call-template>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose> Format titles in xref text </refpurpose>
		<doc:description>
			<para>
				Does not add quotation marks around the title
				but does italicise it.
			</para>
		</doc:description>
	</doc:template>
	<xsl:template match="book" mode="insert.title.markup">
		<xsl:param name="title"/>
		<xsl:text>{\textit{</xsl:text>
		<xsl:call-template name="normalize-scape">
			<xsl:with-param name="string" select="$title"/>
		</xsl:call-template>
		<xsl:text>}}</xsl:text>
	</xsl:template>

	<xsl:template match="*" mode="insert.subtitle.markup">
		<xsl:message>Warning: unexpected insert.subtitle.markup in DB2LaTeX</xsl:message>
	</xsl:template>

	<xsl:template match="*" mode="insert.pagenumber.markup">
		<xsl:param name="target" select="."/>
		<xsl:choose>
			<xsl:when test="$latex.use.varioref='1'">
				<xsl:variable name="options"><xsl:call-template name="latex.vpageref.options"/></xsl:variable>
				<xsl:text>\vpageref</xsl:text>
				<xsl:if test="$options!=''">
					<xsl:choose>
						<xsl:when test="contains($options,'[')">
							<xsl:value-of select="$options"/>
						</xsl:when>
						<xsl:otherwise>
							<xsl:text>[</xsl:text>
							<xsl:value-of select="$options"/>
							<xsl:text>]</xsl:text>
						</xsl:otherwise>
					</xsl:choose>
				</xsl:if>
				<xsl:text>{</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text>\pageref{</xsl:text>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:value-of select="$target/@id"/>
		<xsl:text>}</xsl:text>
	</xsl:template>

	<xsl:template match="*" mode="insert.direction.markup">
		<xsl:message>Warning: unexpected insert.direction.markup in DB2LaTeX</xsl:message>
	</xsl:template>

<doc:template match="ulink" xmlns="">
  <refpurpose>A link that addresses its target by means of a URL (Uniform Resource Locator)</refpurpose>
  <doc:variables>
	<itemizedlist>
		<listitem><simpara><xref linkend="param.ulink.show"/></simpara></listitem>
		<listitem><simpara><xref linkend="param.ulink.footnotes"/></simpara></listitem>
		<listitem><simpara><xref linkend="param.latex.ulink.protocols.relaxed"/></simpara></listitem>
		<listitem><simpara><xref linkend="param.latex.hyphenation.tttricks"/></simpara></listitem>
	</itemizedlist>
  </doc:variables>
</doc:template>
    <xsl:template match="ulink" name="ulink">
	<xsl:param name="hyphenation">\docbookhyphenateurl</xsl:param>
	<xsl:param name="url" select="@url"/>
	<xsl:param name="content">
		<xsl:call-template name="trim-outer">
			<xsl:with-param name="string" select="."/>
		</xsl:call-template>
	</xsl:param>
	<xsl:choose>
		<xsl:when test="$content = '' or $content = $url">
			<xsl:call-template name="generate.typeset.url">
				<xsl:with-param name="hyphenation" select="$hyphenation"/>
				<xsl:with-param name="url" select="$url"/>
				<xsl:with-param name="prepend" select="''"/>
			</xsl:call-template>
		</xsl:when>
		<xsl:when test="$latex.ulink.protocols.relaxed='1' and (substring-after($url,':')=$content or substring-after($url,'://')=$content)">
			<xsl:call-template name="generate.typeset.url">
				<xsl:with-param name="hyphenation" select="$hyphenation"/>
				<xsl:with-param name="url" select="$content"/>
				<xsl:with-param name="prepend" select="''"/>
			</xsl:call-template>
			<xsl:if test="$ulink.footnotes='1' and count(ancestor::footnote)=0">
				<xsl:call-template name="generate.ulink.in.footnote">
					<xsl:with-param name="hyphenation" select="$hyphenation"/>
					<xsl:with-param name="url" select="$url"/>
				</xsl:call-template>
			</xsl:if>
		</xsl:when>
		<xsl:when test="$latex.use.tabularx=1 and count(ancestor::table)&gt;0">
			<xsl:apply-templates/>
			<xsl:call-template name="generate.typeset.url">
				<xsl:with-param name="hyphenation" select="$hyphenation"/>
				<xsl:with-param name="url" select="$url"/>
			</xsl:call-template>
		</xsl:when>
		<xsl:when test="$ulink.footnotes='1' or $ulink.show='1'">
			<xsl:apply-templates/>
			<xsl:if test="$ulink.footnotes='1' and count(ancestor::footnote)=0">
				<xsl:call-template name="generate.ulink.in.footnote">
					<xsl:with-param name="hyphenation" select="$hyphenation"/>
					<xsl:with-param name="url" select="$url"/>
				</xsl:call-template>
			</xsl:if>
			<xsl:if test="$ulink.show='1' or ($ulink.footnotes='1' and ancestor::footnote)">
				<xsl:call-template name="generate.typeset.url">
					<xsl:with-param name="hyphenation" select="$hyphenation"/>
					<xsl:with-param name="url" select="$url"/>
				</xsl:call-template>
			</xsl:if>
		</xsl:when>
		<xsl:otherwise>
			<xsl:text>\href{</xsl:text>
				<xsl:call-template name="scape-href">
					<xsl:with-param name="string" select="$url"/>
				</xsl:call-template>
			<xsl:text>}</xsl:text>
			<xsl:text>{</xsl:text>
				<xsl:apply-templates/>
			<xsl:text>}</xsl:text><!-- End Of second argument of \href -->
		</xsl:otherwise>
	</xsl:choose>
    </xsl:template>

<doc:template match="olink" xmlns="">
  <refpurpose>OLink XSL template</refpurpose>
  <doc:description>
  <para></para>
  </doc:description>
</doc:template>
    <xsl:template match="olink">
		<xsl:apply-templates/>
    </xsl:template>

    <xsl:template match="*" name="title.xref">
	<xsl:param name="target" select="."/>
	<xsl:choose>
	    <xsl:when test="name($target) = 'figure'
		or name($target) = 'example'
		or name($target) = 'equation'
		or name($target) = 'table'
		or name($target) = 'dedication'
		or name($target) = 'preface'
		or name($target) = 'bibliography'
		or name($target) = 'glossary'
		or name($target) = 'index'
		or name($target) = 'setindex'
		or name($target) = 'colophon'">
		<xsl:call-template name="gentext.startquote"/>
		<xsl:apply-templates select="$target" mode="title.content"/>
		<xsl:call-template name="gentext.endquote"/>
	    </xsl:when>
	    <xsl:otherwise>
		<xsl:text>{\em </xsl:text><xsl:apply-templates select="$target" mode="title.content"/><xsl:text>}</xsl:text>
	    </xsl:otherwise>
	</xsl:choose>
    </xsl:template>

    <xsl:template match="title" mode="xref">
	<xsl:apply-templates/>
    </xsl:template>

    <xsl:template match="command" mode="xref">
	<xsl:call-template name="inline.boldseq"/>
    </xsl:template>

    <xsl:template match="function" mode="xref">
	<xsl:call-template name="inline.monoseq"/>
    </xsl:template>

	<doc:template xmlns="">
		<refpurpose> Typeset a URL using the <function condition="latex">url</function> or <function condition="latex">href</function> commands </refpurpose>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.url.quotation"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:description>
			<para>If <xref linkend="param.latex.url.quotation"/> is set,
			the <quote>urlstartquote</quote> and <quote>urlendquote</quote>
			gentext dingbats will be emitted.</para>
		</doc:description>
	</doc:template>
	<xsl:template name="generate.typeset.url">
		<xsl:param name="hyphenation"/>
		<xsl:param name="url" select="@url"/>
		<xsl:param name="prepend" select="' '"/>
		<xsl:value-of select="$prepend"/>
		<xsl:if test="$latex.url.quotation=1">
			<xsl:call-template name="gentext.dingbat">
				<xsl:with-param name="dingbat">urlstartquote</xsl:with-param>
			</xsl:call-template>
		</xsl:if>
		<xsl:choose>
			<xsl:when test="$latex.use.url='1'">
				<xsl:text>\url{</xsl:text>
				<xsl:call-template name="scape-url">
					<xsl:with-param name="string" select="$url"/>
				</xsl:call-template>
				<xsl:text>}</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text>\href{</xsl:text>
				<xsl:call-template name="scape-href">
					<xsl:with-param name="string" select="$url"/>
				</xsl:call-template>
				<xsl:text>}{\texttt{</xsl:text>
				<xsl:call-template name="generate.string.url">
					<xsl:with-param name="hyphenation" select="$hyphenation"/>
					<xsl:with-param name="string" select="$url"/>
				</xsl:call-template>
				<xsl:text>}}</xsl:text>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:if test="$latex.url.quotation=1">
			<xsl:call-template name="gentext.dingbat">
				<xsl:with-param name="dingbat">urlendquote</xsl:with-param>
			</xsl:call-template>
		</xsl:if>
	</xsl:template>

	<doc:template name="generate.string.url" xmlns="">
		<refpurpose>Escape and hyphenate a string as a teletype URL.</refpurpose>
		<doc:description>
		<para>
		This template typsets teletype text using slash.hyphen if
		$latex.hyphenation.tttricks is disabled.
		Has two parameters: 'hyphenation' and 'string'.
		</para>
		</doc:description>
	</doc:template>
	<xsl:template name="generate.string.url">
		<xsl:param name="hyphenation" />
		<xsl:param name="string" />
		<xsl:param name="url" select="$string"/>
		<xsl:choose>
			<xsl:when test="$latex.hyphenation.tttricks=1">
				<xsl:value-of select="$hyphenation" />
				<xsl:text>{</xsl:text>
				<xsl:call-template name="normalize-scape"><xsl:with-param name="string" select="$string"/></xsl:call-template>
				<xsl:text>}</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<!-- LaTeX chars are scaped. Each / except the :// is mapped to a /\- -->
				<xsl:call-template name="scape.slash.hyphen"><xsl:with-param name="string" select="$url"/></xsl:call-template>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

	<xsl:template name="xpath.location">
	<!-- abbreviated -->
		<xsl:param name="node" select="."/>
		<xsl:value-of select="local-name($node)"/>
	</xsl:template>

<xsl:template match="*" mode="object.xref.template">
  <xsl:param name="purpose"/>
  <xsl:param name="xrefstyle"/>
  <xsl:param name="referrer"/>

  <xsl:variable name="user-template">
	<xsl:if test="$xrefstyle != '' and not(contains($xrefstyle, ':'))">
    <xsl:call-template name="gentext.template.exists">
      <xsl:with-param name="context" select="$xrefstyle"/>
      <xsl:with-param name="name">
        <xsl:call-template name="xpath.location"/>
      </xsl:with-param>
    </xsl:call-template>
	</xsl:if>
  </xsl:variable>

  <xsl:variable name="context">
    <xsl:choose>
      <xsl:when test="$user-template = 1">
         <xsl:value-of select="$xrefstyle"/>
      </xsl:when>
      <xsl:otherwise>
         <xsl:value-of select="'xref'"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:call-template name="gentext.template">
    <xsl:with-param name="context" select="$context"/>
    <xsl:with-param name="name">
      <xsl:call-template name="xpath.location"/>
    </xsl:with-param>
    <xsl:with-param name="purpose" select="$purpose"/>
    <xsl:with-param name="xrefstyle" select="$xrefstyle"/>
    <xsl:with-param name="referrer" select="$referrer"/>
  </xsl:call-template>

</xsl:template>

<xsl:template match="*" mode="xref-to">
  <xsl:param name="referrer"/>
  <xsl:param name="xrefstyle"/>

  <xsl:message>
    <xsl:text>Don't know what gentext to create for xref to: "</xsl:text>
    <xsl:value-of select="name(.)"/>
    <xsl:text>"</xsl:text>
  </xsl:message>
  <xsl:text>?</xsl:text>
  <xsl:value-of select="$referrer/@linkend"/>
  <xsl:text>?</xsl:text>
</xsl:template>

<xsl:template match="title" mode="xref-to">
	<xsl:param name="referrer"/>
	<xsl:param name="purpose"/>
	<xsl:param name="xrefstyle"/>
	<xsl:param name="name">
		<xsl:choose>
			<xsl:when test="contains(local-name(parent::*), 'info')">
				<xsl:call-template name="xpath.location">
					<xsl:with-param name="node" select="../.."/>
				</xsl:call-template>
			</xsl:when>
			<xsl:otherwise>
				<xsl:call-template name="xpath.location">
					<xsl:with-param name="node" select=".."/>
				</xsl:call-template>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:param>

	<xsl:variable name="template">
		<xsl:variable name="user-template">
			<xsl:if test="$xrefstyle != '' and not(contains($xrefstyle, ':'))">
				<xsl:call-template name="gentext.template.exists">
					<xsl:with-param name="context" select="$xrefstyle"/>
					<xsl:with-param name="name" select="$name"/>
				</xsl:call-template>
			</xsl:if>
		</xsl:variable>
		<xsl:variable name="context">
			<xsl:choose>
				<xsl:when test="$user-template = 1">
					<xsl:value-of select="$xrefstyle"/>
				</xsl:when>
				<xsl:otherwise>
					<xsl:value-of select="'title'"/>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:variable>
		<xsl:call-template name="gentext.template">
			<xsl:with-param name="context" select="$context"/>
			<xsl:with-param name="name" select="$name"/>
			<xsl:with-param name="xrefstyle" select="$xrefstyle"/>
			<xsl:with-param name="purpose" select="$purpose"/>
			<xsl:with-param name="referrer" select="$referrer"/>
		</xsl:call-template>
	</xsl:variable>

	<xsl:call-template name="substitute-markup">
		<xsl:with-param name="purpose" select="$purpose"/>
		<xsl:with-param name="xrefstyle" select="$xrefstyle"/>
		<xsl:with-param name="referrer" select="$referrer"/>
		<xsl:with-param name="template" select="$template"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="abstract|article|authorblurb|bibliodiv|bibliomset
                     |biblioset|blockquote|calloutlist|caution|colophon
                     |constraintdef|formalpara|glossdiv|important|indexdiv
                     |itemizedlist|legalnotice|lot|msg|msgexplan|msgmain
                     |msgrel|msgset|msgsub|note|orderedlist|partintro
                     |productionset|qandadiv|refsynopsisdiv|segmentedlist
                     |set|setindex|sidebar|tip|toc|variablelist|warning"
              mode="xref-to">
  <xsl:param name="referrer"/>
  <xsl:param name="xrefstyle"/>

  <!-- catch-all for things with (possibly optional) titles -->
  <xsl:apply-templates select="." mode="object.xref.markup">
    <xsl:with-param name="purpose" select="'xref'"/>
    <xsl:with-param name="xrefstyle" select="$xrefstyle"/>
    <xsl:with-param name="referrer" select="$referrer"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="author|editor|othercredit|personname" mode="xref-to">
  <xsl:param name="referrer"/>
  <xsl:param name="xrefstyle"/>

  <xsl:call-template name="person.name"/>
</xsl:template>

<xsl:template match="authorgroup" mode="xref-to">
  <xsl:param name="referrer"/>
  <xsl:param name="xrefstyle"/>

  <xsl:call-template name="person.name.list"/>
</xsl:template>

<xsl:template match="figure|example|table|equation" mode="xref-to">
  <xsl:param name="referrer"/>
  <xsl:param name="xrefstyle"/>

  <xsl:apply-templates select="." mode="object.xref.markup">
    <xsl:with-param name="purpose" select="'xref'"/>
    <xsl:with-param name="xrefstyle" select="$xrefstyle"/>
    <xsl:with-param name="referrer" select="$referrer"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="procedure" mode="xref-to">
  <xsl:param name="referrer"/>
  <xsl:param name="xrefstyle"/>

  <xsl:apply-templates select="." mode="object.xref.markup">
    <xsl:with-param name="purpose" select="'xref'"/>
    <xsl:with-param name="xrefstyle" select="$xrefstyle"/>
    <xsl:with-param name="referrer" select="$referrer"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="cmdsynopsis" mode="xref-to">
  <xsl:param name="referrer"/>
  <xsl:param name="xrefstyle"/>

  <xsl:apply-templates select="(.//command)[1]" mode="xref"/>
</xsl:template>

<xsl:template match="funcsynopsis" mode="xref-to">
  <xsl:param name="referrer"/>
  <xsl:param name="xrefstyle"/>

  <xsl:apply-templates select="(.//function)[1]" mode="xref"/>
</xsl:template>

<xsl:template match="dedication|preface|chapter|appendix" mode="xref-to">
  <xsl:param name="referrer"/>
  <xsl:param name="xrefstyle"/>

  <xsl:apply-templates select="." mode="object.xref.markup">
    <xsl:with-param name="purpose" select="'xref'"/>
    <xsl:with-param name="xrefstyle" select="$xrefstyle"/>
    <xsl:with-param name="referrer" select="$referrer"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="bibliography" mode="xref-to">
  <xsl:param name="referrer"/>
  <xsl:param name="xrefstyle"/>

  <xsl:apply-templates select="." mode="object.xref.markup">
    <xsl:with-param name="purpose" select="'xref'"/>
    <xsl:with-param name="xrefstyle" select="$xrefstyle"/>
    <xsl:with-param name="referrer" select="$referrer"/>
  </xsl:apply-templates>
</xsl:template>

<!--
<xsl:template match="biblioentry|bibliomixed" mode="xref-to">
  <xsl:param name="referrer"/>
  <xsl:param name="xrefstyle"/>

  <xsl:text>[</xsl:text>
  <xsl:choose>
    <xsl:when test="string(.) = ''">
      <xsl:variable name="bib" select="document($bibliography.collection)"/>
      <xsl:variable name="id" select="@id"/>
      <xsl:variable name="entry" select="$bib/bibliography/*[@id=$id][1]"/>
      <xsl:choose>
        <xsl:when test="$entry">
          <xsl:choose>
            <xsl:when test="$bibliography.numbered != 0">
              <xsl:number from="bibliography" count="biblioentry|bibliomixed"
                          level="any" format="1"/>
            </xsl:when>
            <xsl:when test="local-name($entry/*[1]) = 'abbrev'">
              <xsl:apply-templates select="$entry/*[1]"/>
            </xsl:when>
            <xsl:otherwise>
              <xsl:value-of select="@id"/>
            </xsl:otherwise>
          </xsl:choose>
        </xsl:when>
        <xsl:otherwise>
          <xsl:message>
            <xsl:text>No bibliography entry: </xsl:text>
            <xsl:value-of select="$id"/>
            <xsl:text> found in </xsl:text>
            <xsl:value-of select="$bibliography.collection"/>
          </xsl:message>
          <xsl:value-of select="@id"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:when>
    <xsl:otherwise>
      <xsl:choose>
        <xsl:when test="$bibliography.numbered != 0">
          <xsl:number from="bibliography" count="biblioentry|bibliomixed"
                      level="any" format="1"/>
        </xsl:when>
        <xsl:when test="local-name(*[1]) = 'abbrev'">
          <xsl:apply-templates select="*[1]"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="@id"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:otherwise>
  </xsl:choose>
  <xsl:text>]</xsl:text>
</xsl:template>
-->

<xsl:template match="glossary" mode="xref-to">
  <xsl:param name="referrer"/>
  <xsl:param name="xrefstyle"/>

  <xsl:apply-templates select="." mode="object.xref.markup">
    <xsl:with-param name="purpose" select="'xref'"/>
    <xsl:with-param name="xrefstyle" select="$xrefstyle"/>
    <xsl:with-param name="referrer" select="$referrer"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="glossentry" mode="xref-to">
  <xsl:choose>
    <xsl:when test="$glossentry.show.acronym = 'primary'">
      <xsl:choose>
        <xsl:when test="acronym|abbrev">
          <xsl:apply-templates select="(acronym|abbrev)[1]"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:apply-templates select="glossterm[1]" mode="xref-to"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:when>
    <xsl:otherwise>
      <xsl:apply-templates select="glossterm[1]" mode="xref-to"/>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template match="glossterm" mode="xref-to">
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match="index" mode="xref-to">
  <xsl:param name="referrer"/>
  <xsl:param name="xrefstyle"/>

  <xsl:apply-templates select="." mode="object.xref.markup">
    <xsl:with-param name="purpose" select="'xref'"/>
    <xsl:with-param name="xrefstyle" select="$xrefstyle"/>
    <xsl:with-param name="referrer" select="$referrer"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="listitem" mode="xref-to">
  <xsl:param name="referrer"/>
  <xsl:param name="xrefstyle"/>

  <xsl:apply-templates select="." mode="object.xref.markup">
    <xsl:with-param name="purpose" select="'xref'"/>
    <xsl:with-param name="xrefstyle" select="$xrefstyle"/>
    <xsl:with-param name="referrer" select="$referrer"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="section|simplesect
                     |sect1|sect2|sect3|sect4|sect5
                     |refsect1|refsect2|refsect3" mode="xref-to">
  <xsl:param name="referrer"/>
  <xsl:param name="xrefstyle"/>

  <xsl:apply-templates select="." mode="object.xref.markup">
    <xsl:with-param name="purpose" select="'xref'"/>
    <xsl:with-param name="xrefstyle" select="$xrefstyle"/>
    <xsl:with-param name="referrer" select="$referrer"/>
  </xsl:apply-templates>
  <!-- What about "in Chapter X"? -->
</xsl:template>

<xsl:template match="bridgehead" mode="xref-to">
  <xsl:param name="referrer"/>
  <xsl:param name="xrefstyle"/>

  <xsl:apply-templates select="." mode="object.xref.markup">
    <xsl:with-param name="purpose" select="'xref'"/>
    <xsl:with-param name="xrefstyle" select="$xrefstyle"/>
    <xsl:with-param name="referrer" select="$referrer"/>
  </xsl:apply-templates>
  <!-- What about "in Chapter X"? -->
</xsl:template>

<xsl:template match="qandaset" mode="xref-to">
  <xsl:param name="referrer"/>
  <xsl:param name="xrefstyle"/>

  <xsl:apply-templates select="." mode="object.xref.markup">
    <xsl:with-param name="purpose" select="'xref'"/>
    <xsl:with-param name="xrefstyle" select="$xrefstyle"/>
    <xsl:with-param name="referrer" select="$referrer"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="qandadiv" mode="xref-to">
  <xsl:param name="referrer"/>
  <xsl:param name="xrefstyle"/>

  <xsl:apply-templates select="." mode="object.xref.markup">
    <xsl:with-param name="purpose" select="'xref'"/>
    <xsl:with-param name="xrefstyle" select="$xrefstyle"/>
    <xsl:with-param name="referrer" select="$referrer"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="qandaentry" mode="xref-to">
  <xsl:param name="referrer"/>
  <xsl:param name="xrefstyle"/>

  <xsl:apply-templates select="question[1]" mode="object.xref.markup">
    <xsl:with-param name="purpose" select="'xref'"/>
    <xsl:with-param name="xrefstyle" select="$xrefstyle"/>
    <xsl:with-param name="referrer" select="$referrer"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="question|answer" mode="xref-to">
  <xsl:param name="referrer"/>
  <xsl:param name="xrefstyle"/>

  <xsl:apply-templates select="." mode="object.xref.markup">
    <xsl:with-param name="purpose" select="'xref'"/>
    <xsl:with-param name="xrefstyle" select="$xrefstyle"/>
    <xsl:with-param name="referrer" select="$referrer"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="part|reference" mode="xref-to">
  <xsl:param name="referrer"/>
  <xsl:param name="xrefstyle"/>

  <xsl:apply-templates select="." mode="object.xref.markup">
    <xsl:with-param name="purpose" select="'xref'"/>
    <xsl:with-param name="xrefstyle" select="$xrefstyle"/>
    <xsl:with-param name="referrer" select="$referrer"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="refentry" mode="xref-to">
  <xsl:param name="referrer"/>
  <xsl:param name="xrefstyle"/>

  <xsl:choose>
    <xsl:when test="refmeta/refentrytitle">
      <xsl:apply-templates select="refmeta/refentrytitle"/>
    </xsl:when>
    <xsl:otherwise>
      <xsl:apply-templates select="refnamediv/refname[1]"/>
    </xsl:otherwise>
  </xsl:choose>
  <xsl:apply-templates select="refmeta/manvolnum"/>
</xsl:template>

<xsl:template match="refnamediv" mode="xref-to">
  <xsl:param name="referrer"/>
  <xsl:param name="xrefstyle"/>

  <xsl:apply-templates select="refname[1]" mode="xref-to">
    <xsl:with-param name="xrefstyle" select="$xrefstyle"/>
    <xsl:with-param name="referrer" select="$referrer"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="refname" mode="xref-to">
  <xsl:param name="referrer"/>
  <xsl:param name="xrefstyle"/>

  <xsl:apply-templates mode="xref-to">
    <xsl:with-param name="xrefstyle" select="$xrefstyle"/>
    <xsl:with-param name="referrer" select="$referrer"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="step" mode="xref-to">
  <xsl:param name="referrer"/>
  <xsl:param name="xrefstyle"/>

  <xsl:call-template name="gentext">
    <xsl:with-param name="key" select="'Step'"/>
  </xsl:call-template>
  <xsl:text> </xsl:text>
  <xsl:apply-templates select="." mode="number"/>
</xsl:template>

<xsl:template match="varlistentry" mode="xref-to">
  <xsl:param name="referrer"/>
  <xsl:param name="xrefstyle"/>

  <xsl:apply-templates select="term[1]" mode="xref-to">
    <xsl:with-param name="xrefstyle" select="$xrefstyle"/>
    <xsl:with-param name="referrer" select="$referrer"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="varlistentry/term" mode="xref-to">
  <!-- to avoid the comma that will be generated if there are several terms -->
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match="co" mode="xref-to">
  <xsl:param name="referrer"/>
  <xsl:param name="xrefstyle"/>

  <xsl:apply-templates select="." mode="callout-bug"/>
</xsl:template>

<xsl:template match="book" mode="xref-to">
  <xsl:param name="referrer"/>
  <xsl:param name="xrefstyle"/>

  <xsl:apply-templates select="." mode="object.xref.markup">
    <xsl:with-param name="purpose" select="'xref'"/>
    <xsl:with-param name="xrefstyle" select="$xrefstyle"/>
    <xsl:with-param name="referrer" select="$referrer"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="para" mode="xref-to">
  <xsl:param name="referrer"/>
  <xsl:param name="xrefstyle"/>

  <xsl:variable name="context" select="(ancestor::simplesect
                                       |ancestor::section
                                       |ancestor::sect1
                                       |ancestor::sect2
                                       |ancestor::sect3
                                       |ancestor::sect4
                                       |ancestor::sect5
                                       |ancestor::refsection
                                       |ancestor::refsect1
                                       |ancestor::refsect2
                                       |ancestor::refsect3
                                       |ancestor::chapter
                                       |ancestor::appendix
                                       |ancestor::preface
                                       |ancestor::partintro
                                       |ancestor::dedication
                                       |ancestor::colophon
                                       |ancestor::bibliography
                                       |ancestor::index
                                       |ancestor::glossary
                                       |ancestor::glossentry
                                       |ancestor::listitem
                                       |ancestor::varlistentry)[last()]"/>

  <xsl:apply-templates select="$context" mode="xref-to"/>
<!--
  <xsl:apply-templates select="." mode="object.xref.markup">
    <xsl:with-param name="purpose" select="'xref'"/>
    <xsl:with-param name="xrefstyle" select="$xrefstyle"/>
    <xsl:with-param name="referrer" select="$referrer"/>
  </xsl:apply-templates>
-->
</xsl:template>

</xsl:stylesheet>
