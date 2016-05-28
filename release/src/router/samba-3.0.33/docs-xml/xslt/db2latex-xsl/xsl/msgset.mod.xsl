<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: msgset.mod.xsl,v 1.2 2004/01/01 14:04:58 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
+ ############################################################################## -->

<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="msgset" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: msgset.mod.xsl,v 1.2 2004/01/01 14:04:58 j-devenish Exp $
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
				<doc:revision rcasver="1.2">&rev_2003_05;</doc:revision>
			</revhistory>
		</referenceinfo>
		<title>Message Sets <filename>msgset.mod.xsl</filename></title>
		<partintro>
			<para>
			
			
			</para>
		</partintro>
	</doc:reference>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>msgset</doc:db> elements</refpurpose>
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
	<xsl:template match="msgset">
		<xsl:apply-templates/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>msgentry</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Formats a block object using <xref linkend="template.block.object"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="msgentry">
		<xsl:call-template name="block.object"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>simplemsgentry</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Formats a block object using <xref linkend="template.block.object"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="simplemsgentry">
		<xsl:call-template name="block.object"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>msg</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Formats a block object using <xref linkend="template.block.object"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="msg">
		<xsl:call-template name="block.object"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>msgmain</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Formats a block object using <xref linkend="template.block.object"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="msgmain">
		<xsl:apply-templates/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>msgmain</doc:db>'s <doc:db>title</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates inside a <function condition="latex">textbf</function> command.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="msgmain/title|msgsub/title|msgrel/title">
		<xsl:text>{\textbf{</xsl:text>
		<xsl:apply-templates/>
		<xsl:text>}} </xsl:text>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>msgsub</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="msgsub">
		<xsl:apply-templates/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>msgrel</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="msgrel">
		<xsl:apply-templates/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>msgtext</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="msgtext">
		<xsl:apply-templates/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>msginfo</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Formats a block object using <xref linkend="template.block.object"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="msginfo">
		<xsl:call-template name="block.object"/>
	</xsl:template>

	<!-- localised -->
	<doc:template xmlns="">
		<refpurpose>Process <doc:db>msglevel</doc:db>, <doc:db>msgorig</doc:db> and <doc:db>msgaud</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates as a block, preceded by gentext inside a
				<function condition="latex">textbf</function> command.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="msglevel|msgorig|msgaud">
		<xsl:text>&#10;</xsl:text>
		<xsl:text>{\textbf{</xsl:text>
		<xsl:call-template name="gentext.element.name"/>
		<xsl:text>: </xsl:text>
		<xsl:text>}} </xsl:text>
		<xsl:apply-templates/>
		<xsl:text>&#10;</xsl:text>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>msgexplan</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Formats a block object using <xref linkend="template.block.object"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="msgexplan">
		<xsl:call-template name="block.object"/>
	</xsl:template>

</xsl:stylesheet>
