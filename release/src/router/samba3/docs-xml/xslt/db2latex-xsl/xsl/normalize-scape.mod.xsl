<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: normalize-scape.mod.xsl,v 1.33 2004/01/26 09:40:12 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
|														
|   PURPOSE:
|	Escape LaTeX and normalize-space templates.
|    < > # $ % & ~ _ ^ \ { } |
+ ############################################################################## -->

<xsl:stylesheet 
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="normalize-scape" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: normalize-scape.mod.xsl,v 1.33 2004/01/26 09:40:12 j-devenish Exp $
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
				<doc:revision rcasver="1.30">&rev_2003_05;</doc:revision>
			</revhistory>
		</referenceinfo>
		<title>Whitespace Normalization and Character Encoding <filename>normalize-scape.mod.xsl</filename></title>
		<partintro>

			<para>Normalize whitespace and and escape <quote>active</quote> &latex; characters.</para>
			<para>Includes the auto-generated <filename>scape.mod.xsl</filename> module.</para>

		</partintro>
	</doc:reference>

	<xsl:include href="scape.mod.xsl"/>

	<doc:template match="text()" xmlns="">
		<refpurpose>Process <literal>text()</literal> nodes</refpurpose>
		<doc:description>
			<para>

				Handles regular text content (i.e. <literal>#PCDATA</literal>)
				from &docbook; documents.

			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>

				For most elements, &latex; active characters
				will be escaped using the <xref linkend="template.scape"/>
				template.
				However, for text within the following elements, the
				<xref linkend="template.scape-verbatim"/> template
				will be used to typeset monospace text:
				<doc:db>literal</doc:db>,
				<doc:db>filename</doc:db>,
				<doc:db>userinput</doc:db>,
				<doc:db>systemitem</doc:db>,
				<doc:db>prompt</doc:db>,
				<doc:db>email</doc:db>,
				<doc:db>sgmltag</doc:db>.

			</para>
			<para>

				In all cases, interior whitespace will be normalised according
				to the XSLT specification with the additional feature that
				leading and trailing whitespace will be elided (as expected
				with SGML parsers).

			</para>
		</doc:notes>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara><xref linkend="gentext"/></simpara></listitem>
				<listitem><simpara><xref linkend="param.latex.inputenc"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="text()" name="text">
		<xsl:call-template name="trim-outer">
			<xsl:with-param name="string">
				<xsl:choose>
					<xsl:when test="ancestor::literal|ancestor::filename|ancestor::userinput|ancestor::systemitem|ancestor::prompt|ancestor::email|ancestor::sgmltag">
						<xsl:call-template name="scape-verbatim">
							<xsl:with-param name="string" select="."/>
						</xsl:call-template>
					</xsl:when>
					<xsl:otherwise>
						<xsl:call-template name="scape">
							<xsl:with-param name="string" select="."/>
						</xsl:call-template>
					</xsl:otherwise>
				</xsl:choose>
			</xsl:with-param>
		</xsl:call-template>
	</xsl:template>

	<!--
	<xsl:template match="abbrev/text()">
		<xsl:variable name="string">
			<xsl:call-template name="text"/>
		</xsl:variable>
		<xsl:call-template name="string-replace">
			<xsl:with-param name="to">.\ </xsl:with-param>
			<xsl:with-param name="from">. </xsl:with-param>
			<xsl:with-param name="string" select="$string"/>
		</xsl:call-template>
	</xsl:template>
	-->

	<doc:template match="text()" mode="xref.text" xmlns="">
		<refpurpose>Process <literal>text()</literal> nodes</refpurpose>
		<doc:description>
			<para>

				Handles regular text content (i.e. <literal>#PCDATA</literal>)
				from &docbook; documents when they are forming the displayed
				text of an <doc:db>xref</doc:db>.

			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>

				&latex; active characters will be escaped using the <xref
				linkend="template.scape"/> template.

			</para>
			<para>

				In all cases, interior whitespace will be normalised according
				to the XSLT specification with the additional feature that
				leading and trailing whitespace will be elided (as expected
				with SGML parsers).

			</para>
		</doc:notes>
	</doc:template>
    <xsl:template match="text()" mode="xref.text">
		<xsl:call-template name="trim-outer">
			<xsl:with-param name="string">
				<xsl:call-template name="scape">
					<xsl:with-param name="string" select="."/>
				</xsl:call-template>
			</xsl:with-param>
		</xsl:call-template>
    </xsl:template>

	<doc:template match="text()" mode="xref-to" xmlns="">
		<refpurpose>Process <literal>text()</literal> nodes</refpurpose>
		<doc:description>
			<para>

				Handles regular text content (i.e. <literal>#PCDATA</literal>)
				from &docbook; documents when they are forming the displayed
				text of an <doc:db>xref</doc:db>.

			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>

				&latex; active characters will be escaped using the <xref
				linkend="template.scape"/> template.

			</para>
			<para>

				In all cases, interior whitespace will be normalised according
				to the XSLT specification with the additional feature that
				leading and trailing whitespace will be elided (as expected
				with SGML parsers).

			</para>
		</doc:notes>
	</doc:template>
    <xsl:template match="text()" mode="xref-to">
		<xsl:call-template name="trim-outer">
			<xsl:with-param name="string">
				<xsl:call-template name="scape">
					<xsl:with-param name="string" select="."/>
				</xsl:call-template>
			</xsl:with-param>
		</xsl:call-template>
    </xsl:template>

	<doc:template match="text()" mode="latex.verbatim" xmlns="">
		<refpurpose>Process <literal>text()</literal> nodes</refpurpose>
		<doc:description>
			<para>

				Handles regular text content (i.e. <literal>#PCDATA</literal>)
				from &docbook; documents with they occur within certain
				<quote>verbatim</quote>-mode elements.

			</para>
		</doc:description>
		<doc:variables>
			<variablelist>
				<varlistentry>
					<term><xref linkend="param.latex.trim.verbatim"/></term>
					<listitem><simpara>
						When this variable is enabled, leading and trailing whitespace
						will be elided. Otherwise, all text is used verbatim.
					</simpara></listitem>
				</varlistentry>
			</variablelist>
		</doc:variables>
		<doc:notes>
			<para>

				Unlike other <literal>text()</literal> templates, &latex;
				characters are not escaped by this template. This will result
				in invalid output in some instances. However, it is currently
				necessary for <quote>verbatim</quote>-mode support. Whitespace
				is neither normalised nor elided.

			</para>
		</doc:notes>
	</doc:template>
    <xsl:template match="text()" mode="latex.verbatim">
		<xsl:choose>
			<xsl:when test="$latex.trim.verbatim=1">
				<xsl:call-template name="trim.verbatim"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="."/> 
			</xsl:otherwise>
		</xsl:choose>
    </xsl:template>

	<doc:template match="text()" mode="slash.hyphen" xmlns="">
		<refpurpose>Process <literal>text()</literal> nodes</refpurpose>
		<doc:description>
			<para>

				Handles URL text content from &docbook; documents.

			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>

				This template is only used by <xref
				linkend="template.generate.string.url"/> and only when <xref
				linkend="param.latex.hyphenation.tttricks"/> is disabled.
				&latex; active characters will be escaped or hyphenated in a
				fashion that is tailored for URLs via
				<xref linkend="template.scape.slash.hyphen"/>.

			</para>
		</doc:notes>
	</doc:template>
    <xsl:template match="text()" mode="slash.hyphen">
		<xsl:call-template name="trim-outer">
			<xsl:with-param name="string">
				<xsl:call-template name="scape.slash.hyphen">
					<xsl:with-param name="string" select="." />
				</xsl:call-template>
			</xsl:with-param>
		</xsl:call-template>
	</xsl:template>

	<doc:template name="trim-outer" xmlns="">
		<refpurpose>Whitespace Normalization and Discretionary Elision</refpurpose>
		<doc:description>
			<para>

				This template is used by various <literal>text()</literal>
				templates to normalise interior whitespace and trim whitespace
				that occurs at the start or end of a &docbook; element's
				content.

			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:params>
			<variablelist>
				<varlistentry>
					<term>string</term>
					<listitem><simpara>The text to be processed.</simpara></listitem>
				</varlistentry>
			</variablelist>
		</doc:params>
		<doc:notes>
			<para>

				The template is normally called when the context node is within
				a &docbook; document. The elision of leading or trailing
				whitespace is dependent on values of the XPath functions
				<function condition="xpath">position()</function> and <function
				condition="xpath">last()</function>. This is similar to the
				handling of whitespace by SGML parsers and allows authors to
				format their XML documents with <quote>pretty</quote>
				indentation without causing spurious whitespace in &latex;.

			</para>
			<para>

				In all cases, interiour whitespace will be normalised with the
				XPath <function condition="xpath">normalize-space()</function>
				function. This is necessary to prevent blank-line problems in
				&latex;.

			</para>
		</doc:notes>
	</doc:template>
	<xsl:template name="trim-outer">
		<xsl:param name="string"/>
		<xsl:variable name="trimleft" select="position()=1"/>
		<xsl:variable name="trimright" select="position()=last()"/>
		<xsl:choose>
			<xsl:when test="$trimleft and not($trimright)">
				<xsl:value-of select="substring-before(normalize-space(concat($string,'$$')),'$$')"/>
			</xsl:when>
			<xsl:when test="$trimright and not($trimleft)">
				<xsl:value-of select="substring-after(normalize-space(concat('$$',$string)),'$$')"/>
			</xsl:when>
			<xsl:when test="$trimleft and $trimright">
				<xsl:value-of select="normalize-space($string)"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="substring-after(substring-before(normalize-space(concat('$$',$string,'$$$')),'$$$'),'$$')"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

	<doc:template name="scape.slash.hyphen" xmlns="">
		<refpurpose>Process URL text</refpurpose>
		<doc:description>
			<para>

				Escapes or hyphenates &latex; active characters is URLs.

			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:params>
			<variablelist>
				<varlistentry>
					<term>string</term>
					<listitem><simpara>The URL text to be processed.</simpara></listitem>
				</varlistentry>
			</variablelist>
		</doc:params>
		<doc:notes>
			<para>

				This template is called by <xref
				linkend="template.text()-slash.hyphen"/>. Text will be escaped
				and hyphenated by the <xref linkend="template.scape-slash"/>
				template, except that any portion up to <literal>://</literal>
				will not be treated specially.

			</para>
		</doc:notes>
	</doc:template>
	<xsl:template name="scape.slash.hyphen">
	<xsl:param name="string" />
	<xsl:choose>
		<xsl:when test="contains($string,'://')">
			<xsl:call-template name="scape-slash">
				<xsl:with-param name="string">
					<xsl:value-of select="substring-before($string,'://')"/>
					<xsl:value-of select="'://'"/>
					<xsl:call-template name="scape">
						<xsl:with-param name="string" select="substring-after($string,'://')"/>
					</xsl:call-template>
				</xsl:with-param>
			</xsl:call-template>
		</xsl:when>
		<xsl:otherwise>
			<xsl:call-template name="scape-slash">
				<xsl:with-param name="string">
					<xsl:call-template name="scape">
						<xsl:with-param name="string" select="$string"/>
					</xsl:call-template>
				</xsl:with-param>
			</xsl:call-template>
		</xsl:otherwise>
	</xsl:choose>
	</xsl:template>

	<doc:template name="normalize-scape" xmlns="">
		<refpurpose>Character Escaping and Whitespace Normalization</refpurpose>
		<doc:description>
			<para>

				This template is used by various templates to escape &latex;
				active characters and to normalise whitespace.

			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:params>
			<variablelist>
				<varlistentry>
					<term>string</term>
					<listitem><simpara>The text to be processed.</simpara></listitem>
				</varlistentry>
			</variablelist>
		</doc:params>
		<doc:notes>
			<para>

				This template will call the <xref linkend="template.scape"/>
				template and process its output with the XPath
				<function condition="xpath">normalize-space</function>
				function.

			</para>
		</doc:notes>
	</doc:template>
	<xsl:template name="normalize-scape">
		<xsl:param name="string"/>
		<xsl:variable name="result">
			<xsl:call-template name="scape">
				<xsl:with-param name="string" select="$string"/>
			</xsl:call-template>
		</xsl:variable>
		<xsl:value-of select="normalize-space($result)"/>
	</xsl:template>

	<doc:template name="string-replace" xmlns="">
		<refpurpose>Search-and-replace</refpurpose>
		<doc:description>
			<para>

				This template performs search-and-replace to modify all
				instances of a substring.

			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:params>
			<variablelist>
				<varlistentry>
					<term>string</term>
					<listitem><simpara>The text to be searched.</simpara></listitem>
				</varlistentry>
				<varlistentry>
					<term>from</term>
					<listitem><simpara>The text (substring) to be replaced.</simpara></listitem>
				</varlistentry>
				<varlistentry>
					<term>to</term>
					<listitem><simpara>The text that replaces the <literal>from</literal> text.</simpara></listitem>
				</varlistentry>
			</variablelist>
		</doc:params>
		<doc:notes>
			<para>

				This template will search within the <literal>string</literal>
				text for all occurrences of <literal>from</literal> and replace
				them with the <literal>to</literal> text.

			</para>
		</doc:notes>
	</doc:template>
	<xsl:template name="string-replace">
		<xsl:param name="string"/>
		<xsl:param name="from"/>
		<xsl:param name="to"/>

		<xsl:choose>
			<xsl:when test="contains($string, $from)">

			<xsl:variable name="before" select="substring-before($string, $from)"/>
			<xsl:variable name="after" select="substring-after($string, $from)"/>
			<xsl:variable name="prefix" select="concat($before, $to)"/>

			<xsl:value-of select="$before"/>
			<xsl:value-of select="$to"/>
			<xsl:call-template name="string-replace">
				<xsl:with-param name="string" select="$after"/>
				<xsl:with-param name="from" select="$from"/>
				<xsl:with-param name="to" select="$to"/>
			</xsl:call-template>
			</xsl:when>
			<xsl:otherwise>
			<xsl:value-of select="$string"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

    <!--  
    (c) David Carlisle
    replace all occurences of the character(s) `from'
    by the string `to' in the string `string'.
    <xsl:template name="string-replace" >
	<xsl:param name="string"/>
	<xsl:param name="from"/>
	<xsl:param name="to"/>
	<xsl:choose>
	    <xsl:when test="contains($string,$from)">
		<xsl:value-of select="substring-before($string,$from)"/>
		<xsl:value-of select="$to"/>
		<xsl:call-template name="string-replace">
		    <xsl:with-param name="string" select="substring-after($string,$from)"/>
		    <xsl:with-param name="from" select="$from"/>
		    <xsl:with-param name="to" select="$to"/>
		</xsl:call-template>
	    </xsl:when>
	    <xsl:otherwise>
		<xsl:value-of select="$string"/>
	    </xsl:otherwise>
	</xsl:choose>
    </xsl:template>
    -->

	<xsl:template name="trim.verbatim">
		<xsl:variable name="before" select="preceding-sibling::node()"/>
		<xsl:variable name="after" select="following-sibling::node()"/>

		<xsl:variable name="conts" select="."/>

		<xsl:variable name="contsl">
			<xsl:choose>
				<xsl:when test="count($before) = 0">
					<xsl:call-template name="remove-lf-left">
						<xsl:with-param name="astr" select="$conts"/>
					</xsl:call-template>
				</xsl:when>
				<xsl:otherwise>
					<xsl:value-of select="$conts"/>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:variable>

		<xsl:variable name="contslr">
			<xsl:choose>
				<xsl:when test="count($after) = 0">
					<xsl:call-template name="remove-ws-right">
						<xsl:with-param name="astr" select="$contsl"/>
					</xsl:call-template>
				</xsl:when>
				<xsl:otherwise>
					<xsl:value-of select="$contsl"/>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:variable>

		<xsl:value-of select="$contslr"/>
	</xsl:template>

	<xsl:template name="remove-lf-left">
		<xsl:param name="astr"/>
		<xsl:choose>
			<xsl:when test="starts-with($astr,'&#xA;') or
			                starts-with($astr,'&#xD;') or
			                starts-with($astr,'&#x20;') or
			                starts-with($astr,'&#x9;')">
				<xsl:call-template name="remove-lf-left">
					<xsl:with-param name="astr" select="substring($astr, 2)"/>
				</xsl:call-template>
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="$astr"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

	<xsl:template name="remove-ws-right">
		<xsl:param name="astr"/>
		<xsl:variable name="last-char">
			<xsl:value-of select="substring($astr, string-length($astr), 1)"/>
		</xsl:variable>
		<xsl:choose>
			<xsl:when test="($last-char = '&#xA;') or
			                ($last-char = '&#xD;') or
			                ($last-char = '&#x20;') or
			                ($last-char = '&#x9;')">
				<xsl:call-template name="remove-ws-right">
					<xsl:with-param name="astr" select="substring($astr, 1, string-length($astr) - 1)"/>
				</xsl:call-template>
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="$astr"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

</xsl:stylesheet>
