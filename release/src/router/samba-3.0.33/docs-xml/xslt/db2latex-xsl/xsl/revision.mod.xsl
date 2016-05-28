<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: revision.mod.xsl,v 1.7 2004/01/03 09:48:34 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
+ ############################################################################## -->

<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

    <doc:reference id="revision" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: revision.mod.xsl,v 1.7 2004/01/03 09:48:34 j-devenish Exp $
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
		<title>Revision Management <filename>revision.mod.xsl</filename></title>
		<partintro>
			<para>

				This file defines the &DB2LaTeX; XSL templates for
				<doc:db>revision</doc:db> and its children. The basic mapping
				is to output a &LaTeX; table and a table row for each revision.

			</para>
		</partintro>
	</doc:reference>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>revhistory</doc:db> elements</refpurpose>
		<doc:description>
			<para>
			
			Format a list of <doc:db basename="revision">revisions</doc:db>
			as a block.
			
			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.latex.output.revhistory"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:notes>
			<para>
				Uses a &LaTeX; mapping and applies templates.
			</para>
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_article;
				&test_book;
				&test_ddh;
				&test_ieeebiblio;
				&test_mapping;
				&test_minimal;
			</simplelist>
		</doc:samples>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara>&mapping;</simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="revhistory">
	<!--
	<xsl:message>RCAS: Processing Revision History </xsl:message>
	-->
		<xsl:if test="$latex.output.revhistory=1">
			<xsl:call-template name="map.begin"/>
			<xsl:apply-templates/>
			<xsl:call-template name="map.end"/>
		</xsl:if>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>revision</doc:db> elements</refpurpose>
		<doc:description>
			<para>
			
			Format a list of <doc:db basename="revision">revisions</doc:db>.
			
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				
				Each <doc:db>revision</doc:db> corresponds to a &LaTeX; table
				row containing the revision number, the date, author initials
				and the description/ remarks of the revision.
				
			</para>
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_article;
				&test_book;
				&test_ddh;
				&test_ieeebiblio;
				&test_mapping;
				&test_minimal;
			</simplelist>
		</doc:samples>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara>Gentext</simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="revision">
		<xsl:variable name="revnumber" select=".//revnumber"/>
		<xsl:variable name="revdate"   select=".//date"/>
		<xsl:variable name="revauthor" select=".//authorinitials"/>
		<xsl:variable name="revremark" select=".//revremark|.//revdescription"/>
		<!-- Row starts here -->
		<xsl:if test="$revnumber">
			<xsl:call-template name="gentext.element.name"/>
			<xsl:text> </xsl:text>
			<xsl:apply-templates select="$revnumber"/>
		</xsl:if>
		<xsl:text> &amp; </xsl:text>
		<xsl:apply-templates select="$revdate"/>
		<xsl:text> &amp; </xsl:text>
		<xsl:choose>
			<xsl:when test="count($revauthor)=0">
			<xsl:call-template name="dingbat">
				<xsl:with-param name="dingbat">nbsp</xsl:with-param>
			</xsl:call-template>
			</xsl:when>
			<xsl:otherwise>
			<xsl:apply-templates select="$revauthor"/>
			</xsl:otherwise>
		</xsl:choose>
		<!-- End Row here -->
		<xsl:text> \\ \hline&#10;</xsl:text>
		<!-- Add Remark Row if exists-->
		<xsl:if test="$revremark"> 
			<xsl:text>\multicolumn{3}{|l|}{</xsl:text>
			<xsl:apply-templates select="$revremark"/> 
			<!-- End Row here -->
			<xsl:text>} \\ \hline&#10;</xsl:text>
		</xsl:if>
	</xsl:template>


	<doc:template basename="authorinitials" xmlns="">
		<refpurpose>Process a <doc:db>revision</doc:db>'s <doc:db>authorinitials</doc:db> elements</refpurpose>
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
				Applies templates and outputs a "comma" between multiple
				<doc:db>authorinitials</doc:db> elements.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="revision/authorinitials">
		<xsl:apply-templates/>
		<xsl:if test="following-sibling::authorinitials">
			<xsl:text>, </xsl:text>
		</xsl:if>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>revnumber</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="revnumber">
		<xsl:apply-templates/>
	</xsl:template>

	<doc:template basename="date" xmlns="">
		<refpurpose>Process a <doc:db>revision</doc:db>'s <doc:db>date</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="revision/date">
		<xsl:apply-templates/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>revremark</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="revremark">
		<xsl:apply-templates/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>revdescription</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="revdescription">
		<xsl:apply-templates/>
	</xsl:template>

</xsl:stylesheet>
