<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: callout.mod.xsl,v 1.10 2004/01/14 14:54:32 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
+ ############################################################################## -->

<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="callout" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: callout.mod.xsl,v 1.10 2004/01/14 14:54:32 j-devenish Exp $
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
				<doc:revision rcasver="1.6">
					<date>January 2004</date>
					<revremark>Added callout support.</revremark>
				</doc:revision>
			</revhistory>
		</referenceinfo>
		<title>Callouts <filename>callout.mod.xsl</filename></title>
		<partintro>
			<para>
			
			These template use the concept of an <quote>areamark</quote>, an
			<quote>arearef</quote> and an <quote>areasymbol</quote>. An areamark
			is a way of illustrating a callout area as part of an image or
			listing. An arearef is a way of illustrating a callout area as part of
			a callout list. By default, the areamark and the arearef are both
			represented by the areasymbol. (Aside: that the areamark is drawn
			first.) This system allows opens the possibility for an areamark to
			draw a box around an area in addition to displaying the areasymbol.
			
			</para>
		</partintro>
	</doc:reference>

	<doc:template xmlns="">
		<refpurpose> Essential preamble for <filename>callout.mod.xsl</filename> support </refpurpose>
		<doc:description>
			<para>

				Loads the <productname>overpic</productname> packages and
				defines <function condition="latex">calsscale</function>
				and <function condition="latex">calspair</function> (which
				are used to convert <quote>calspair</quote> coordinates into
				<productname>overpic</productname> percent-style coordinates.

			</para>
		</doc:description>
		<doc:variables>
			<variablelist>
				<varlistentry>
					<term><xref linkend="param.latex.use.overpic"/></term>
					<listitem><simpara>
						Whether to enable this preamble or not.
						If 0, callouts will cause &LaTeX; errors.
					</simpara></listitem>
				</varlistentry>
			</variablelist>
		</doc:variables>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara>&preamble;</simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template name="latex.preamble.essential.callout">
		<xsl:if test="$latex.use.overpic=1 and //callout">
			<xsl:text>
				<![CDATA[
\usepackage[percent]{overpic}
\newdimen\dblatex@ascale \newdimen\dblatex@bscale
\newdimen\dblatex@adimen \newdimen\dblatex@bdimen
\newdimen\dblatex@cdimen \newdimen\dblatex@ddimen
\newcommand{\calsscale}{%
   \ifnum\@tempcnta>\@tempcntb%
      \dblatex@ascale=1pt%
      \dblatex@bscale=\@tempcntb pt%
      \divide\dblatex@bscale by \@tempcnta%
   \else%
      \dblatex@bscale=1 pt%
      \dblatex@ascale=\@tempcnta pt%
      \divide\dblatex@ascale by \@tempcntb%
   \fi%
}
\newcommand{\calspair}[3]{
   \sbox{\z@}{#3}
   \settowidth{\dblatex@cdimen}{\usebox{\z@}}
   \settoheight{\dblatex@ddimen}{\usebox{\z@}}
   \divide\dblatex@cdimen by 2
   \divide\dblatex@ddimen by 2
   \dblatex@adimen=#1 pt \dblatex@adimen=\strip@pt\dblatex@ascale\dblatex@adimen
   \dblatex@bdimen=#2 pt \dblatex@bdimen=\strip@pt\dblatex@bscale\dblatex@bdimen
   \put(\strip@pt\dblatex@adimen,\strip@pt\dblatex@bdimen){\hspace{-\dblatex@cdimen}\raisebox{-\dblatex@ddimen}{\usebox{\z@}}}
}
]]>
			</xsl:text>
		</xsl:if>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>programlistingco</doc:db> and <doc:db>screenco</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="programlistingco|screenco">
		<xsl:apply-templates select="programlisting|screen|calloutlist"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>areaset</doc:db>-related elements</refpurpose>
		<doc:description>
			<para>
				Suppressed (<doc:db>area</doc:db> templates are applied by
				<xref linkend="template.imagedata"/>).
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	 <xsl:template match="areaspec|areaset"/>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>co</doc:db>-related elements</refpurpose>
		<doc:description>
			<para>
				Print a callout number as a parenthesis.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>This template is probably never applied, because
			we can't yet handled <doc:db basename="co">cos</doc:db>
			in verbatim environments.</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="co">
		<xsl:variable name="conum">
			<xsl:number count="co" format="1"/>
		</xsl:variable>
		<xsl:text>(</xsl:text>
		<xsl:value-of select="$conum"/>
		<xsl:text>)</xsl:text>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>calloutlist</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="calloutlist">
		<xsl:apply-templates select="./title"/>
		<xsl:text>&#10;\begin{description}&#10;</xsl:text>
		<xsl:apply-templates select="callout"/>
		<xsl:text>\end{description}&#10;</xsl:text>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process titles for <doc:db>calloutlist</doc:db> elements</refpurpose>
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
	<xsl:template match="calloutlist/title">
		<xsl:param name="style" select="$latex.list.title.style"/>
		<xsl:text>&#10;{</xsl:text>
		<xsl:value-of select="$style"/>
		<xsl:text>{</xsl:text>
		<xsl:apply-templates/>
		<xsl:text>}}&#10;</xsl:text>
	</xsl:template>

	<doc:template basename="callout" xmlns="">
		<refpurpose>Process <doc:db>callout</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Formats arearefs as an <function condition="latex">item</function>,
				then applies templates. Since there may be multiple IDs specified
				in the <sgmltag class="attribute">arearefs</sgmltag> attribute,
				the <xref linkend="template.generate.callout.arearefs"/> template is
				called recursively to generate the arearefs.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="callout">
		<xsl:text>\item[{</xsl:text>
		<xsl:call-template name="generate.callout.arearefs"/>
		<xsl:text>}]\null{}&#10;</xsl:text>
		<xsl:apply-templates/>
		<xsl:text>&#10;</xsl:text>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Determine a <doc:db>callout</doc:db>'s arearefs</refpurpose>
		<doc:description>
			<para>
				Splits the arearef attribute on whitespace, then
				constructs a list of references by applying templates
				in <quote>generate.callout.arearef</quote> mode.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				The current node must be a <doc:db>callout</doc:db>.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template name="generate.callout.arearefs">
		<xsl:param name="arearefs" select="normalize-space(@arearefs)"/>
		<xsl:param name="count" select="1"/>
		<xsl:if test="$arearefs!=''">
			<xsl:choose>
				<xsl:when test="substring-before($arearefs,' ')=''">
					<xsl:apply-templates select="." mode="generate.callout.arearef">
						<xsl:with-param name="arearef" select="$arearefs"/>
						<xsl:with-param name="count" select="$count"/>
						<xsl:with-param name="last" select="true()"/>
					</xsl:apply-templates>
				</xsl:when>
				<xsl:otherwise>
					<xsl:apply-templates select="." mode="generate.callout.arearef">
						<xsl:with-param name="arearef" select="substring-before($arearefs,' ')"/>
						<xsl:with-param name="count" select="$count"/>
						<xsl:with-param name="last" select="false()"/>
					</xsl:apply-templates>
				</xsl:otherwise>
			</xsl:choose>
			<xsl:call-template name="generate.callout.arearefs">
				<xsl:with-param name="arearefs" select="substring-after($arearefs,' ')"/>
				<xsl:with-param name="count" select="$count + 1"/>
			</xsl:call-template>
		</xsl:if>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Format a <doc:db>callout</doc:db>'s arearefs</refpurpose>
		<doc:description>
			<para>
				Applies templates in <quote>generate.callout.arearef</quote> mode.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:params>
			<variablelist>
				<varlistentry>
					<term>arearef</term>
					<listitem><simpara>
						The ID of the <doc:db>area</doc:db> to which a <doc:db>callout</doc:db>
						refers.
					</simpara></listitem>
				</varlistentry>
				<varlistentry>
					<term>area</term>
					<listitem><simpara>
						The <doc:db>area</doc:db> object to which a <doc:db>callout</doc:db>
						refers. By default, this searches for a area whose <sgmltag
						class="attribute">id</sgmltag> attribute equals the
						<literal>arearef</literal> parameter.
					</simpara></listitem>
				</varlistentry>
				<varlistentry>
					<term>count</term>
					<listitem><simpara>
						The position of this reference in the list of references
						used by a given <doc:db>callout</doc:db>. Influences
						delimiters for list items.
					</simpara></listitem>
				</varlistentry>
				<varlistentry>
					<term>last</term>
					<listitem><simpara>
						Whether this area reference is the last one for a given
						<doc:db>callout</doc:db>. Influences delimiters for list
						items.
					</simpara></listitem>
				</varlistentry>
			</variablelist>
		</doc:params>
		<doc:notes>
			<para>
				Formats a reference for a single arearef. This is performed by
				applying templates for the <doc:db>area</doc:db> in
				<quote>generate.area.arearef</quote> mode.
			</para>
			<para>
				The current node must be a <doc:db>callout</doc:db>.
			</para>
			<para>
				Uses the <quote>naturalinlinelist</quote> localisation context.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="callout" mode="generate.callout.arearef">
		<xsl:param name="arearef" select="@arearefs"/>
		<xsl:param name="area" select="key('id', $arearef)"/>
		<xsl:param name="last" select="false()"/>
		<xsl:param name="count" select="1"/>
		<xsl:variable name="first" select="$count=1"/>
		<xsl:choose>
			<xsl:when test="$first">
				<xsl:call-template name="gentext.template">
					<xsl:with-param name="context" select="'naturalinlinelist'"/>
					<xsl:with-param name="name" select="'start'"/>
				</xsl:call-template>
			</xsl:when>
			<xsl:when test="$last">
				<xsl:call-template name="gentext.template">
					<xsl:with-param name="context" select="'naturalinlinelist'"/>
					<xsl:with-param name="name">
						<xsl:choose>
							<xsl:when test="$count &gt; 2">
								<xsl:text>lastofmany</xsl:text>
							</xsl:when>
							<xsl:otherwise>
								<xsl:text>lastoftwo</xsl:text>
							</xsl:otherwise>
						</xsl:choose>
					</xsl:with-param>
				</xsl:call-template>
			</xsl:when>
			<xsl:otherwise>
				<xsl:call-template name="gentext.template">
					<xsl:with-param name="context" select="'naturalinlinelist'"/>
					<xsl:with-param name="name" select="'middle'"/>
				</xsl:call-template>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:choose>
			<xsl:when test="$area">
				<xsl:apply-templates select="$area" mode="generate.area.arearef"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text>?</xsl:text>
				<xsl:message>
					<xsl:text>Error: no ID for constraint arearefs: </xsl:text>
					<xsl:value-of select="$arearef"/>
					<xsl:text>.</xsl:text>
				</xsl:message>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:if test="$last">
			<xsl:call-template name="gentext.template">
				<xsl:with-param name="context" select="'naturalinlinelist'"/>
				<xsl:with-param name="name" select="'end'"/>
			</xsl:call-template>
		</xsl:if>
	</xsl:template>

	<doc:template basename="area" xmlns="">
		<refpurpose>Illustrate a reference to a callout's area</refpurpose>
		<doc:description>
			<para>
				Formats an <doc:db>area</doc:db> as part of a
				callout list.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				The current node must be an <doc:db>area</doc:db>.
			</para>
			<para>
				Applies templates in the <quote>generate.arearef.calspair</quote>,
				<quote>generate.arearef.linerange</quote> and
				<quote>generate.arearef</quote> modes.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="area" mode="generate.area.arearef">
		<xsl:variable name="units">
			<xsl:choose>
				<xsl:when test="@units!=''">
					<xsl:value-of select="@units"/>
				</xsl:when>
				<xsl:when test="../@units!=''">
					<xsl:value-of select="../@units"/>
				</xsl:when>
				<xsl:when test="../../@units!=''">
					<xsl:value-of select="../../@units"/>
				</xsl:when>
			</xsl:choose>
		</xsl:variable>
		<xsl:choose>
			<xsl:when test="$units='calspair'">
				<xsl:apply-templates select="." mode="generate.arearef.calspair"/>
			</xsl:when>
			<xsl:when test="$units='linerange'">
				<xsl:apply-templates select="." mode="generate.arearef.linerange"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:apply-templates select="." mode="generate.arearef">
					<xsl:with-param name="units" select="$units"/>
				</xsl:apply-templates>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

	<doc:template basename="area" xmlns="">
		<refpurpose>Illustrate a reference to a callout's area</refpurpose>
		<doc:description>
			<para>
				Formats an <doc:db>area</doc:db> as part of a
				callout list.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				This is a fallback template for unknown units.
				It does not format an arearef but instead prints
				an error message.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="area" mode="generate.arearef">
		<xsl:param name="units"/>
		<xsl:message>Error: unsupported arearef units <xsl:value-of select="$units"/>.</xsl:message>
	</xsl:template>

	<doc:template basename="area" xmlns="">
		<refpurpose>Illustrate a reference to a callout's area</refpurpose>
		<doc:description>
			<para>
				Formats calspair units for a callout list.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				Calls <xref linkend="template.area-generate.area.areasymbol"/>.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="area" mode="generate.arearef.calspair">
		<xsl:apply-templates select="." mode="generate.area.areasymbol"/>
	</xsl:template>

	<doc:template basename="area" xmlns="">
		<refpurpose>Illustrate a reference to a callout's area</refpurpose>
		<doc:description>
			<para>
				Formats linerange units for a callout list.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				Formats a line range numerically, condensing the line range
				down to a single line reference if the starting line is the
				same as the finishing line.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="area" mode="generate.arearef.linerange">
		<xsl:choose>
			<xsl:when test="not(contains(@coords, ' '))">
				<xsl:value-of select="@coords"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:variable name="start" select="substring-before(@coords, ' ')"/>
				<xsl:variable name="finish" select="substring-after(@coords, ' ')"/>
				<xsl:choose>
					<xsl:when test="$start=$finish">
						<xsl:value-of select="$start"/>
					</xsl:when>
					<xsl:otherwise>
						<xsl:call-template name="string-replace">
							<xsl:with-param name="from" select="' '"/>
							<xsl:with-param name="to" select="'--'"/>
							<xsl:with-param name="string" select="@coords"/>
						</xsl:call-template>
					</xsl:otherwise>
				</xsl:choose>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

	<doc:template basename="area" xmlns="">
		<refpurpose>Illustrate a callout's area as part of an image or listing</refpurpose>
		<doc:description>
			<para>
				Formats an <doc:db>area</doc:db> as part of a
				displayed image or listing.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				Applies templates in the <quote>generate.area.areamark</quote> mode.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="area">
		<xsl:apply-templates select="." mode="generate.area.areamark"/>
	</xsl:template>

	<doc:template basename="area" xmlns="">
		<refpurpose>Illustrate a callout's area as part of an image or listing</refpurpose>
		<doc:description>
			<para>
				Formats an <doc:db>area</doc:db> as part of a
				displayed image or listing.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				Applies templates in the <quote>generate.areamark.calspair</quote> or
				<quote>generate.areamark</quote> modes.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="area" mode="generate.area.areamark">
		<xsl:variable name="units">
			<xsl:choose>
				<xsl:when test="@units!=''">
					<xsl:value-of select="@units"/>
				</xsl:when>
				<xsl:when test="../@units!=''">
					<xsl:value-of select="../@units"/>
				</xsl:when>
				<xsl:when test="../../@units!=''">
					<xsl:value-of select="../../@units"/>
				</xsl:when>
			</xsl:choose>
		</xsl:variable>
		<xsl:choose>
			<xsl:when test="$units='calspair'">
				<xsl:apply-templates select="." mode="generate.areamark.calspair"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:apply-templates select="." mode="generate.areamark">
					<xsl:with-param name="units" select="$units"/>
				</xsl:apply-templates>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

	<doc:template basename="area" xmlns="">
		<refpurpose>Illustrate a callout's area as part of an image or listing</refpurpose>
		<doc:description>
			<para>
				Formats an <doc:db>area</doc:db> as part of a
				displayed image or listing.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				This is a fallback template for unknown units.
				It does not format an areamark but instead prints
				an error message.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="area" mode="generate.areamark">
		<xsl:param name="units"/>
		<xsl:message>Error: unsupported areamark units <xsl:value-of select="$units"/>.</xsl:message>
	</xsl:template>

	<doc:template basename="area" xmlns="">
		<refpurpose>Illustrate a callout's area as part of an image or listing</refpurpose>
		<doc:description>
			<para>
				Formats calspair units for a displayed image or listing.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				Converts calspair coordinates relative to the width and height of the
				displayed image area. Will understand "x1,y1 x2,y2" and also "x1 y1".
				In the former case, the drawing location is moved to the centre of
				the implied rectangle. Templates are then applied in the
				<quote>generate.area.areasymbol</quote> mode.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="area" mode="generate.areamark.calspair">
		<xsl:choose>
			<xsl:when test="not(contains(@coords, ' '))">
				<xsl:message>Error: invalid calspair '<xsl:value-of select="@coords"/>'.</xsl:message>
			</xsl:when>
			<xsl:otherwise>
				<xsl:variable name="x1y1">
					<xsl:value-of select="substring-before(@coords, ' ')"/>
				</xsl:variable>
				<xsl:variable name="x1">
					<xsl:choose>
						<xsl:when test="contains($x1y1, ',')">
							<xsl:value-of select="substring-before($x1y1, ',')"/>
						</xsl:when>
						<xsl:otherwise>
							<xsl:value-of select="$x1y1"/>
						</xsl:otherwise>
					</xsl:choose>
				</xsl:variable>
				<xsl:variable name="y1">
					<xsl:choose>
						<xsl:when test="contains($x1y1, ',')">
							<xsl:value-of select="substring-after($x1y1, ',')"/>
						</xsl:when>
						<xsl:otherwise>
							<xsl:value-of select="''"/>
						</xsl:otherwise>
					</xsl:choose>
				</xsl:variable>
				<xsl:variable name="x2y2">
					<xsl:value-of select="substring-after(@coords, ' ')"/>
				</xsl:variable>
				<xsl:variable name="y2">
					<xsl:choose>
						<xsl:when test="contains($x2y2, ',')">
							<xsl:value-of select="substring-after($x2y2, ',')"/>
						</xsl:when>
						<xsl:otherwise>
							<xsl:value-of select="$x2y2"/>
						</xsl:otherwise>
					</xsl:choose>
				</xsl:variable>
				<xsl:variable name="x2">
					<xsl:choose>
						<xsl:when test="contains($x2y2, ',')">
							<xsl:value-of select="substring-before($x2y2, ',')"/>
						</xsl:when>
						<xsl:otherwise>
							<xsl:value-of select="''"/>
						</xsl:otherwise>
					</xsl:choose>
				</xsl:variable>
				<xsl:text>\calspair{</xsl:text>
				<!-- choose horizontal coordinate -->
				<xsl:choose>
					<xsl:when test="$x1 != '' and $x2 != ''">
						<xsl:value-of select="(number($x1)+number($x2)) div 200"/>
					</xsl:when>
					<xsl:otherwise>
						<xsl:value-of select="number(concat($x1, $x2)) div 100"/>
					</xsl:otherwise>
				</xsl:choose>
				<xsl:text>}{</xsl:text>
				<!-- choose vertical coordinate -->
				<xsl:choose>
					<xsl:when test="$y1 != '' and $y2 != ''">
						<xsl:value-of select="(number($y1)+number($y2)) div 200"/>
					</xsl:when>
					<xsl:otherwise>
						<xsl:value-of select="number(concat($y1, $y2)) div 100"/>
					</xsl:otherwise>
				</xsl:choose>
				<xsl:text>}{</xsl:text>
				<xsl:apply-templates select="." mode="generate.area.areasymbol"/>
				<xsl:text>}&#10;</xsl:text>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

	<doc:template basename="area" xmlns="">
		<refpurpose>Illustrate a callout's area as part of an image or listing</refpurpose>
		<doc:description>
			<para>
				Formats an <doc:db>area</doc:db> as an overlay
				on an image or listing.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				If the area has a <sgmltag class="attribute">label</sgmltag>
				attribute, it is used as raw &LaTeX; code.
			</para>
			<para>
				If the area has a linkends attribute, templates are
				applied for the first linkend using
				<quote>generate.callout.areasymbol</quote> mode.
				It is implicit in this scenario that an arearef
				and an areamark will both consist of an identical
				icon for an area.
			</para>
			<para>
				If none of the above were performed, an asterisk is printed.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="area" mode="generate.area.areasymbol">
		<xsl:param name="linkends" select="normalize-space(@linkends)"/>
		<xsl:choose>
			<xsl:when test="@label">
				<xsl:value-of select="@label"/>
			</xsl:when>
			<xsl:when test="$linkends!=''">
				<xsl:variable name="linkend">
					<xsl:choose>
						<xsl:when test="contains($linkends, ' ')">
							<xsl:value-of select="substring-before($linkends, ' ')"/>
						</xsl:when>
						<xsl:otherwise>
							<xsl:value-of select="$linkends"/>
						</xsl:otherwise>
					</xsl:choose>
				</xsl:variable>
				<xsl:variable name="target" select="key('id', $linkend)"/>
				<xsl:choose>
					<xsl:when test="count($target)&gt;0">
						<xsl:for-each select="$target">
							<xsl:apply-templates select="." mode="generate.callout.areasymbol">
								<xsl:with-param name="arearef" select="generate-id(current())"/>
								<xsl:with-param name="area" select="current()"/>
							</xsl:apply-templates>
						</xsl:for-each>
					</xsl:when>
					<xsl:otherwise>
						<xsl:text>?</xsl:text>
						<xsl:message>
							<xsl:text>Error: no ID for constraint linkends: </xsl:text>
							<xsl:value-of select="$linkends"/>
							<xsl:text>.</xsl:text>
						</xsl:message>
					</xsl:otherwise>
				</xsl:choose>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text>*</xsl:text>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

	<doc:template basename="callout" xmlns="">
		<refpurpose>Illustrate a callout's area as part of an image or listing</refpurpose>
		<doc:description>
			<para>
				Illustrates an <doc:db>area</doc:db> as part of a
				displayed image or listing.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				Prints the numeric position of the <doc:db>callout</doc:db> within its <doc:db>calloutlist</doc:db>.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="callout" mode="generate.callout.areasymbol">
		<xsl:number count="callout" format="1"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>mediaobjectco</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="mediaobjectco">
		<xsl:text>&#10;</xsl:text>
		<xsl:apply-templates select="imageobjectco"/>
		<xsl:text>&#10;</xsl:text>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>imageobjectco</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="imageobjectco">
		<xsl:apply-templates select="imageobject"/>
		<xsl:text>&#10;</xsl:text>
		<xsl:apply-templates select="calloutlist"/>
	</xsl:template>

	<doc:template basename="imageobject" xmlns="">
		<refpurpose>Process a <doc:db>imageobjectco</doc:db>'s <doc:db>imageobject</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates for <doc:db>imagedata</doc:db>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="imageobjectco/imageobject">
		<xsl:apply-templates select="imagedata">
			<xsl:with-param name="is.imageobjectco" select="true()"/>
		</xsl:apply-templates>
	</xsl:template>

</xsl:stylesheet>
