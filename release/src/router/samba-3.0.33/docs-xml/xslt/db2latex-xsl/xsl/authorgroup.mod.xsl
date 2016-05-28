<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: authorgroup.mod.xsl,v 1.10 2003/12/30 13:38:54 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $												
+ ############################################################################## -->

<xsl:stylesheet 
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="authorgroup" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: authorgroup.mod.xsl,v 1.10 2003/12/30 13:38:54 j-devenish Exp $
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
		<title>Authorgroups <filename>authorgroup.mod.xsl</filename></title>
		<partintro>
 			<para>The file <filename>authorgroup.mod.xsl</filename> contains the
 			XSL templates for <doc:db>author</doc:db>, <doc:db>editor</doc:db>,
 			<doc:db>othercredit</doc:db><doc:db>personname</doc:db>, <doc:db>authorblurb</doc:db>,
 			<doc:db>authorgroup</doc:db> and <doc:db>authorinitials</doc:db>.</para>
			<section>
				<title>Pertinent Variables</title>
				<itemizedlist>
					<listitem><simpara><xref linkend="param.biblioentry.item.separator"/></simpara></listitem>
				</itemizedlist>
			</section>
		</partintro>
	</doc:reference>

	<doc:template basename="authorgroup" xmlns="">
		<refpurpose>Process <doc:db>authorgroup</doc:db> elements</refpurpose>
		<doc:description>
 			<para>
				Formats a list of authors for typsetting as a formatted block
				(not inline).
 			</para>
			<para>
				Applies templates for <doc:db>author</doc:db> elements,
				inserting <quote>and</quote> between authors' names.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:params>
			<variablelist>
				<varlistentry>
					<term>person.list</term>
					<listitem>
						<para>

							The nodes to be formatted. By default, the template
							will select child::author, child::corpauthor,
							child::othercredit and child::editor for the
							current node.

						</para>
					</listitem>
				</varlistentry>
			</variablelist>
		</doc:params>
		<doc:notes>
 			<para>

 				May be called from any template when the current node has
 				<doc:db>author</doc:db>, <doc:db>editor</doc:db>,
 				<doc:db>corpauthor</doc:db> or <doc:db>othercredit</doc:db> children.

 			</para>
 			<para>

				This template uses <function
				condition='xslt'>person.name.list</function> from
				<filename>db2latex/xsl/common/common.xsl</filename> to format
				the list of authors.

			</para>
 
 			<para>
			
				For compatibility with &latex;, <xref
				linkend="template.normalize-scape"/> is called on the output of
				<function condition='xslt'>person.name.list</function>.
				<doc:todo>This may pose problems but has not been
				investigated.</doc:todo>
			
			</para>
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_article;
				&test_biblio;
				&test_bind;
				&test_book;
				&test_ieeebiblio;
				&test_minimal;
			</simplelist>
		</doc:samples>
	</doc:template>

	<xsl:template match="authorgroup" name="authorgroup">
		<xsl:param name="person.list" select="./author|./corpauthor|./othercredit|./editor"/>
		<xsl:call-template name="normalize-scape">
			<xsl:with-param name="string">
				<xsl:call-template name="person.name.list">
					<xsl:with-param name="person.list" select="$person.list"/>
				</xsl:call-template>
			</xsl:with-param>
		</xsl:call-template>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process names of <doc:db>authors</doc:db> and similar elements</refpurpose>
		<doc:description>
			<para>

				Formats a person's name for inline display.

			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>

			<para>
			
			This template uses <function
			condition='xslt'>person.name</function> from
			<filename>db2latex/xsl/common/common.xsl</filename> to format the
			names of <doc:db>author</doc:db>, <doc:db>editor</doc:db>,
			<doc:db>othercredit</doc:db> and <doc:db>personname</doc:db>
			elements.
			
			</para>

 			<para>
			
				For compatibility with &latex;, <xref
				linkend="template.normalize-scape"/> is called on the output of
				<function condition='xslt'>person.name.list</function>.
				<doc:todo>This may pose problems but has not been
				investigated.</doc:todo>
			
			</para>

		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				<member>See <xref linkend="template.authorgroup"/>.</member>
			</simplelist>
		</doc:samples>
	</doc:template>

	<xsl:template match="author|editor|othercredit|personname">
		<xsl:call-template name="normalize-scape">
			<xsl:with-param name="string">
				<xsl:call-template name="person.name"/>
			</xsl:with-param>
		</xsl:call-template>
	</xsl:template>

	<doc:template basename="authorinitials" xmlns="">
		<refpurpose>Process <doc:db>authorinitials</doc:db> elements</refpurpose>
		<doc:description>
			<para>

				Represents <doc:db>authorinitials</doc:db> by applying templates
				normally and then appending <xref
				linkend="param.biblioentry.item.separator"/>.

			</para>
		</doc:description>
		<doc:variables>
			<itemizedlist>
				<listitem><simpara><xref linkend="param.biblioentry.item.separator"/></simpara></listitem>
			</itemizedlist>
		</doc:variables>
		<doc:notes>
			<para>

			<doc:todo>The use of <xref linkend="param.biblioentry.item.separator"/>
			should be replaced with the normal localisation mechanism.</doc:todo>

			</para>
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_book;
				&test_article;
				&test_ieeebiblio;
				&test_minimal;
			</simplelist>
		</doc:samples>
	</doc:template>

	<xsl:template match="authorinitials">
		<xsl:apply-templates/>
		<xsl:value-of select="$biblioentry.item.separator"/>
	</xsl:template>

</xsl:stylesheet>
