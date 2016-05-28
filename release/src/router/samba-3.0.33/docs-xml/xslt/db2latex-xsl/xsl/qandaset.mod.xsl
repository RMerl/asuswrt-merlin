<?xml version="1.0"?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY % xsldoc.ent SYSTEM "./xsldoc.ent"> %xsldoc.ent; ]>
<!--############################################################################# 
|	$Id: qandaset.mod.xsl,v 1.13 2004/01/04 13:22:27 j-devenish Exp $
|- #############################################################################
|	$Author: j-devenish $
|   PURPOSE:
|   Portions (c) Norman Walsh, official DocBook XSL stylesheets.
|                See docbook.sf.net
+ ############################################################################## -->

<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform" 
	xmlns:doc="http://nwalsh.com/xsl/documentation/1.0" 
	exclude-result-prefixes="doc" version="1.0">

	<doc:reference id="qandaset" xmlns="">
		<referenceinfo>
			<releaseinfo role="meta">
				$Id: qandaset.mod.xsl,v 1.13 2004/01/04 13:22:27 j-devenish Exp $
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
				<doc:revision rcasver="1.11">&rev_2003_05;</doc:revision>
			</revhistory>
		</referenceinfo>
		<title>QandaSet <filename>qandaset.mod.xsl</filename></title>
		<partintro>
			<para>
			
				Portions (c) Norman Walsh, official DocBook XSL stylesheets. See docbook.sf.net
			
			</para>
		</partintro>
	</doc:reference>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>qandadiv</doc:db> elements</refpurpose>
		<doc:description>
			<para><doc:todo>Undocumented.</doc:todo></para>
		</doc:description>
	</doc:template>
<xsl:template match="qandaset">
		<!-- get all children that are not the following -->
		<xsl:variable name="preamble" select="*[name(.) != 'title'
									  and name(.) != 'titleabbrev'
									  and name(.) != 'blockinfo'
									  and name(.) != 'qandadiv'
									  and name(.) != 'qandaentry']"/>
		<xsl:variable name="label-width"/>
		<xsl:variable name="table-summary"/>
		<xsl:variable name="cellpadding"/>
		<xsl:variable name="cellspacing"/>
		<xsl:variable name="toc"/>
		<xsl:variable name="toc.params"/>
  		<xsl:variable name="qalevel">
    		<xsl:call-template name="qanda.section.level"/>
  		</xsl:variable>
		<xsl:text>% -------------------------------------------------------------&#10;</xsl:text>
		<xsl:text>% QandASet                                                     &#10;</xsl:text>
		<xsl:text>% -------------------------------------------------------------&#10;</xsl:text>
		<xsl:choose>
      		<xsl:when test="ancestor::sect2">
	    		<xsl:text>\subsubsection*{</xsl:text>
			</xsl:when>
      		<xsl:when test="ancestor::sect1">
	    		<xsl:text>\subsection*{</xsl:text>
			</xsl:when>
      		<xsl:when test="ancestor::article | ancestor::appendix">
	    		<xsl:text>\section*{</xsl:text>
			</xsl:when>
      		<xsl:when test="ancestor::book">
	    		<xsl:text>\chapter*{</xsl:text>
			</xsl:when>
		</xsl:choose>
		<xsl:choose>
			<xsl:when test="title">
				<xsl:apply-templates select="title"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text>F.A.Q.</xsl:text>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:text>}&#10;</xsl:text>
	    <xsl:call-template name="label.id"/>
		<xsl:text>&#10;</xsl:text>

		<!-- process toc -->
		<xsl:if test="contains($toc.params, 'toc') and $toc != '0'">
			<xsl:call-template name="process.qanda.toc"/>
		</xsl:if>
		<!-- process preamble -->
		<xsl:apply-templates select="$preamble"/>
		<!-- process divs and entries -->
		<xsl:apply-templates select="qandaentry|qandadiv"/>
</xsl:template>

	<doc:template basename="qandaentry" xmlns="">
		<refpurpose>Process <doc:db>qandadiv</doc:db> elements</refpurpose>
		<doc:description>
			<para><doc:todo>Undocumented.</doc:todo></para>
		</doc:description>
	</doc:template>
	<xsl:template match="qandaset/qandaentry">
		<xsl:text>\vspace{1em}&#10;</xsl:text>
		<xsl:text>\noindent{}</xsl:text>
		<xsl:value-of select="position()"/>
		<xsl:text>.~</xsl:text>
		<xsl:apply-templates select="question"/>
		<xsl:text>\newline&#10;</xsl:text>
		<xsl:apply-templates select="answer"/>
		<xsl:text>\vspace{1em}&#10;</xsl:text>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>qandadiv</doc:db> elements</refpurpose>
		<doc:description>
			<para><doc:todo>Undocumented.</doc:todo></para>
		</doc:description>
	</doc:template>
<xsl:template match="qandadiv">
	<!-- get the preamble -->
  <xsl:variable name="preamble" select="*[name(.) != 'title'
                                          and name(.) != 'titleabbrev'
                                          and name(.) != 'qandadiv'
                                          and name(.) != 'qandaentry']"/>
  <xsl:variable name="qalevel">
    <xsl:call-template name="qandadiv.section.level"/>
  </xsl:variable>
	<!-- process the title if it exists -->
		<xsl:text>% -----------&#10;</xsl:text>
		<xsl:text>% QandADiv   &#10;</xsl:text>
		<xsl:text>% -----------&#10;</xsl:text>
		<xsl:text>\noindent\begin{minipage}{\linewidth}&#10;</xsl:text>
		<xsl:text>\vspace{0.25em}\hrule\vspace{0.25em}&#10;</xsl:text>
		<xsl:choose>
      		<xsl:when test="ancestor::sect2">
	    		<xsl:text>\paragraph*{</xsl:text>
			</xsl:when>
      		<xsl:when test="ancestor::sect1">
	    		<xsl:text>\subsubsection*{</xsl:text>
			</xsl:when>
      		<xsl:when test="ancestor::article | ancestor::appendix">
	    		<xsl:text>\subsection*{</xsl:text>
			</xsl:when>
      		<xsl:when test="ancestor::book">
	    		<xsl:text>\section*{</xsl:text>
			</xsl:when>
		</xsl:choose>
		<xsl:choose>
			<xsl:when test="title">
				<xsl:apply-templates select="title"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text>F.A.Q. Part</xsl:text>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:text>}</xsl:text>
		<xsl:call-template name="label.id"/>
		<xsl:text>&#10;</xsl:text>
		<xsl:text>\hrule\vspace{0.25em}&#10;</xsl:text>
		<xsl:text>\end{minipage}&#10;</xsl:text>

<!--
	<xsl:text>\begin{toc}&#10;</xsl:text>
	<xsl:for-each select="qandaentry">
		<xsl:text>\tocref{</xsl:text>
		<xsl:value-of select="@id"/>
		<xsl:text>}&#10;</xsl:text>
	</xsl:for-each>
	<xsl:text>\end{toc}&#10;</xsl:text>
-->

<!-- pseudo table of contents -->
	<!--
	<xsl:choose>
		<xsl:when test="title">
			<xsl:text>\caption{</xsl:text>
			<xsl:apply-templates select="title"/>
			<xsl:text>}&#10;</xsl:text>
		</xsl:when>
		<xsl:otherwise>
			<xsl:text>\caption{</xsl:text>
			<xsl:text>F.A.Q. Part</xsl:text>
			<xsl:text>}&#10;</xsl:text>
		</xsl:otherwise>
	</xsl:choose>
	-->
	<xsl:for-each select="qandaentry">
	<xsl:text>\noindent{}</xsl:text>
	<xsl:value-of select="position()"/>
	<xsl:text>.~</xsl:text>
	<xsl:apply-templates select="question"/>
	<xsl:if test="position()!=last()"><xsl:text>\newline&#10;</xsl:text></xsl:if>
	</xsl:for-each>
	<xsl:text>\vspace{0.25em}\hrule&#10;</xsl:text>

	<xsl:for-each select="qandaentry">
	<xsl:text>\vspace{1em}&#10;</xsl:text>
	<xsl:text>\noindent{}</xsl:text>
	<xsl:value-of select="position()"/>
	<xsl:text>.~</xsl:text>
	<xsl:apply-templates select="question"/>
	<xsl:text>\newline&#10;</xsl:text>
	<xsl:apply-templates select="answer"/>
	</xsl:for-each>
	<xsl:text>\vspace{1em}&#10;</xsl:text>
</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>question</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates as a italicised block, preceded by a bold
				letter <quote>Q</quote>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
		<doc:notes>
			<para>defaultlabel attributes are not honoured.</para>
		</doc:notes>
	</doc:template>
	<xsl:template match="question">
		<!-- get the default label
		<xsl:variable name="deflabel">
			<xsl:choose>
				<xsl:when test="ancestor-or-self::*[@defaultlabel]">
					<xsl:value-of select="(ancestor-or-self::*[@defaultlabel])[last()]/@defaultlabel"/>
				</xsl:when>
				<xsl:otherwise>
					<xsl:value-of select="latex.qanda.defaultlabel"/>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:variable>
		-->
		<!-- process the question itself 
		<xsl:apply-templates select="." mode="label.markup"/>
		<xsl:choose>
			<xsl:when test="$deflabel = 'none' and not(label)">
				<xsl:apply-templates select="*[name(.) != 'label']"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:apply-templates select="*[name(.) != 'label']"/>
			</xsl:otherwise>
		</xsl:choose>
		-->
		<xsl:text>\textbf{Q:}~\textit{</xsl:text>
		<xsl:apply-templates/>
		<xsl:text>}&#10;</xsl:text>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>answer</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates as a block, preceded by a bold
				letter <quote>A</quote>.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="answer">
		<xsl:text>\noindent\textbf{A:}~</xsl:text>
		<xsl:apply-templates/>
		<xsl:text>&#10;&#10;</xsl:text>
	</xsl:template>

	<doc:template xmlns="">
		<refpurpose>Process <doc:db>label</doc:db> elements</refpurpose>
		<doc:description>
			<para>
				Applies templates.
			</para>
		</doc:description>
		<doc:variables>
			&no_var;
		</doc:variables>
	</doc:template>
	<xsl:template match="label">
		<xsl:apply-templates/>
	</xsl:template>

	<xsl:template name="process.qanda.toc">
		<xsl:apply-templates select="qandadiv" mode="qandatoc.mode"/>
		<xsl:apply-templates select="qandaentry" mode="qandatoc.mode"/>
	</xsl:template>

	<doc:template basename="qandadiv" xmlns="">
		<refpurpose>Process <doc:db>qandadiv</doc:db> elements</refpurpose>
		<doc:description>
			<para><doc:todo>Undocumented.</doc:todo></para>
		</doc:description>
	</doc:template>
	<xsl:template match="qandadiv" mode="qandatoc.mode">
		<xsl:apply-templates select="title" mode="qandatoc.mode"/>
		<xsl:call-template name="process.qanda.toc"/>
	</xsl:template>

	<doc:template basename="title" xmlns="">
		<refpurpose>Process a <doc:db>qandadiv</doc:db>'s <doc:db>title</doc:db> elements</refpurpose>
		<doc:description>
			<para><doc:todo>Undocumented.</doc:todo></para>
		</doc:description>
	</doc:template>
	<xsl:template match="qandadiv/title" mode="qandatoc.mode">
		<xsl:variable name="qalevel">
			<xsl:call-template name="qandadiv.section.level"/>
		</xsl:variable>
		<xsl:call-template name="label.id">
			<xsl:with-param name="object" select="parent::*"/>
		</xsl:call-template>
		<xsl:apply-templates select="parent::qandadiv" mode="label.markup"/>
		<xsl:value-of select="$autotoc.label.separator"/>
		<xsl:apply-templates/>
	</xsl:template>

	<doc:template xmlns="" basename="qandaentry">
		<refpurpose>Process <doc:db>qandaentry</doc:db> elements</refpurpose>
		<doc:description>
			<para><doc:todo>Undocumented.</doc:todo></para>
		</doc:description>
	</doc:template>
	<xsl:template match="qandaentry" mode="qandatoc.mode">
		<xsl:apply-templates mode="qandatoc.mode"/>
	</xsl:template>

	<doc:template basename="question" xmlns="">
		<refpurpose>Process <doc:db>question</doc:db> elements</refpurpose>
		<doc:description>
			<para><doc:todo>Undocumented.</doc:todo></para>
		</doc:description>
	</doc:template>
	<xsl:template match="question" mode="qandatoc.mode">
		<xsl:variable name="firstch" select="(*[name(.)!='label'])[1]"/>
		<xsl:apply-templates select="." mode="label.markup"/>
		<xsl:text> </xsl:text>
	</xsl:template>

	<doc:template xmlns="" basename="answer">
		<refpurpose>Process <doc:db>answer</doc:db> elements</refpurpose>
		<doc:description>
			<para><doc:todo>Undocumented.</doc:todo></para>
		</doc:description>
	</doc:template>
	<xsl:template match="answer" mode="qandatoc.mode"/>

	<doc:template xmlns="" basename="revhistory">
		<refpurpose>Process <doc:db>revhistory</doc:db> elements</refpurpose>
		<doc:description>
			<para><doc:todo>Undocumented.</doc:todo></para>
		</doc:description>
	</doc:template>
	<xsl:template match="revhistory" mode="qandatoc.mode"/>

<xsl:template name="question.answer.label">
	<!-- variable: deflabel -->
  <xsl:variable name="deflabel">
  	<!-- chck whether someone has a defaultlabel attribute -->
    <xsl:choose>
		<xsl:when test="ancestor-or-self::*[@defaultlabel]">
        	<xsl:value-of select="(ancestor-or-self::*[@defaultlabel])[last()]/@defaultlabel"/>
	      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="latex.qanda.defaultlabel"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:variable name="label" select="@label"/>
  <xsl:choose>
    <xsl:when test="$deflabel = 'qanda'">
      <xsl:call-template name="gentext">
        <xsl:with-param name="key">
          <xsl:choose>
            <xsl:when test="local-name(.) = 'question'">question</xsl:when>
            <xsl:when test="local-name(.) = 'answer'">answer</xsl:when>
            <xsl:when test="local-name(.) = 'qandadiv'">qandadiv</xsl:when>
            <xsl:otherwise>qandaset</xsl:otherwise>
          </xsl:choose>
        </xsl:with-param>
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="$deflabel = 'label'">
      <xsl:value-of select="$label"/>
    </xsl:when>
    <xsl:when test="$deflabel = 'number' and local-name(.) = 'question'">
      <xsl:apply-templates select="ancestor::qandaset[1]" mode="number"/>
      <xsl:choose>
        <xsl:when test="ancestor::qandadiv">
          <xsl:apply-templates select="ancestor::qandadiv[1]" mode="number"/>
          <xsl:apply-templates select="ancestor::qandaentry" mode="number"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:apply-templates select="ancestor::qandaentry" mode="number"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:when>
    <xsl:otherwise>
      <!-- nothing -->
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

</xsl:stylesheet>
