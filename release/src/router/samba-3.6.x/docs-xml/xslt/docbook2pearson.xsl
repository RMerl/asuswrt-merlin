<?xml version='1.0'?>
<!-- 
	Convert DocBook to XML validating against the Pearson DTD

	(C) Jochen Hein
	(C) Jelmer Vernooij <jelmer@samba.org>			2004
-->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc"
	version="1.1">

	<!-- TODO <entry></entry> -> <entry><p/></entry> -->

	<xsl:output method="xml"
		encoding="UTF-8"
		doctype-public="-//Pearson//DTD Books//DE"
		doctype-system="http://www.pearson.de/pearson.dtd"
		indent="yes"
		omit-xml-declaration="no"
		standalone="no"/>

	<!-- Warn about DocBook elements that are not converted -->
	<xsl:template match="*">
		<xsl:message>
			<xsl:text>No template matches </xsl:text>
			<xsl:value-of select="name(.)"/>
			<xsl:text>.</xsl:text>
		</xsl:message>
	</xsl:template>

	<!--  
	====================================================================  
	-->
	<!-- Establish strip/preserve whitespace rules -->

	<xsl:preserve-space elements="*"/>

	<xsl:strip-space elements="
		abstract affiliation anchor answer appendix area areaset areaspec
		artheader article audiodata audioobject author authorblurb authorgroup
		beginpage bibliodiv biblioentry bibliography biblioset blockquote book
		bookbiblio bookinfo callout calloutlist caption caution
		city pubdate publisher publishername"/>

	<xsl:template name="transform.id.attribute">
		<xsl:if test="@id != ''">
			<xsl:attribute name="id">
				<xsl:value-of select="@id"/>
			</xsl:attribute>
		</xsl:if>
	</xsl:template>

	<xsl:template match="/" mode="default">
		<xsl:processing-instruction  
			name="xml-stylesheet">href="pearson.css"  
			type="text/css"</xsl:processing-instruction>
		<xsl:text>
		</xsl:text>
		<xsl:comment>
			<xsl:text>Transformed with pearson.xsl by Jochen Hein</xsl:text>
		</xsl:comment>
		<xsl:text>
		</xsl:text>
		<!-- Releaseinfo einbauen? -->
		<xsl:comment>
			<xsl:apply-templates select=".//releaseinfo" mode="comment"/>
		</xsl:comment>
		<xsl:text>
		</xsl:text>
		<xsl:apply-templates/>
	</xsl:template>

	<xsl:template match="book">
		<book>
			<xsl:apply-templates/>
		</book>
	</xsl:template>

	<xsl:template match="bookinfo">
		<bookinfo>
			<xsl:apply-templates/>
		</bookinfo>
	</xsl:template>

	<xsl:template match="author">
		<author>
			<xsl:apply-templates/>
		</author>
	</xsl:template>

	<xsl:template match="editor">
		<editor>
			<xsl:apply-templates/>
		</editor>
	</xsl:template>

	<xsl:template match="chapter">
		<chapter>
			<xsl:call-template name="transform.id.attribute"/>
			<xsl:apply-templates/>
		</chapter>
	</xsl:template>

	<xsl:template match="chapter/title/command">
		<xsl:apply-templates/>
	</xsl:template>

	<xsl:template match="index">
       <xsl:comment> XXX insert index here </xsl:comment>
   </xsl:template>

   <xsl:template match="preface">
	   <preface>
		   <xsl:call-template name="transform.id.attribute"/>
		   <xsl:apply-templates/>
	   </preface>
   </xsl:template>

   <xsl:template match="section|sect1|sect2|sect3|sect4">
	   <sect>
		   <xsl:call-template name="transform.id.attribute"/>
		   <xsl:apply-templates/>
	   </sect>
   </xsl:template>

   <xsl:template match="partintro">
	   <xsl:copy>
		   <xsl:apply-templates select="@*|node()"/>
	   </xsl:copy>
   </xsl:template>

   <xsl:template match="part">
	   <appendix>
		   <xsl:apply-templates/>
	   </appendix>
   </xsl:template>

   <xsl:template match="appendix[1]">
	   <appendix>
		   <chapter>
			   <xsl:call-template name="transform.id.attribute"/>
			   <xsl:apply-templates/>
		   </chapter>
		   <xsl:for-each select="following-sibling::appendix">
			   <chapter>
				   <xsl:call-template name="transform.id.attribute"/>
				   <xsl:apply-templates/>
			   </chapter>
		   </xsl:for-each>
	   </appendix>
   </xsl:template>

   <xsl:template match="appendix[position() != 1]">
   </xsl:template>

   <xsl:template match="title">
	   <title><xsl:apply-templates/></title>
   </xsl:template>

	<xsl:param name="notpchilds" select="'variablelist itemizedlist orderedlist'"/>

	<xsl:template match="para"> 
		<!-- loop thru all elements: -->
		<xsl:for-each select="*|text()">
			<xsl:choose>
				<xsl:when test="string-length(name(.)) > 0 and contains($notpchilds,name(.))">
					<xsl:message><xsl:text>Removing from p:</xsl:text><xsl:value-of select="name(.)"/></xsl:message>
					<xsl:apply-templates select="."/>
				</xsl:when>
				<xsl:otherwise>
					<xsl:call-template name="hstartpara"/>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:for-each>
	</xsl:template>

	<!-- Do open paragraph when previous sibling was not a valid p child -->
	<xsl:template name="hstartpara">
		<xsl:if test="not(preceding-sibling::*[1]) or (contains($notpchilds,name(preceding-sibling::*[1])) and not(string-length(name(.)) > 0))">
			<p>
				<xsl:apply-templates select="."/>
				<xsl:for-each select="following-sibling::*[not(contains($notpchilds,name(.)) and string-length(name(.)) > 0)]">
					<xsl:apply-templates select="."/>
				</xsl:for-each>
			</p>
		</xsl:if>
	</xsl:template>

	<xsl:template name="hsubpara">
		<xsl:param name="data"/>
		<xsl:apply-templates select="$data"/>
		<xsl:if test="$data[following-sibling::*[1]]">
			<xsl:param name="next" select="$data[following-sibling::*[1]]"/>
			<xsl:message><xsl:value-of select="name(.)"/></xsl:message>
			<xsl:if test="$next and (not(contains($notpchilds,name($next))) or $next[text()])">
				<xsl:message><xsl:text>Followed by : </xsl:text><xsl:value-of select="name($next)"/></xsl:message>
				<xsl:call-template name="hsubpara">
					<xsl:with-param name="data" select="$next"/>
				</xsl:call-template>
				<xsl:message><xsl:text>Done Followed by : </xsl:text><xsl:value-of select="name($next)"/></xsl:message>
			</xsl:if>
		</xsl:if>
	</xsl:template>

	<xsl:template match="tip">
		<tip><xsl:apply-templates/></tip>
	</xsl:template>

   <xsl:template match="warning|important|caution">
	   <stop><xsl:apply-templates/></stop>
   </xsl:template>

   <xsl:template match="note">
	   <info><xsl:apply-templates/></info>
   </xsl:template>

   <xsl:template match="emphasis">
	   <xsl:variable name="role" select="@role"/>

	   <xsl:choose>
		   <xsl:when test="$role='italic'">
			   <i><xsl:apply-templates/></i>
		   </xsl:when>
		   <xsl:otherwise>
			   <em><xsl:apply-templates/></em>
		   </xsl:otherwise>
	   </xsl:choose>
   </xsl:template>

   <xsl:template match="ulink">
	   <xsl:apply-templates/>
	   <footnote><p><url><xsl:value-of select="@ulink"/></url></p></footnote>
	</xsl:template>

   <xsl:template match="email">
	   <url>mailto:<xsl:apply-templates/></url>
   </xsl:template>

   <xsl:template match="indexterm">
	   <ie><xsl:apply-templates/></ie>
   </xsl:template>

   <xsl:template match="primary">
	   <t><xsl:apply-templates/></t>
   </xsl:template>

   <xsl:template match="secondary">
	   <t><xsl:apply-templates/></t>
   </xsl:template>

   <xsl:template match="citerefentry">
	   <xsl:value-of select="refentrytitle"/><xsl:text>(</xsl:text><xsl:value-of select="manvolnum"/><xsl:text>)</xsl:text>
   </xsl:template>

   <xsl:template match="formalpara">
	   <xsl:apply-templates/>
   </xsl:template>

   <xsl:template match="substeps">
	   <xsl:apply-templates/>
   </xsl:template>

   <xsl:template match="see|seealso">
	   <synonym><xsl:apply-templates/></synonym>
   </xsl:template>

   <xsl:template  
	   match="footnote"><footnote><xsl:apply-templates/></footnote></xsl:template>

   <xsl:template match="command">
	   <code><xsl:apply-templates/></code>
   </xsl:template>

   <xsl:template match="function">
	   <code><xsl:apply-templates/></code>
   </xsl:template>

   <xsl:template match="systemitem">

	   <!-- xsl:if test="@role != '' and @role != 'signal'">
	   <xsl:message>
		   Warning: role attribute is <xsl:value-of select="@role"/>
	   </xsl:message>
   </xsl:if -->
   <code><xsl:apply-templates/></code>
   </xsl:template>

   <xsl:template match="sgmltag">
	   <code><xsl:apply-templates/></code>
   </xsl:template>

   <xsl:template match="filename">
	   <code><xsl:apply-templates/></code>
   </xsl:template>

   <xsl:template match="errorcode">
	   <code><xsl:apply-templates/></code>
   </xsl:template>

   <xsl:template match="errortype">
	   <code><xsl:apply-templates/></code>
   </xsl:template>

   <xsl:template match="returnvalue">
	   <code><xsl:apply-templates/></code>
   </xsl:template>

   <xsl:template match="glosslist|glossary">
	   <dl><xsl:apply-templates/></dl>
   </xsl:template>

   <xsl:template match="glossentry">
	   <dlitem><xsl:apply-templates/></dlitem>
   </xsl:template>

   <xsl:template match="glossentry/acronym">
	   <dt><xsl:apply-templates/></dt>
   </xsl:template>

   <xsl:template match="acronym">
	   <b><xsl:apply-templates/></b>
   </xsl:template>

   <xsl:template match="glossterm">
	   <dt><xsl:apply-templates/></dt>
   </xsl:template>

   <xsl:template match="glossdef">
	   <dd><xsl:apply-templates/></dd>
   </xsl:template>

   <xsl:template match="itemizedlist">
	   <ul><xsl:apply-templates/></ul>
   </xsl:template>

   <xsl:template match="varlistentry/term">
	   <dt><xsl:apply-templates/></dt>
   </xsl:template>

   <xsl:template match="varlistentry/listitem">
	   <dd><xsl:apply-templates/></dd>
   </xsl:template>

   <xsl:template match="varlistentry">
	   <dlitem>
		   	<xsl:apply-templates/>
	   </dlitem>
   </xsl:template>

   <xsl:template match="variablelist">
	   <dl><xsl:apply-templates/></dl>
   </xsl:template>

   <xsl:template match="simplelist">
	   <ul>
		   <xsl:for-each select="member">
			   <li><p><xsl:apply-templates/></p></li>
		   </xsl:for-each>
	   </ul>
   </xsl:template>

   <xsl:template match="orderedlist">
	   <xsl:variable name="numeration" select="@numeration"/>
	   <ol>
		   <xsl:if test="$numeration != ''">
			   <xsl:attribute name="type">
				   <xsl:value-of select="$numeration"/>
			   </xsl:attribute>
		   </xsl:if>
		   <xsl:apply-templates/>
	   </ol>
   </xsl:template>

   <xsl:template match="listitem">
	   <li><xsl:apply-templates/></li>
   </xsl:template>

   <xsl:template match="firstterm">
	   <em><xsl:apply-templates/></em>
   </xsl:template>

   <xsl:template match="literal/systemitem">
	   <em><xsl:apply-templates/></em>
   </xsl:template>

   <xsl:template match="superscript">
	   <sup><xsl:apply-templates/></sup>
   </xsl:template>

   <xsl:template match="productname">
	   <xsl:apply-templates/>
   </xsl:template>

   <xsl:template match="keycombo">

	   <xsl:variable name="action" select="@action"/>
	   <xsl:variable name="joinchar">

		   <xsl:choose>

			   <xsl:when test="$action='seq'">
				   <xsl:text> </xsl:text></xsl:when>
			   <xsl:when test="$action='simul'">+</xsl:when>
			   <xsl:when test="$action='press'">-</xsl:when>
			   <xsl:when test="$action='click'">-</xsl:when>
			   <xsl:when test="$action='double-click'">-</xsl:when>
			   <xsl:when test="$action='other'"></xsl:when>
			   <xsl:otherwise>-</xsl:otherwise>
		   </xsl:choose>
	   </xsl:variable>

	   <xsl:for-each select="*">
		   <xsl:if test="position()>1"><xsl:value-of  
				   select="$joinchar"/></xsl:if>
		   <xsl:apply-templates select="."/>
	   </xsl:for-each>

   </xsl:template>


   <xsl:template match="keycap">
	   <key><xsl:apply-templates/></key>
   </xsl:template>

   <xsl:template match="keycap/replaceable">
	   <xsl:apply-templates/>
   </xsl:template>



   <xsl:template match="menuchoice">
	   <xsl:variable name="shortcut" select="./shortcut"/>
	   <xsl:call-template name="process.menuchoice"/>
	   <xsl:if test="$shortcut">
		   <xsl:text> (</xsl:text>
		   <xsl:apply-templates select="$shortcut"/>
		   <xsl:text>)</xsl:text>
	   </xsl:if>
   </xsl:template>

   <xsl:template name="process.menuchoice">
	   <xsl:param name="nodelist"  
		   select="guibutton|guiicon|guilabel|guimenu|guimenuitem|guisubmenu|interface"/><!-- not(shortcut) -->
	   <xsl:param name="count" select="1"/>

	   <xsl:choose>
		   <xsl:when test="$count>count($nodelist)"></xsl:when>
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
				   <xsl:when test="name($node)='guimenuitem'
					   or name($node)='guisubmenu'">
					   <xsl:text>-&gt;</xsl:text>
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


   <xsl:template match="guimenu">
	   <guiitem><xsl:apply-templates/></guiitem>
   </xsl:template>

   <xsl:template match="guimenuitem">
	   <guiitem><xsl:apply-templates/></guiitem>
   </xsl:template>

	<xsl:template match="guiicon">
	   <guiitem><xsl:apply-templates/></guiitem>
   </xsl:template>

   <xsl:template match="guilabel">
	   <guiitem><xsl:apply-templates/></guiitem>
   </xsl:template>

   <xsl:template match="guibutton">
	   <guiitem><xsl:apply-templates/></guiitem>
   </xsl:template>

   <xsl:template name="transform.epigraph">
	   <xsl:apply-templates select="para"/>
	   <xsl:apply-templates select="attribution"/>
   </xsl:template>

   <xsl:template match="epigraph">
	   <chapterintro><motto><xsl:call-template  
				   name="transform.epigraph"/></motto></chapterintro>
   </xsl:template>

   <xsl:template match="section/epigraph">
	   <motto><xsl:call-template name="transform.epigraph"/></motto>
   </xsl:template>

   <xsl:template match="epigraph/para">
	   <p><em><xsl:apply-templates/></em></p>
   </xsl:template>
   <xsl:template match="attribution">
	   <p><i><xsl:apply-templates/></i></p>
   </xsl:template>

   <xsl:template match="errorname">
	   <i><xsl:apply-templates/></i>
   </xsl:template>

   <xsl:template match="constant">
	   <b><xsl:apply-templates/></b>
   </xsl:template>

   <xsl:template match="envar">
	   <code><xsl:apply-templates/></code>
   </xsl:template>

   <xsl:template match="literal">
	   <code><xsl:apply-templates/></code>
   </xsl:template>

   <xsl:template match="option">
	   <code><xsl:apply-templates/></code>
   </xsl:template>

   <xsl:template match="parameter">
	   <code><xsl:apply-templates/></code>
   </xsl:template>

   <xsl:template match="application">
	   <b><xsl:apply-templates/></b>
   </xsl:template>

   <xsl:template match="screen/userinput">
	   <b><xsl:apply-templates/></b>
   </xsl:template>

   <xsl:template match="screen/prompt">
	   <xsl:apply-templates/>
   </xsl:template>

   <xsl:template match="prompt">
	   <code><xsl:apply-templates/></code>
   </xsl:template>

   <xsl:template match="userinput">
	   <code><b><xsl:apply-templates/></b></code>
   </xsl:template>

   <xsl:template match="computeroutput">
	   <code><xsl:apply-templates/></code>
   </xsl:template>

   <xsl:template match="screen/computeroutput">
	   <xsl:apply-templates/>
   </xsl:template>

   <xsl:template match="replaceable">
	   <i><xsl:apply-templates/></i>
   </xsl:template>

   <xsl:template match="userinput/replaceable">
	   <i><xsl:apply-templates/></i>
   </xsl:template>

   <xsl:template match="citation"
	   xmlns:xlink="http://www.w3.org/1999/xlink">

	   <xsl:variable name="target" select="."/>
	   <xsl:variable name="targets" select="id($target)"/>

	   <!-- xsl:call-template name="check.id.unique">
	   <xsl:with-param name="linkend" select="$target"/>
   </xsl:call-template -->

   <xsl:choose>
	   <xsl:when test="count($targets) = 0">
		   <xsl:message>
			   <xsl:text>Citation of nonexistent id: </xsl:text>
			   <xsl:value-of select="."/>
		   </xsl:message>
		   <xsl:text>[??? </xsl:text>
		   <xsl:value-of select="."/>
		   <xsl:text>]</xsl:text>
	   </xsl:when>
	   <xsl:otherwise>
		   <xsl:text>[</xsl:text><xsl:value-of  
			   select="."/><xsl:text>]</xsl:text>
	   </xsl:otherwise>
   </xsl:choose>

   </xsl:template>

   <xsl:template match="figure">
	   <figure>
		   <xsl:call-template name="transform.id.attribute"/>
		   <xsl:apply-templates/>
	   </figure>
   </xsl:template>

   <xsl:template match="graphic">
	   <xsl:message>
		   <xsl:text>Obsolete tag "graphic" found.</xsl:text>
	   </xsl:message>

	   <figureref>
		   <xsl:attribute name="fileref">
			   <xsl:value-of select="@fileref"/>
		   </xsl:attribute>
	   </figureref>
   </xsl:template>

   <xsl:template match="mediaobject">
	   <xsl:apply-templates/>
   </xsl:template>

   <xsl:template match="imageobject">
	   <xsl:apply-templates/>
   </xsl:template>

   <xsl:template match="imagedata">
	   <xsl:variable name="scale" select="@scale"/>
	   <figureref>
		   <xsl:attribute name="fileref">
			   <xsl:value-of select="@fileref"/>
		   </xsl:attribute>
		   <xsl:if test="$scale != ''">
			   <xsl:attribute name="scale"><xsl:value-of  
					   select="$scale"/>%</xsl:attribute>
		   </xsl:if>
	   </figureref>
   </xsl:template>

   <xsl:template match="example">
	   <screen>
		   <xsl:call-template name="transform.id.attribute"/>
		   <xsl:apply-templates/>
	   </screen>
   </xsl:template>

   <xsl:template match="listing">
	   <screen>
		   <xsl:call-template name="transform.id.attribute"/>
		   <xsl:apply-templates/>    </screen>
   </xsl:template>

   <xsl:template name="check.title.not.empty">
	   <xsl:variable name="title" select="."/>
	   <xsl:if test="$title=''">
		   <xsl:message>title tag is empty!</xsl:message>
	   </xsl:if>
   </xsl:template>

   <xsl:template match="figure/title">
	   <xsl:call-template name="check.title.not.empty"/>
	   <description><xsl:apply-templates/></description>
   </xsl:template>
   <xsl:template match="listing/title">
	   <xsl:call-template name="check.title.not.empty"/>
	   <description><xsl:apply-templates/></description>
   </xsl:template>
   <xsl:template match="example/title">
	   <xsl:call-template name="check.title.not.empty"/>
	   <description><xsl:apply-templates/></description>
   </xsl:template>

   <xsl:template match="literallayout">
	   <screentext><xsl:apply-templates/></screentext>
   </xsl:template>
   <xsl:template match="screen">
	   <screentext><xsl:apply-templates/></screentext>
   </xsl:template>

   <xsl:template match="programlisting">
	   <listing>
		   <xsl:if test="title != ''">
			   <description><xsl:value-of select="title"/></description>
		   </xsl:if>
		   <listingcode>
			   <xsl:apply-templates/>
		   </listingcode>
		</listing>
   </xsl:template>

   <!-- Tabellen -->
   <xsl:template match="table">
	   <table>
		   <xsl:call-template name="transform.id.attribute"/>
		   <xsl:apply-templates/>
	   </table>
   </xsl:template>
   <xsl:template match="table/title">
	   <description><xsl:apply-templates/></description>
   </xsl:template>

   <xsl:template match="tgroup">
	   <tgroup>
		   <xsl:attribute name="cols">
			   <xsl:value-of select="@cols"/></xsl:attribute>
		   <xsl:apply-templates/>
	   </tgroup>
   </xsl:template>
   <xsl:template match="thead">
	   <thead><xsl:apply-templates/></thead>
   </xsl:template>
   <xsl:template match="tbody">
	   <tbody><xsl:apply-templates/></tbody>
   </xsl:template>
   <xsl:template match="row">
	   <row><xsl:apply-templates/></row>
   </xsl:template>
   <xsl:template match="entry">
	   <entry><p><xsl:apply-templates/></p></entry>
   </xsl:template>
   <xsl:template match="colspec">
	   <colspec><xsl:copy-of select="@*"/><xsl:apply-templates/></colspec>
   </xsl:template>

   <!-- xref nach pearson-equivalent - Einige Ideen stammen von Norm -->

   <xsl:template name="object.id">
	   <xsl:param name="object" select="."/>
	   <xsl:choose>
		   <xsl:when test="$object/@id">
			   <xsl:value-of select="$object/@id"/>
		   </xsl:when>
		   <xsl:otherwise>
			   <xsl:value-of select="generate-id($object)"/>
		   </xsl:otherwise>
	   </xsl:choose>
   </xsl:template>

   <xsl:template name="anchor">
	   <xsl:param name="node" select="."/>
	   <xsl:param name="conditional" select="1"/>
	   <xsl:variable name="id">
		   <xsl:call-template name="object.id">
			   <xsl:with-param name="object" select="$node"/>
		   </xsl:call-template>
	   </xsl:variable>
	   <xsl:if test="$conditional = 0 or $node/@id">
		   <a name="{$id}"/>
	   </xsl:if>
   </xsl:template>

   <xsl:template name="check.id.unique">
	   <xsl:param name="linkend"></xsl:param>
	   <xsl:if test="$linkend != ''">
		   <xsl:variable name="targets" select="id($linkend)"/>
		   <xsl:variable name="target" select="$targets[1]"/>

		   <xsl:if test="count($targets)=0">
			   <xsl:message>
				   <xsl:text>Error: no ID for constraint linkend: </xsl:text>
				   <xsl:value-of select="$linkend"/>
				   <xsl:text>.</xsl:text>
			   </xsl:message>
		   </xsl:if>

		   <xsl:if test="count($targets)>1">
			   <xsl:message>
				   <xsl:text>Warning: multiple "IDs" for constraint linkend:  
				   </xsl:text>
				   <xsl:value-of select="$linkend"/>
				   <xsl:text>.</xsl:text>
			   </xsl:message>
		   </xsl:if>
	   </xsl:if>
   </xsl:template>

   <xsl:template match="xref|link"
	   xmlns:xlink="http://www.w3.org/1999/xlink">
	   <xsl:variable name="linkend" select="@linkend"/>
	   <xsl:variable name="targets" select="id(@linkend)"/>
	   <xsl:variable name="target" select="$targets[1]"/>
	   <xsl:variable name="refelem" select="local-name($target)"/>

	   <xsl:call-template name="check.id.unique">
		   <xsl:with-param name="linkend" select="$linkend"/>
	   </xsl:call-template>

	   <xsl:call-template name="anchor"/>

	   <xsl:choose>
		   <xsl:when test="count($targets) = 0">
			   <xsl:message>
				   <xsl:text>XRef to nonexistent id: </xsl:text>
				   <xsl:value-of select="@linkend"/>
			   </xsl:message>
			   <xsl:text>[??? </xsl:text>
			   <xsl:value-of select="@linkend"/>
			   <xsl:text>]</xsl:text>
		   </xsl:when>
		   <xsl:when test="count($targets) > 1">
			   <xsl:message>Uh, multiple linkends
				   <xsl:value-of select="@linkend"/>
			   </xsl:message>
		   </xsl:when>
	   </xsl:choose>

	   <xsl:choose>
		   <xsl:when test='$refelem = "chapter" or $refelem = "preface"'>
			   <xsl:text>Kapitel </xsl:text>
		   </xsl:when>
		   <xsl:when test='$refelem = "section" or $refelem = "sect1" or $refelem = "sect2" or $refelem = "sect3" or $refelem = "sect4"'>
			   <xsl:text>Abschnitt </xsl:text>
		   </xsl:when>
		   <xsl:when test='$refelem = "figure"'>
			   <xsl:text>Abbildung </xsl:text>
		   </xsl:when>
		   <xsl:when test='$refelem = "example"'>
			   <!-- xsl:choose>
			   <xsl:when test="$target/@role='listing'">
				   <xsl:text>Listing </xsl:text>
			   </xsl:when>
			   <xsl:otherwise -->
				   <xsl:text>Listing </xsl:text>
				   <!-- /xsl:otherwise>
			   </xsl:choose -->
		   </xsl:when>
		   <xsl:when test='$refelem = "table"'>
			   <xsl:text>Tabelle </xsl:text>
		   </xsl:when>
		   <xsl:when test='$refelem = "appendix"'>
			   <xsl:text>Anhang </xsl:text>
		   </xsl:when>
		   <xsl:otherwise>
			   <xsl:if test="$refelem != ''">
				   <xsl:message>
					   <xsl:text>Cant't handle xref to </xsl:text>
					   <xsl:value-of select="$refelem"/>
					   <xsl:text>:</xsl:text>
					   <xsl:value-of select="@linkend"/>
				   </xsl:message>
				   <xsl:text>(??? $refelem)</xsl:text>
			   </xsl:if>
		   </xsl:otherwise>
	   </xsl:choose>

	   <xref>
		   <xsl:attribute name="xlink:href"
			   xmlns:xlink="http://www.w3.org/1999/xlink">
			   <!-- xsl:attribute name="id" -->
			   <xsl:value-of select="@linkend"/>
		   </xsl:attribute>
	   </xref>
   </xsl:template>

   <xsl:template match="quote">"<xsl:apply-templates/>"</xsl:template>
   <!-- xsl:template  
   match="quote"><quote><xsl:apply-templates/></quote></xsl:template -->


   <xsl:template match="othername">
	   <xsl:text>(</xsl:text><xsl:apply-templates/><xsl:text>)</xsl:text>0
   </xsl:template>
   <xsl:template match="firstname">
	   <xsl:apply-templates/><xsl:text> </xsl:text>
   </xsl:template>
   <xsl:template match="surname">
	   <xsl:apply-templates/>
   </xsl:template>
   <xsl:template match="copyright">
	   <!-- empty -->
   </xsl:template>
   <xsl:template match="edition">
	   <!-- empty -->
   </xsl:template>
   <xsl:template match="publisher">
	   <!-- empty -->
   </xsl:template>
   <xsl:template match="pubdate">
	   <xsl:apply-templates/>
   </xsl:template>
   <xsl:template match="isbn">
	   ISBN <xsl:apply-templates/>
   </xsl:template>

   <!-- Literaturverzeichnis -->
   <xsl:template match="bibliography">
	   <sect>
		   <xsl:apply-templates/>
	   </sect>
   </xsl:template>

   <xsl:template name="process.bibliomixed">

	   <xsl:variable name="bibid" select="id(@id)"/>
	   <dlitem><dt>[<xsl:value-of select="@id"/>]</dt>
		   <dd><p><xsl:apply-templates select=".//title"/>,
				   <xsl:if test="count(.//subtitle)>0">
					   <xsl:apply-templates select=".//subtitle"/>,
				   </xsl:if>
				   <xsl:apply-templates select=".//author"/>
				   <xsl:apply-templates select=".//publisher"/>
				   <xsl:apply-templates select=".//pubdate"/>
				   <xsl:if test="count(.//isbn)>0">
					   <xsl:text>, </xsl:text>
					   <xsl:apply-templates select=".//isbn"/>
				   </xsl:if>
				   <xsl:text>.</xsl:text></p>
			   <xsl:call-template name="process.abstract" select=".//abstract"/>
		   </dd>
	   </dlitem>
   </xsl:template>

   <xsl:template match="bibliomixed[1]">
	   <dl><xsl:call-template name="process.bibliomixed"/>
		   <xsl:for-each select="following-sibling::bibliomixed">
			   <xsl:call-template name="process.bibliomixed"/>
		   </xsl:for-each>
	   </dl>
   </xsl:template>


   <xsl:template match="bibliomixed/title">
	   <i><xsl:apply-templates/></i>
   </xsl:template>

   <xsl:template match="biblioentry/title">
	   <i><xsl:apply-templates/></i>
   </xsl:template>

   <xsl:template match="bibliomixed/subtitle">
	   <xsl:apply-templates/>
   </xsl:template>

   <xsl:template match="bibliomixed/publisher">
	   <xsl:apply-templates/>
   </xsl:template>

   <xsl:template match="publishername">
	   <xsl:apply-templates/>,
   </xsl:template>
   <xsl:template match="publisher/address">
	   <xsl:apply-templates/>
   </xsl:template>
   <xsl:template match="city">
	   <xsl:apply-templates/>,
   </xsl:template>

   <xsl:template name="process.abstract">
	   <xsl:apply-templates select="abstract//para"/>
   </xsl:template>
   <xsl:template match="bibliomixed">
	   <!-- empty -->
   </xsl:template>

   <xsl:template match="chapterinfo">
	   <!-- empty -->
   </xsl:template>

   <xsl:template match="sectioninfo">
	   <!-- empty -->
   </xsl:template>

   <xsl:template match="prefaceinfo">
	   <!-- empty -->
   </xsl:template>

   <xsl:template match="appendixinfo">
	   <!-- empty -->
   </xsl:template>

   <xsl:template match="releaseinfo">
	   <xsl:comment><xsl:apply-templates/></xsl:comment>
   </xsl:template>

   <xsl:template match="releaseinfo" mode="comment">
	   <xsl:text>
	   </xsl:text>
	   <xsl:apply-templates/>
   </xsl:template>

   <xsl:template match="trademark">
	   <xsl:apply-templates/>
   </xsl:template>

   <xsl:template match="processing-instruction('linebreak')">
	   <br/>
   </xsl:template>

   <xsl:template match="processing-instruction('pearson')">
	   <xsl:copy/>
   </xsl:template>

   <xsl:template match="procedure">
		   <xsl:apply-templates/>
   </xsl:template>

   <xsl:template match="step">
	   <step><xsl:apply-templates/></step>
   </xsl:template>

   <xsl:template match="toc">
	   <xsl:element name="toc">
		   <xsl:attribute name="role">
			   <xsl:text>sect</xsl:text>
		   </xsl:attribute>
	   </xsl:element>
   </xsl:template>

   <xsl:template match="lot">
	   <xsl:element name="toc">
		   <xsl:attribute name="role">
			   <xsl:text>table</xsl:text>
		   </xsl:attribute>
	   </xsl:element>
   </xsl:template>

   <xsl:template match="lof">
	   <xsl:element name="toc">
		   <xsl:attribute name="role">
			   <xsl:text>figure</xsl:text>
		   </xsl:attribute>
	   </xsl:element>
   </xsl:template>

   <xsl:template match="blockquote">
	   <motto><xsl:apply-templates/></motto>
   </xsl:template>

	<xsl:template match="authorgroup">
		<xsl:apply-templates/>
	</xsl:template>


	<xsl:template match="affiliation">
		<!-- Ignoring affiliations for now -->
	</xsl:template>

</xsl:stylesheet>
