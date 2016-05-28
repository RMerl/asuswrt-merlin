<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: lists.mod.xsl,v 1.25 2004/01/31 11:53:50 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
+ ############################################################################## -->

<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="lists" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: lists.mod.xsl,v 1.25 2004/01/31 11:53:50 j-devenish Exp $
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
		<title>Lists <filename>lists.mod.xsl</filename></title>
		<partintro>
			<para>
			
			
			
			</para>
		</partintro>
	</doc:reference>

	<doc:template xmlns="">
		<refpurpose>Process titles for <doc:db>variablelist</doc:db>, <doc:db>orderedlist</doc:db>, <doc:db>itemizedlist</doc:db> and <doc:db>simplelist</doc:db> elements</refpurpose>
		<doc:description>
			<para>
			
			Formats a title.
			
			</para>
		</doc:description>
		<doc:variables>
			<variablelist>
				<varlistentry>
					<term><xref linkend="param.latex.list.title.style"/></term>
					<listitem><simpara>
					The &LaTeX; command for formatting titles.
					</simpara></listitem>
				</varlistentry>
			</variablelist>
		</doc:variables>
		<doc:params>
			<variablelist>
				<varlistentry>
					<term>style</term>
					<listitem><simpara>The &LaTeX; command to use. Defaults to
					<xref linkend="param.latex.list.title.style"/>.</simpara></listitem>
				</varlistentry>
			</variablelist>
		</doc:params>
		<doc:notes>
			<para>
				Applies templates as a paragraph, formatted with the specified style.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="variablelist/title|orderedlist/title|itemizedlist/title|simplelist/title">
		<xsl:param name="style" select="$latex.list.title.style"/>
		<xsl:text>&#10;{</xsl:text>
		<xsl:value-of select="$style"/>
		<xsl:text>{</xsl:text>
		<xsl:apply-templates/>
		<xsl:text>}}&#10;</xsl:text>
	</xsl:template>

	<doc:template basename="listitem" xmlns="">
		<refpurpose>Process <doc:db>listitem</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Formats a list item.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				Applies templates within a &LaTeX; <function condition="latex">item</function>
				command.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="listitem">
		<xsl:text>&#10;%--- Item&#10;</xsl:text>
		<xsl:text>\item </xsl:text>
		<xsl:apply-templates/>
		<xsl:text>&#10;</xsl:text>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>itemizedlist</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Formats an itemised list.
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.use.noindent"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:notes>
			<para>
				Applies templates. Uses a &LaTeX; <function condition="env">itemize</function>
				environment.
			</para>
			<para>
				The <sgmltag class="attribute">spacing</sgmltag>=<quote>compact</quote>
				attribute is recognised.
			</para>
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_book;
				&test_chemistry;
				&test_lists;
			</simplelist>
		</doc:samples>
	</doc:template>
	<xsl:template match="itemizedlist">
		<xsl:apply-templates select="node()[not(self::listitem)]"/>
		<xsl:call-template name="compactlist.pre"/>
		<xsl:text>&#10;\begin{itemize}</xsl:text>
		<xsl:call-template name="compactlist.begin"/>
		<xsl:apply-templates select="listitem"/>
		<xsl:text>\end{itemize}&#10;</xsl:text>
		<xsl:call-template name="compactlist.post"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>variablelist</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Formats a list in which each item is denoted by a textual label.
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.use.noindent"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:notes>
			<para>
				Applies templates. Uses a &LaTeX; <function condition="env">description</function>
				environment.
			</para>
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_bind;
				&test_book;
				&test_ddh;
				&test_lists;
			</simplelist>
		</doc:samples>
	</doc:template>
	<xsl:template match="variablelist">
		<xsl:apply-templates select="node()[not(self::varlistentry)]"/>
		<xsl:text>&#10;\begin{description}&#10;</xsl:text>
		<xsl:apply-templates select="varlistentry"/>
		<xsl:text>\end{description}&#10;</xsl:text>
		<xsl:if test="$latex.use.noindent=1">
			<xsl:text>\noindent </xsl:text>
		</xsl:if>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>orderedlist</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Formats a list in which each item is denoted by a numeric label.
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.use.noindent"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:notes>
			<para>The only &DocBook; attribute that is supported is <sgmltag class="attribute">numeration</sgmltag>.</para>
			<para>
				Applies templates. Uses a &LaTeX; <function condition="env">enumerate</function>
				environment.
			</para>
			<para>
				The <sgmltag class="attribute">spacing</sgmltag>=<quote>compact</quote>
				attribute is recognised.
			</para>
			<para>
				The <sgmltag class="attribute">numeration</sgmltag> attribute
				is recognised.
			</para>
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_book;
				&test_chemistry;
			</simplelist>
		</doc:samples>
	</doc:template>
	<xsl:template match="orderedlist">
		<xsl:variable name="numeration">
			<xsl:choose>
				<xsl:when test="@numeration">
					<xsl:value-of select="@numeration"/>
				</xsl:when>
				<xsl:otherwise>
					<xsl:value-of select="arabic"/>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:variable>
		<xsl:apply-templates select="node()[not(self::listitem)]"/>
		<xsl:call-template name="compactlist.pre"/>
		<xsl:text>&#10;\begin{enumerate}</xsl:text>
		<xsl:if test="@numeration">
			<xsl:choose>
			<xsl:when test="@numeration='arabic'"> 	<xsl:text>[1]</xsl:text>&#10;</xsl:when>
			<xsl:when test="@numeration='upperalpha'"><xsl:text>[A]</xsl:text>&#10;</xsl:when>
			<xsl:when test="@numeration='loweralpha'"><xsl:text>[a]</xsl:text>&#10;</xsl:when>
			<xsl:when test="@numeration='upperroman'"><xsl:text>[I]</xsl:text>&#10;</xsl:when>
			<xsl:when test="@numeration='lowerroman'"><xsl:text>[i]</xsl:text>&#10;</xsl:when>
			</xsl:choose>
		</xsl:if>
		<xsl:call-template name="compactlist.begin"/>
		<xsl:apply-templates select="listitem"/>
		<xsl:text>\end{enumerate}&#10;</xsl:text>
		<xsl:call-template name="compactlist.post"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>varlistentry</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Formats a labeled list item.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				Applies templates within a &LaTeX; <function condition="latex">item</function>
				command.
				A comma is inserted between successive <doc:db basename="term">terms</doc:db>.
			</para>
			<para>
				A &LaTeX; <function condition="latex">null{}</function> command is
				inserted after the <function condition="latex">item</function> to
				guard against empty <doc:db basename="listitem">listitems</doc:db>.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="varlistentry">
		<xsl:param name="object" select="listitem/*[1]"/>
		<xsl:param name="next.is.list">
			<xsl:value-of select="count($object[self::itemizedlist or self::orderedlist or self::variablelist])"/>
		</xsl:param>
		<xsl:variable name="id">
			<xsl:call-template name="label.id"/>
		</xsl:variable>
		<xsl:text>% \null and \mbox are tricks to induce different typesetting decisions&#10;</xsl:text>
		<xsl:text>\item[{</xsl:text>
		<xsl:for-each select="term">
			<xsl:apply-templates/>
			<xsl:if test="position()!=last()">
				<xsl:text>, </xsl:text>
			</xsl:if>
		</xsl:for-each>
		<xsl:choose>
			<xsl:when test="$next.is.list=1">
				<xsl:text>}]\mbox{}</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text>}]\null{}</xsl:text>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:apply-templates select="listitem"/>
	</xsl:template>

	<doc:template basename="term" xmlns="">
		<refpurpose>Process <doc:db>varlistentry</doc:db>'s <doc:db>term</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="varlistentry/term">
		<xsl:apply-templates/><xsl:text>, </xsl:text>
	</xsl:template>

	<doc:template basename="listitem" xmlns="">
		<refpurpose>Process <doc:db>listitem</doc:db> elements</refpurpose>
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
				Unlike <xref linkend="template.listitem"/>, the \item
				has been output by the enclosing element's template.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="varlistentry/listitem">
		<xsl:apply-templates/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Generate a <function condition="env">tabular</function> specification</refpurpose>
		<doc:description>
			<para>
			
			Produces a left-aligned tabular specification list.
			
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:params>
			<variablelist>
				<varlistentry>
					<term>cols</term>
					<listitem><simpara>The number of repetitions</simpara></listitem>
				</varlistentry>
			</variablelist>
		</doc:params>
	</doc:template>
	<xsl:template name="generate.simplelist.tabular.string">
		<xsl:param name="cols" select="1"/>
		<xsl:param name="i" select="1"/>
		<xsl:choose>
			<xsl:when test="$i > $cols"></xsl:when>
			<xsl:otherwise>
			<xsl:text>l</xsl:text>
			<xsl:call-template name="generate.simplelist.tabular.string">
				<xsl:with-param name="i" select="$i+1"/>
				<xsl:with-param name="cols" select="$cols"/>
			</xsl:call-template>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

	<doc:template basename="simplelist" xmlns="">
		<refpurpose>Process <doc:db>simplelist</doc:db> elements with inline <doc:db basename="member">members</doc:db></refpurpose>
		<doc:description>
			<para>
			Formats a simple, comma-separated list for a <doc:db>simplelist</doc:db>
			that has a <sgmltag class="attribute">type</sgmltag> attribute equal to <quote>inline</quote>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				This is not made into a paragraph and is not temrinated by a full stop (<quote>period</quote>).
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="simplelist[@type='inline']" name="generate.simplelist.inline">
		<xsl:for-each select="member">
			<xsl:apply-templates/>
			<xsl:if test="position()!=last()">
				<xsl:text>, </xsl:text>
			</xsl:if>
		</xsl:for-each>
	</xsl:template>

	<doc:template basename="simplelist" xmlns="">
		<refpurpose>Process <doc:db>simplelist</doc:db> elements with <quote>horiz</quote> <doc:db basename="member">members</doc:db></refpurpose>
		<doc:description>
			<para>
			Formats a simple, comma-separated list for a <doc:db>simplelist</doc:db>
			that has a <sgmltag class="attribute">type</sgmltag> attribute equal to <quote>horiz</quote>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:params>
			<variablelist>
				<varlistentry>
					<term>environment</term>
					<listitem><simpara>The &LaTeX; tabular-like environment to use. Defaults to <quote>tabular</quote> unless <xref linkend="param.latex.use.ltxtable"/> or <xref linkend="param.latex.use.longtable"/> is enabled, in which case the default is <quote>longtable</quote>.</simpara></listitem>
				</varlistentry>
				<varlistentry>
					<term>cols</term>
					<listitem><simpara>The number of members per line (defaults to the value of the <sgmltag class="attribute">columns</sgmltag> attribute).</simpara></listitem>
				</varlistentry>
			</variablelist>
		</doc:params>
		<doc:notes>
			<para>
				This is formatted as a border-less &LaTeX; table.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="simplelist[@type='horiz']" name="generate.simplelist.horiz">
		<xsl:param name="environment">
			<xsl:choose>
				<xsl:when test="$latex.use.ltxtable='1' or $latex.use.longtable='1'">
					<xsl:text>longtable</xsl:text>
				</xsl:when>
				<xsl:otherwise>
					<xsl:text>tabular</xsl:text>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:param>
		<xsl:param name="cols">
			<xsl:choose>
			<xsl:when test="@columns">
				<xsl:value-of select="@columns"/>
			</xsl:when>
			<xsl:otherwise>1</xsl:otherwise>
			</xsl:choose>
		</xsl:param>
		<xsl:text>&#10;</xsl:text>
		<xsl:text>\begin{</xsl:text>
		<xsl:value-of select="$environment"/>
		<xsl:text>}{</xsl:text>
		<xsl:call-template name="generate.simplelist.tabular.string">
			<xsl:with-param name="cols" select="$cols"/>
		</xsl:call-template>
		<xsl:text>}&#10;</xsl:text>
		<xsl:call-template name="simplelist.horiz">
			<xsl:with-param name="cols" select="$cols"/>
		</xsl:call-template>
		<xsl:text>&#10;\end{</xsl:text>
		<xsl:value-of select="$environment"/>
		<xsl:text>}&#10;</xsl:text>
	</xsl:template>

    <xsl:template name="simplelist.horiz">
	<xsl:param name="cols">1</xsl:param>
	<xsl:param name="cell">1</xsl:param>
	<xsl:param name="members" select="./member"/>
	<xsl:if test="$cell &lt;= count($members)">
	    <xsl:text>&#10;</xsl:text> 
	    <xsl:call-template name="simplelist.horiz.row">
		<xsl:with-param name="cols" select="$cols"/>
		<xsl:with-param name="cell" select="$cell"/>
		<xsl:with-param name="members" select="$members"/>
	    </xsl:call-template>
	    <xsl:text> \\</xsl:text> 
	    <xsl:call-template name="simplelist.horiz">
		<xsl:with-param name="cols" select="$cols"/>
		<xsl:with-param name="cell" select="$cell + $cols"/>
		<xsl:with-param name="members" select="$members"/>
	    </xsl:call-template>
	</xsl:if>
    </xsl:template>

    <xsl:template name="simplelist.horiz.row">
	<xsl:param name="cols">1</xsl:param>
	<xsl:param name="cell">1</xsl:param>
	<xsl:param name="members" select="./member"/>
	<xsl:param name="curcol">1</xsl:param>
	<xsl:if test="$curcol &lt;= $cols">
	    <xsl:choose>
		<xsl:when test="$members[position()=$cell]">
		    <xsl:apply-templates select="$members[position()=$cell]"/>
		    <xsl:text> </xsl:text> 
		    <xsl:if test="$curcol &lt; $cols">
			<xsl:call-template name="generate.latex.cell.separator"/>
		    </xsl:if>
		</xsl:when>
	    </xsl:choose>
	    <xsl:call-template name="simplelist.horiz.row">
		<xsl:with-param name="cols" select="$cols"/>
		<xsl:with-param name="cell" select="$cell+1"/>
		<xsl:with-param name="members" select="$members"/>
		<xsl:with-param name="curcol" select="$curcol+1"/>
	    </xsl:call-template>
	</xsl:if>
    </xsl:template>

	<doc:template basename="simplelist" xmlns="">
		<refpurpose>Process <doc:db>simplelist</doc:db> elements with <quote>vert</quote> <doc:db basename="member">members</doc:db></refpurpose>
		<doc:description>
			<para>
			Formats a simple, comma-separated list for a <doc:db>simplelist</doc:db>
			that has a <sgmltag class="attribute">type</sgmltag> attribute that is either
			empty or equal to <quote>vert</quote>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:params>
			<variablelist>
				<varlistentry>
					<term>environment</term>
					<listitem><simpara>The &LaTeX; tabular-like environment to use. Defaults to <quote>tabular</quote> unless <xref linkend="param.latex.use.ltxtable"/> or <xref linkend="param.latex.use.longtable"/> is set, in which case the default is <quote>longtable</quote>.</simpara></listitem>
				</varlistentry>
				<varlistentry>
					<term>cols</term>
					<listitem><simpara>The number of members per line (defaults to the value of the <sgmltag class="attribute">columns</sgmltag> attribute).</simpara></listitem>
				</varlistentry>
			</variablelist>
		</doc:params>
		<doc:notes>
			<para>
				This is formatted as a border-less &LaTeX; table.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="simplelist|simplelist[@type='vert']" name="generate.simplelist.vert">
		<xsl:param name="environment">
			<xsl:choose>
				<xsl:when test="$latex.use.ltxtable='1' or $latex.use.longtable='1'">
					<xsl:text>longtable</xsl:text>
				</xsl:when>
				<xsl:otherwise>
					<xsl:text>tabular</xsl:text>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:param>
		<xsl:param name="cols">
			<xsl:choose>
			<xsl:when test="@columns">
				<xsl:value-of select="@columns"/>
			</xsl:when>
			<xsl:otherwise>1</xsl:otherwise>
			</xsl:choose>
		</xsl:param>
		<xsl:text>&#10;</xsl:text>
		<xsl:text>\begin{</xsl:text>
		<xsl:value-of select="$environment"/>
		<xsl:text>}{</xsl:text>
		<xsl:call-template name="generate.simplelist.tabular.string">
			<xsl:with-param name="cols" select="$cols"/>
		</xsl:call-template>
		<xsl:text>}&#10;</xsl:text> 
		<xsl:call-template name="simplelist.vert">
			<xsl:with-param name="cols" select="$cols"/>
		</xsl:call-template>
		<xsl:text>&#10;\end{</xsl:text>
		<xsl:value-of select="$environment"/>
		<xsl:text>}&#10;</xsl:text>
	</xsl:template>

    <xsl:template name="simplelist.vert">
	<xsl:param name="cols">1</xsl:param>
	<xsl:param name="cell">1</xsl:param>
	<xsl:param name="members" select="./member"/>
	<xsl:param name="rows" select="floor((count($members)+$cols - 1) div $cols)"/>
	<xsl:if test="$cell &lt;= $rows">
	    <xsl:text>&#10;</xsl:text> 
	    <xsl:call-template name="simplelist.vert.row">
		<xsl:with-param name="cols" select="$cols"/>
		<xsl:with-param name="rows" select="$rows"/>
		<xsl:with-param name="cell" select="$cell"/>
		<xsl:with-param name="members" select="$members"/>
	    </xsl:call-template>
	    <xsl:text> \\</xsl:text> 
	    <xsl:call-template name="simplelist.vert">
		<xsl:with-param name="cols" select="$cols"/>
		<xsl:with-param name="cell" select="$cell+1"/>
		<xsl:with-param name="members" select="$members"/>
		<xsl:with-param name="rows" select="$rows"/>
	    </xsl:call-template>
	</xsl:if>
    </xsl:template>

    <xsl:template name="simplelist.vert.row">
	<xsl:param name="cols">1</xsl:param>
	<xsl:param name="rows">1</xsl:param>
	<xsl:param name="cell">1</xsl:param>
	<xsl:param name="members" select="./member"/>
	<xsl:param name="curcol">1</xsl:param>
	<xsl:if test="$curcol &lt;= $cols">
	    <xsl:choose>
		<xsl:when test="$members[position()=$cell]">
		    <xsl:apply-templates select="$members[position()=$cell]"/>
		    <xsl:text> </xsl:text> 
		    <xsl:if test="$curcol &lt; $cols">
			<xsl:call-template name="generate.latex.cell.separator"/>
		    </xsl:if>
		</xsl:when>
		<xsl:otherwise>
		</xsl:otherwise>
	    </xsl:choose>
	    <xsl:call-template name="simplelist.vert.row">
		<xsl:with-param name="cols" select="$cols"/>
		<xsl:with-param name="rows" select="$rows"/>
		<xsl:with-param name="cell" select="$cell+$rows"/>
		<xsl:with-param name="members" select="$members"/>
		<xsl:with-param name="curcol" select="$curcol+1"/>
	    </xsl:call-template>
	</xsl:if>
    </xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>member</doc:db> elements</refpurpose>
		<doc:description>
			<para>
			Applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="member">
		<xsl:apply-templates/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>segmentedlist</doc:db> elements</refpurpose>
		<doc:description>
			<para>
			Applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:samples>
			<simplelist type='inline'>
				&test_book;
			</simplelist>
		</doc:samples>
	</doc:template>
	<xsl:template match="segmentedlist">
		<xsl:apply-templates select="title|titleabbrev"/>
		<xsl:apply-templates select="seglistitem"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process titles for <doc:db>segmentedlist</doc:db> elements</refpurpose>
		<doc:description>
			<para>
			Formats a title as a paragraph.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:params>
			<variablelist>
				<varlistentry>
					<term>style</term>
					<listitem><simpara>The &LaTeX; command to use.</simpara></listitem>
				</varlistentry>
			</variablelist>
		</doc:params>
		<doc:notes>
			<para>
				Applies templates as a paragraph, formatted with the specified style, and
				terminated with a newline command.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="segmentedlist/title">
		<xsl:param name="style" select="$latex.list.title.style"/>
		<xsl:text>&#10;{</xsl:text>
		<xsl:value-of select="$style"/>
		<xsl:text>{</xsl:text>
		<xsl:apply-templates/>
		<xsl:text>}}\\&#10;</xsl:text>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>segtitle</doc:db> elements</refpurpose>
		<doc:description>
			<para>
			Applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="segtitle">
		<xsl:apply-templates/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>seglistitem</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Formats a segmented list item.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>Applies <doc:db>seg</doc:db> templates. Each item is separated by a &LaTeX;
			<function condition="latex">\</function> command.</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="seglistitem">
		<xsl:apply-templates/>
		<xsl:choose>
			<xsl:when test="position()=last()"><xsl:text>&#10;&#10;</xsl:text></xsl:when>
			<xsl:otherwise><xsl:text> \\&#10;</xsl:text></xsl:otherwise>
		</xsl:choose>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>seg</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Formats a <doc:db>seg</doc:db> with its <doc:db>segtitle</doc:db>.
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.segtitle.style"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
	</doc:template>
	<xsl:template match="seg">
		<xsl:variable name="segnum" select="position()"/>
		<xsl:variable name="seglist" select="ancestor::segmentedlist"/>
		<xsl:variable name="segtitles" select="$seglist/segtitle"/>

		<!--
		Note: segtitle is only going to be the right thing in a well formed
		SegmentedList.  If there are too many Segs or too few SegTitles,
		you'll get something odd...maybe an error
		-->

		<xsl:text> {</xsl:text>
		<xsl:value-of select="$latex.segtitle.style"/>
		<xsl:text>{</xsl:text>
		<xsl:apply-templates select="$segtitles[$segnum=position()]"/>
		<xsl:text>:}} </xsl:text>
		<xsl:apply-templates/>
	</xsl:template>

	<xsl:template name="compactlist.pre">
		<xsl:if test="@spacing='compact'">
			<xsl:if test="$latex.use.parskip=1">
				<xsl:text>&#10;\docbooktolatexnoparskip</xsl:text>
			</xsl:if>
		</xsl:if>
	</xsl:template>

	<xsl:template name="compactlist.begin">
		<xsl:if test="@spacing='compact' and $latex.use.parskip!=1">
			<xsl:text>\setlength{\itemsep}{-0.25em}&#10;</xsl:text>
		</xsl:if>
	</xsl:template>

	<xsl:template name="compactlist.post">
		<xsl:if test="@spacing='compact' and $latex.use.parskip=1">
			<xsl:text>\docbooktolatexrestoreparskip&#10;</xsl:text>
		</xsl:if>
		<xsl:if test="$latex.use.noindent=1">
			<xsl:text>\noindent </xsl:text>
		</xsl:if>
	</xsl:template>

</xsl:stylesheet>
