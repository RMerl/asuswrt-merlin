<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: labelid.mod.xsl,v 1.5 2004/01/01 12:26:41 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
+ ############################################################################## -->

<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="labelid" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: labelid.mod.xsl,v 1.5 2004/01/01 12:26:41 j-devenish Exp $
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
				<doc:revision rcasver="1.5">&rev_2003_05;</doc:revision>
			</revhistory>
		</referenceinfo>
		<title>Labels and Anchors for Cross-referencing <filename>labelid.mod.xsl</filename></title>
		<partintro>
			<para>
			
			
			
			</para>
		</partintro>
	</doc:reference>

	<doc:template xmlns="">
		<refpurpose> Generate &LaTeX; <function condition="latex">label</function>s </refpurpose>
		<doc:description>
			<para>
		
			This template marks the current object with a <function
			condition="latex">label</function>.

			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<!--
		<doc:params>
			<variablelist>
			<varlistentry><term>object</term>
				<listitem><para>The node whose id is to be used.</para></listitem>
			</varlistentry>
			</variablelist>
		</doc:params>
		<doc:notes>
		</doc:notes>
		-->
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="template.generate.label.id"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template name="label.id">
		<xsl:text>\label{</xsl:text>
		<xsl:call-template name="generate.label.id"/>
		<xsl:text>}</xsl:text>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose> Generate a reliable cross-reference id </refpurpose>
		<doc:description>
			<para>
			
			The passed argument indicates the object whose <sgmltag
			class="attribute">id</sgmltag> attribute is used to generate the
			label. In this sense, in most cases its the current node itself. If
			the used object has not an id attribute, a unique id is obtained by
			means of the <literal>generate-id</literal> function. Moreover, if
			we are using the hyperref package, a hypertarget is also defined
			for this object.
			
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:params>
			<variablelist>
			<varlistentry><term>object</term>
				<listitem><para>The node whose id is to be used.</para></listitem>
			</varlistentry>
			</variablelist>
		</doc:params>
	</doc:template>
	<xsl:template name="generate.label.id">
		<xsl:param name="object" select="."/>
		<xsl:variable name="id">
			<xsl:choose>
			<xsl:when test="$object/@id">
				<xsl:value-of select="$object/@id"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="generate-id($object)"/>
			</xsl:otherwise>
			</xsl:choose>
		</xsl:variable>
		<xsl:value-of select="normalize-space($id)"/>
	</xsl:template>

    <!--
    <xsl:template match="*" mode="label.content">
	<xsl:message>
	    <xsl:text>Request for label of unexpected element: </xsl:text>
	    <xsl:value-of select="name(.)"/>
	</xsl:message>
    </xsl:template>

    <xsl:template match="set|book" mode="label.content">
	<xsl:param name="punct">.</xsl:param>
	<xsl:if test="@label">
	    <xsl:value-of select="@label"/>
	    <xsl:value-of select="$punct"/>
	</xsl:if>
    </xsl:template>

    <xsl:template match="part" mode="label.content">
	<xsl:param name="punct">.</xsl:param>
	<xsl:choose>
	    <xsl:when test="@label">
		<xsl:value-of select="@label"/>
		<xsl:value-of select="$punct"/>
	    </xsl:when>
	    <xsl:when test="$part.autolabel != 0">
		<xsl:number from="book" count="part" format="I"/>
		<xsl:value-of select="$punct"/>
	    </xsl:when>
	</xsl:choose>
    </xsl:template>


    <xsl:template match="preface" mode="label.content">
	<xsl:param name="punct">.</xsl:param>
	<xsl:choose>
	    <xsl:when test="@label">
		<xsl:value-of select="@label"/>
		<xsl:value-of select="$punct"/>
	    </xsl:when>
	    <xsl:when test="$preface.autolabel != 0">
		<xsl:number from="book" count="preface" format="1" level="any"/>
		<xsl:value-of select="$punct"/>
	    </xsl:when>
	</xsl:choose>
    </xsl:template>

    <xsl:template match="chapter" mode="label.content">
	<xsl:param name="punct">.</xsl:param>
	<xsl:choose>
	    <xsl:when test="@label">
		<xsl:value-of select="@label"/>
		<xsl:value-of select="$punct"/>
	    </xsl:when>
	    <xsl:when test="$chapter.autolabel != 0">
		<xsl:number from="book" count="chapter" format="1" level="any"/>
		<xsl:value-of select="$punct"/>
	    </xsl:when>
	</xsl:choose>
    </xsl:template>

    <xsl:template match="appendix" mode="label.content">
	<xsl:param name="punct">.</xsl:param>
	<xsl:choose>
	    <xsl:when test="@label">
		<xsl:value-of select="@label"/>
		<xsl:value-of select="$punct"/>
	    </xsl:when>
	    <xsl:when test="$chapter.autolabel != 0">
		<xsl:number from="book" count="appendix" format="A" level="any"/>
		<xsl:value-of select="$punct"/>
	    </xsl:when>
	</xsl:choose>
    </xsl:template>

    <xsl:template match="article" mode="label.content">
	<xsl:param name="punct">.</xsl:param>
	<xsl:if test="@label">
	    <xsl:value-of select="@label"/>
	    <xsl:value-of select="$punct"/>
	</xsl:if>
    </xsl:template>


    <xsl:template match="dedication|colophon" mode="label.content">
	<xsl:param name="punct">.</xsl:param>
	<xsl:if test="@label">
	    <xsl:value-of select="@label"/>
	    <xsl:value-of select="$punct"/>
	</xsl:if>
    </xsl:template>

    <xsl:template match="reference" mode="label.content">
	<xsl:param name="punct">.</xsl:param>
	<xsl:choose>
	    <xsl:when test="@label">
		<xsl:value-of select="@label"/>
		<xsl:value-of select="$punct"/>
	    </xsl:when>
	    <xsl:when test="$part.autolabel != 0">
		<xsl:number from="book" count="reference" format="I" level="any"/>
		<xsl:value-of select="$punct"/>
	    </xsl:when>
	</xsl:choose>
    </xsl:template>

    <xsl:template match="refentry" mode="label.content">
	<xsl:param name="punct">.</xsl:param>
	<xsl:if test="@label">
	    <xsl:value-of select="@label"/>
	    <xsl:value-of select="$punct"/>
	</xsl:if>
    </xsl:template>

    <xsl:template match="section" mode="label.content">
	<xsl:param name="punct">.</xsl:param>

	<xsl:if test="local-name(..) = 'section'">
	    <xsl:apply-templates select=".." mode="label.content">
		<xsl:with-param name="punct">.</xsl:with-param>
	    </xsl:apply-templates>
	</xsl:if>

	<xsl:variable name="parent.is.component">
	    <xsl:call-template name="is.component">
		<xsl:with-param name="node" select=".."/>
	    </xsl:call-template>
	</xsl:variable>

	<xsl:variable name="label">
	    <xsl:call-template name="label.this.section">
		<xsl:with-param name="section" select="."/>
	    </xsl:call-template>
	</xsl:variable>

	<xsl:if test="$section.label.includes.component.label != 0
	    and $parent.is.component != 0">
	    <xsl:apply-templates select=".." mode="label.content">
		<xsl:with-param name="punct">.</xsl:with-param>
	    </xsl:apply-templates>
	</xsl:if>

	<xsl:choose>
	    <xsl:when test="@label">
		<xsl:value-of select="@label"/>
		<xsl:value-of select="$punct"/>
	    </xsl:when>
	    <xsl:when test="$label != 0">
		<xsl:number count="section"/>
		<xsl:value-of select="$punct"/>
	    </xsl:when>
	</xsl:choose>
    </xsl:template>

    <xsl:template match="sect1" mode="label.content">
	<xsl:param name="punct">.</xsl:param>

	<xsl:variable name="parent.is.component">
	    <xsl:call-template name="is.component">
		<xsl:with-param name="node" select=".."/>
	    </xsl:call-template>
	</xsl:variable>
	<xsl:if test="$section.label.includes.component.label != 0
	    and $parent.is.component">
	    <xsl:apply-templates select=".." mode="label.content">
		<xsl:with-param name="punct">.</xsl:with-param>
	    </xsl:apply-templates>
	</xsl:if>

	<xsl:choose>
	    <xsl:when test="@label">
		<xsl:value-of select="@label"/>
		<xsl:value-of select="$punct"/>
	    </xsl:when>
	    <xsl:when test="$section.autolabel != 0">
		<xsl:number count="sect1"/>
		<xsl:value-of select="$punct"/>
	    </xsl:when>
	</xsl:choose>
    </xsl:template>

    <xsl:template match="sect2|sect3|sect4|sect5" mode="label.content">
	<xsl:param name="punct">.</xsl:param>

	<xsl:apply-templates select=".." mode="label.content">
	    <xsl:with-param name="punct">.</xsl:with-param>
	</xsl:apply-templates>

	<xsl:choose>
	    <xsl:when test="@label">
		<xsl:value-of select="@label"/>
		<xsl:value-of select="$punct"/>
	    </xsl:when>
	    <xsl:when test="$section.autolabel != 0">
		<xsl:choose>
		    <xsl:when test="local-name(.) = 'sect2'">
			<xsl:number count="sect2"/>
		    </xsl:when>
		    <xsl:when test="local-name(.) = 'sect3'">
			<xsl:number count="sect3"/>
		    </xsl:when>
		    <xsl:when test="local-name(.) = 'sect4'">
			<xsl:number count="sect4"/>
		    </xsl:when>
		    <xsl:when test="local-name(.) = 'sect5'">
			<xsl:number count="sect5"/>
		    </xsl:when>
		    <xsl:otherwise>
			<xsl:message>label.content: this can't happen!</xsl:message>
		    </xsl:otherwise>
		</xsl:choose>
		<xsl:value-of select="$punct"/>
	    </xsl:when>
	</xsl:choose>
    </xsl:template>
    <xsl:template match="refsect1|refsect2|refsect3" mode="label.content">
	<xsl:param name="punct">.</xsl:param>
	<xsl:choose>
	    <xsl:when test="@label">
		<xsl:value-of select="@label"/>
		<xsl:value-of select="$punct"/>
	    </xsl:when>
	    <xsl:when test="$section.autolabel != 0">
		<xsl:number level="multiple" count="refsect1|refsect2|refsect3"/>
		<xsl:value-of select="$punct"/>
	    </xsl:when>
	</xsl:choose>
    </xsl:template>

    <xsl:template match="simplesect" mode="label.content">
	<xsl:param name="punct">.</xsl:param>
	<xsl:choose>
	    <xsl:when test="@label">
		<xsl:value-of select="@label"/>
		<xsl:value-of select="$punct"/>
	    </xsl:when>
	    <xsl:when test="$section.autolabel != 0">
		<xsl:number level="multiple" count="section
		    |sect1|sect2|sect3|sect4|sect5
		    |refsect1|refsect2|refsect3
		    |simplesect"/>
		<xsl:value-of select="$punct"/>
	    </xsl:when>
	</xsl:choose>
    </xsl:template>

    <xsl:template match="qandadiv" mode="label.content">
	<xsl:param name="punct">.</xsl:param>
	<xsl:variable name="prefix">
	    <xsl:if test="$qanda.inherit.numeration != 0">
		<xsl:variable name="lparent" select="(ancestor::set
		    |ancestor::book
		    |ancestor::chapter
		    |ancestor::appendix
		    |ancestor::preface
		    |ancestor::section
		    |ancestor::simplesect
		    |ancestor::sect1
		    |ancestor::sect2
		    |ancestor::sect3
		    |ancestor::sect4
		    |ancestor::sect5
		    |ancestor::refsect1
		    |ancestor::refsect2
		    |ancestor::refsect3)[last()]"/>
		<xsl:if test="count($lparent)>0">
		    <xsl:apply-templates select="$lparent" mode="label.content"/>
		</xsl:if>
	    </xsl:if>
	</xsl:variable>
	<xsl:choose>
	    <xsl:when test="@label">
		<xsl:value-of select="$prefix"/>
		<xsl:value-of select="@label"/>
		<xsl:value-of select="$punct"/>
	    </xsl:when>
	    <xsl:when test="$qandadiv.autolabel != 0">
		<xsl:value-of select="$prefix"/>
		<xsl:number level="multiple" count="qandadiv" format="1"/>
		<xsl:value-of select="$punct"/>
	    </xsl:when>
	</xsl:choose>
    </xsl:template>

    <xsl:template match="question|answer" mode="label.content">
	<xsl:param name="punct">.</xsl:param>
	<xsl:variable name="prefix">
	    <xsl:if test="$qanda.inherit.numeration != 0">
		<xsl:variable name="lparent" select="(ancestor::set
		    |ancestor::book
		    |ancestor::chapter
		    |ancestor::appendix
		    |ancestor::preface
		    |ancestor::section
		    |ancestor::simplesect
		    |ancestor::sect1
		    |ancestor::sect2
		    |ancestor::sect3
		    |ancestor::sect4
		    |ancestor::sect5
		    |ancestor::refsect1
		    |ancestor::refsect2
		    |ancestor::refsect3
		    |ancestor::qandadiv)[last()]"/>
		<xsl:if test="count($lparent)>0">
		    <xsl:apply-templates select="$lparent" mode="label.content"/>
		</xsl:if>
	    </xsl:if>
	</xsl:variable>

	<xsl:variable name="inhlabel"
	    select="ancestor-or-self::qandaset/@defaultlabel[1]"/>

	<xsl:variable name="deflabel">
	    <xsl:choose>
		<xsl:when test="$inhlabel != ''">
		    <xsl:value-of select="$inhlabel"/>
		</xsl:when>
		<xsl:otherwise>
		    <xsl:value-of select="$qanda.defaultlabel"/>
		</xsl:otherwise>
	    </xsl:choose>
	</xsl:variable>

	<xsl:variable name="label" select="label"/>

	<xsl:choose>
	    <xsl:when test="count($label)>0">
		<xsl:value-of select="$prefix"/>
		<xsl:apply-templates select="$label"/>
		<xsl:value-of select="$punct"/>
	    </xsl:when>

	    <xsl:when test="$deflabel = 'qanda'">
		<xsl:call-template name="gentext.element.name"/>
	    </xsl:when>

	    <xsl:when test="$deflabel = 'number'">
		<xsl:if test="name(.) = 'question'">
		    <xsl:value-of select="$prefix"/>
		    <xsl:number level="multiple" count="qandaentry" format="1"/>
		    <xsl:value-of select="$punct"/>
		</xsl:if>
	    </xsl:when>
	</xsl:choose>
    </xsl:template>

    <xsl:template match="bibliography|glossary|index" mode="label.content">
	<xsl:param name="punct">.</xsl:param>
	<xsl:if test="@label">
	    <xsl:value-of select="@label"/>
	    <xsl:value-of select="$punct"/>
	</xsl:if>
    </xsl:template>

    <xsl:template match="figure|table|example|equation" mode="label.content">
	<xsl:param name="punct">.</xsl:param>
	<xsl:choose>
	    <xsl:when test="@label">
		<xsl:value-of select="@label"/>
		<xsl:value-of select="$punct"/>
	    </xsl:when>
	    <xsl:otherwise>
		<xsl:variable name="pchap"
		    select="ancestor::chapter|ancestor::appendix"/>
		<xsl:choose>
		    <xsl:when test="count($pchap)>0">
			<xsl:apply-templates select="$pchap" mode="label.content">
			    <xsl:with-param name="punct">.</xsl:with-param>
			</xsl:apply-templates>
			<xsl:number format="1" from="chapter|appendix" level="any"/>
			<xsl:value-of select="$punct"/>
		    </xsl:when>
		    <xsl:otherwise>
			<xsl:number format="1" from="book|article" level="any"/>
			<xsl:value-of select="$punct"/>
		    </xsl:otherwise>
		</xsl:choose>
	    </xsl:otherwise>
	</xsl:choose>
    </xsl:template>

    <xsl:template match="abstract" mode="label.content">
	<xsl:param name="punct">.</xsl:param>
    </xsl:template>
    --> 

</xsl:stylesheet>
