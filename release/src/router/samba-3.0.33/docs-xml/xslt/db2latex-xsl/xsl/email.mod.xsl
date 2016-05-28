<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: email.mod.xsl,v 1.6 2003/12/28 10:43:16 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
+ ############################################################################## -->

<xsl:stylesheet 
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="email" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: email.mod.xsl,v 1.6 2003/12/28 10:43:16 j-devenish Exp $
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
		<title>E-mail Addreses <filename>email.mod.xsl</filename></title>
		<partintro>
			<para>The file <filename>email.mod.xsl</filename> contains the
			XSL template for <doc:db>email</doc:db>.</para>
		</partintro>
	</doc:reference>

	<doc:template basename="email" match="email" xmlns="">
		<refpurpose>Process <doc:db>email</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Constructs a URL from the given e-mail address and
				formats it with the template for <doc:db>ulink</doc:db>.
			</para>
		</doc:description>
		<doc:variables>
			<para>As for the <xref linkend="template.ulink"/> template.</para>
		</doc:variables>
		<doc:notes>
			<para>When a an <doc:db>email</doc:db> is a child of an <doc:db>address</doc:db>,
			it will be formatted along with all <quote>verbatim</quote> address text. In this
			case, it might not be hyperlinked.</para>
		</doc:notes>
		<doc:samples>
			<simplelist type='inline'>
				&test_book;
				&test_ddh;
				&test_links;
			</simplelist>
		</doc:samples>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="template.ulink"/></simpara></listitem>
				<!--
				<listitem><simpara>&mapping;</simpara></listitem>
				-->
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="email">
		<xsl:call-template name="ulink">
			<xsl:with-param name="url" select="concat('mailto:',.)"/>
			<xsl:with-param name="content" select="."/>
		</xsl:call-template>
	</xsl:template>

</xsl:stylesheet>
