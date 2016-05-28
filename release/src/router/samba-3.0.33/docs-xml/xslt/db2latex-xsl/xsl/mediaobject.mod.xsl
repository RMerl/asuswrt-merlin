<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: mediaobject.mod.xsl,v 1.22 2004/01/12 13:52:30 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
+ ############################################################################## -->

<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
	exclude-result-prefixes="doc" version='1.0'>

	<doc:reference id="mediaobject" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: mediaobject.mod.xsl,v 1.22 2004/01/12 13:52:30 j-devenish Exp $
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
		<title>MediaObjects <filename>mediaobject.mod.xsl</filename></title>
		<partintro>
			<para>
			
			
			</para>
		</partintro>
	</doc:reference>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>textobject</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="textobject">
		<!-- TODO if mixed in with imageobjects, use subfigure (if appropriate) -->
		<xsl:apply-templates/>
	</xsl:template>

	<doc:template basename="mediaobject" xmlns="">
		<refpurpose>Process <doc:db>mediaobject</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Calls <xref linkend="template.mediacontent"/>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:samples>
			<simplelist type='inline'>
				&test_subfig;
			</simplelist>
		</doc:samples>
	</doc:template>
	<xsl:template match="mediaobject">
		<xsl:if test="local-name(preceding-sibling::*[1])!='mediaobject'">
			<xsl:text>&#10;</xsl:text>
		</xsl:if>
		<xsl:call-template name="mediacontent"/>
		<xsl:text>&#10;</xsl:text>
	</xsl:template>

	<doc:template basename="mediaobject" xmlns="">
		<refpurpose>Process a <doc:db>para</doc:db>'s <doc:db>mediaobject</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Formats a <doc:db>mediaobject</doc:db> as a block surrounded by paragraph text.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				Inserts newline characters around the output of <xref
				linkend="template.mediacontent"/>.
			</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="para/mediaobject">
		<xsl:text>&#10;&#10;</xsl:text>
		<xsl:call-template name="mediacontent"/>
		<xsl:text>&#10;&#10;</xsl:text>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>inlinemediaobject</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="inlinemediaobject">
		<xsl:call-template name="mediacontent"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process media and inline media </refpurpose>
		<doc:description>
			<para>
				Formats image media.
				Would be good to be able to include text media, too,
				so that mixed-content figures look proper.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>
				The template first attempts to obtain a count of the number
				of media within this <doc:db>figure</doc:db>, if this is within a <sgmltag>figure</sgmltag>.
				If the number of objects is greater than one, a <function condition="latex">subfigure</function>
				command will be invoked with the contents of any <doc:db>caption</doc:db>.
				If there are no <doc:db basename="imageobject">imageobjects</doc:db>, <doc:db basename="textobject">textobjects</doc:db>
				will be selected.
				Otherwise, the following algorithm will be used:
			</para>
			<procedure>
				<step><simpara>If <xref linkend="param.use.role.for.mediaobject"/> is set and there is an <doc:db>imageobject</doc:db> with a <sgmltag class="attribute">role</sgmltag> equal to the current <xref linkend="param.preferred.mediaobject.role"/> then that object will be used.</simpara></step>
				<step><simpara>Otherwise, if <xref linkend="param.use.role.for.mediaobject"/> is set and there is an <sgmltag>imageobject</sgmltag> with a role of <quote>latex</quote> or <quote>tex</quote>, that object will be used.</simpara></step>
				<step><simpara>Otherwise, if <xref linkend="param.latex.graphics.formats"/> is set and there are <sgmltag>imagedata</sgmltag> with non-empty <sgmltag class="attribute">format</sgmltag> attributes, and at least one of them has a matching format, then the first one of those objects is used. If none match, then the first <sgmltag>imagedata</sgmltag> with an <emphasis>empty</emphasis> format is used. Otherwise, the first <doc:db>textobject</doc:db> is used.</simpara></step>
				<step><simpara>Otherwise, the first <sgmltag>imageobject</sgmltag> is used regardless.</simpara></step>
			</procedure>
		</doc:notes>
	</doc:template>
	<xsl:template name="mediacontent">
		<!--
		<xsl:variable name="actualmediacnt" select="count(../../..//mediaobject[imageobject or textobject])"/>
		-->
		<xsl:variable name="actualmediacnt" select="count(../mediaobject)"/>
		<xsl:if test="$actualmediacnt &gt; 1 and $latex.use.subfigure='1' and count(ancestor::figure) &gt; 0">
			<xsl:text>\subfigure[</xsl:text>
			<!-- TODO does subfigure stuff up with there are square brackets in here? -->
			<xsl:if test="caption">
				<xsl:apply-templates select="caption[1]"/>
			</xsl:if>
			<xsl:text>]</xsl:text>
		</xsl:if>
		<xsl:text>{</xsl:text>
		<xsl:choose>
			<xsl:when test="count(imageobject)&lt;1">
				<xsl:apply-templates select="textobject[1]"/>
			</xsl:when>
			<xsl:when test="$use.role.for.mediaobject='1' and $preferred.mediaobject.role!='' and count(imageobject[@role=$preferred.mediaobject.role])!=0">
				<xsl:apply-templates select="imageobject[@role=$preferred.mediaobject.role]"/>
			</xsl:when>
			<xsl:when test="$use.role.for.mediaobject='1' and count(imageobject[@role='latex'])!=0">
				<xsl:apply-templates select="imageobject[@role='latex']"/>
			</xsl:when>
			<xsl:when test="$use.role.for.mediaobject='1' and count(imageobject[@role='tex'])!=0">
				<xsl:apply-templates select="imageobject[@role='tex']"/>
			</xsl:when>
			<xsl:when test="$latex.graphics.formats!='' and count(imageobject/imagedata[@format!=''])!=0">
				<!-- this is not really the right method: formats to the left of $latex.graphics.formats
				should be given higher 'priority' than those to the right in a command-separated list -->
				<xsl:variable name="formats" select="concat(',',$latex.graphics.formats,',')"/>
				<xsl:variable name="candidates" select="imageobject/imagedata[contains($formats,concat(',',@format,','))]"/>
				<xsl:choose>
					<xsl:when test="count($candidates)!=0">
						<xsl:apply-templates select="$candidates[1]"/>
					</xsl:when>
					<xsl:otherwise>
						<xsl:variable name="fallbacks" select="imageobject/imagedata[@format='']"/>
						<xsl:choose>
							<xsl:when test="count($fallbacks)!=0">
								<xsl:apply-templates select="$fallbacks[1]"/>
							</xsl:when>
							<xsl:when test="count(textobject)!=0">
								<xsl:apply-templates select="textobject[1]"/>
							</xsl:when>
							<xsl:otherwise>
								<xsl:apply-templates select="imageobject[1]"/>
							</xsl:otherwise>
						</xsl:choose>
					</xsl:otherwise>
				</xsl:choose>
			</xsl:when>
			<xsl:otherwise>
				<xsl:apply-templates select="imageobject[1]"/>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:text>}</xsl:text>
	</xsl:template>

	<doc:template basename="imageobject" xmlns="">
		<refpurpose>Process <doc:db>imageobject</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="imageobject">
		<xsl:apply-templates select="imagedata"/>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>imagedata</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Formats a picture using <function condition="latex">includegraphics</function>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:params>
			<variablelist>
				<varlistentry>
					<term>filename</term>
					<listitem><simpara>The file path to be passsed to <function
					condition="latex">includegraphics</function>. By default,
					the name of the graphics file is gathered from the <sgmltag
					class="attribute">entityref</sgmltag> attribute, if it is
					present, or otherwise from the <sgmltag
					class="attribute">fileref</sgmltag> attribute.
					Often with &LaTeX;, the <sgmltag class="attribute">fileref</sgmltag>
					attribute need not end with any <quote>filename extension</quote>
					(see <xref linkend="param.graphic.default.extension"/>).
					</simpara></listitem>
				</varlistentry>
			</variablelist>
		</doc:params>
		<doc:notes>
			<itemizedlist>
				<listitem><para>If both <literal>@width</literal> and <literal>@scale</literal> are given but <literal>@scalefit='0'</literal>, whitespace is added to the left and right in order to match the specified width.</para></listitem>
				<listitem><para>If <literal>@width</literal> is given and either <literal>@scalefit=1</literal> or no <literal>@scale</literal> is given, then the image is scale to <literal>@width</literal>. Otherwise, <literal>@scale</literal> is used, if it is present.</para></listitem>
				<listitem><para>If this is not the only <literal>imagedata</literal> within the figure, this will be rendered as a 'subfigure', including the <literal>caption</literal> of its enclosing <literal>mediaobject</literal>.</para></listitem>
			</itemizedlist>
			<para>
				For widths, those containing a percent symbol (<quote>%</quote>) will be
				taken relative to the <function condition="latex">textwidth</function>.
			</para>
			<para>
				The <quote>PRN</quote> value of the <sgmltag class="attribute">format</sgmltag> attribute is honoured.
			</para>
		</doc:notes>
		<doc:seealso>
			<itemizedlist>
				<listitem><simpara>&mapping;</simpara></listitem>
				<listitem><simpara><xref linkend="template.content-templates"/></simpara></listitem>
			</itemizedlist>
		</doc:seealso>
	</doc:template>
	<xsl:template match="imagedata" name="imagedata">
		<xsl:param name="filename">
			<xsl:choose>
				<xsl:when test="@entityref">
					<xsl:value-of select="unparsed-entity-uri(@entityref)"/>
				</xsl:when>
				<xsl:otherwise>
					<xsl:value-of select="@fileref"/>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:param>
		<xsl:param name="is.imageobjectco" select="false()"/>
		<xsl:variable name="width">
			<xsl:choose>
				<xsl:when test="contains(@width, '%') and substring-after(@width, '%')=''">
					<xsl:value-of select="number(substring-before(@width, '%')) div 100"/>
					<xsl:text>\textwidth</xsl:text>
				</xsl:when>
				<xsl:otherwise>
					<xsl:value-of select="@width"/>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:variable>
		<xsl:if test="$width!='' and (@scalefit='0' or count(@scale)&gt;0)">
			<xsl:text>\makebox[</xsl:text><xsl:value-of select='$width' /><xsl:text>]</xsl:text>
		</xsl:if>
		<!-- TODO this logic actually needs to make decisions based on the ALLOWED imagedata,
		not all the imagedata present in the source file. -->
		<xsl:choose>
			<xsl:when test="$is.imageobjectco=1">
				<xsl:text>{\begin{overpic}[</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text>{\includegraphics[</xsl:text>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:choose>
			<xsl:when test="@scale"> 
			<xsl:text>scale=</xsl:text>
			<xsl:value-of select="number(@scale) div 100"/>
			</xsl:when>
			<xsl:when test="$width!='' and @scalefit='1'">
			<xsl:text>width=</xsl:text><xsl:value-of select="normalize-space($width)"/>
			</xsl:when>
			<xsl:when test="@depth!='' and @scalefit='1'">
			<xsl:text>height=</xsl:text><xsl:value-of select="normalize-space(@depth)"/>
			</xsl:when>
		</xsl:choose>
		<xsl:choose>
			<xsl:when test="@format = 'PRN'"><xsl:text>,angle=270</xsl:text></xsl:when>
		</xsl:choose>
		<xsl:text>]{</xsl:text>
		<xsl:value-of select="$filename"/>
		<xsl:choose>
			<xsl:when test="$is.imageobjectco=1">
				<xsl:text>}&#10;\calsscale&#10;</xsl:text>
				<xsl:apply-templates select="ancestor::imageobjectco/areaspec//area"/>
				<xsl:text>\end{overpic}}</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text>}}</xsl:text>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>caption</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="caption">
		<xsl:apply-templates/>
	</xsl:template>

</xsl:stylesheet>
