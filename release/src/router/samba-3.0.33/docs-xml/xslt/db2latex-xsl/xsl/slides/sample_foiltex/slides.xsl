<?xml version='1.0'?>
<!--############################################################################# 
 |	$Id: slides.xsl,v 1.1 2003/07/22 07:12:13 rcasellas Exp $
 |- #############################################################################
 |	$Author: rcasellas $												
 |														
 + ############################################################################## -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version='1.0'>

<xsl:import href="../slides.xsl"/>

    <xsl:variable name="db2latex.slides.packages">
    	<xsl:text>\usepackage[latin1]{inputenc}&#10;</xsl:text>
    	<xsl:text>\usepackage{pstricks,pst-node,pst-text,pst-3d}&#10;</xsl:text>
    	<xsl:text>\usepackage{subfigure}&#10;</xsl:text>
    	<xsl:text>\usepackage{fancybox}&#10;</xsl:text>
    	<xsl:text>\usepackage{a4wide}&#10;</xsl:text>
    	<xsl:text>\usepackage{times}&#10;</xsl:text>
    	<xsl:text>\usepackage{fancyvrb}&#10;</xsl:text>
    	<xsl:text>\usepackage{amsmath,amsthm, amsfonts, amssymb, amsxtra,amsopn}&#10;</xsl:text>
		<xsl:text>\usepackage{anysize}&#10;</xsl:text>
		<xsl:text>\usepackage[pdftex,bookmarksnumbered,colorlinks,backref, bookmarks, breaklinks, </xsl:text>
		<xsl:text>linktocpage,hyperfigures,hyperindex,citecolor=blue,urlcolor=blue]{hyperref}&#10;</xsl:text>
		<xsl:text>\usepackage[english]{babel}&#10;</xsl:text>
		<xsl:text>\usepackage[pdftex]{graphicx}&#10;</xsl:text>

	</xsl:variable>

	

    <xsl:variable name="db2latex.slides.optpackages"/>


    <xsl:variable name="db2latex.slides.beforebegin">
		<xsl:text>\pdfcompresslevel=9&#10;</xsl:text>
		<xsl:text>%------------------------------------------------------- Values and counters&#10;</xsl:text>
		<xsl:text>\marginsize{1.5cm}{1.5cm}{0.5cm}{0.5cm}&#10;</xsl:text>
		<xsl:text>\renewcommand\floatpagefraction{.9}&#10;</xsl:text>
		<xsl:text>\renewcommand\topfraction{.9}&#10;</xsl:text>
		<xsl:text>\renewcommand\bottomfraction{.9}&#10;</xsl:text>
		<xsl:text>\renewcommand\textfraction{.1}&#10;</xsl:text>
		<xsl:text>&#10;</xsl:text>
		<xsl:text>\usepackage[usenames,pdftex]{color}&#10;</xsl:text>
		<xsl:text>%------------------------------------------------------- BfBlue Command&#10;</xsl:text>
		<xsl:text>\newcommand{\bfblue}[1]{ \textcolor{blue}{\bf #1} }&#10;</xsl:text>
		<xsl:text>&#10;</xsl:text>
		<xsl:text>%------------------------------------------------------- BfGreen Command&#10;</xsl:text>
		<xsl:text>\newcommand{\bfgreen}[1]{ \textcolor{blue}{\bf #1} }&#10;</xsl:text>
		<xsl:text>&#10;</xsl:text>
		<xsl:text>%------------------------------------------------------- BfRed Command&#10;</xsl:text>
		<xsl:text>\newcommand{\bfred}[1]{ \textcolor{red}{\bf #1} }&#10;</xsl:text>
		<xsl:text>&#10;</xsl:text>
		<xsl:text>%------------------------------------------------------- BfBlue Command&#10;</xsl:text>
		<xsl:text>\newcommand{\emblue}[1]{ \textcolor{blue}{\emph{#1}} }&#10;</xsl:text>
		<xsl:text>&#10;</xsl:text>
		<xsl:text>%------------------------------------------------------- BfGreen Command&#10;</xsl:text>
		<xsl:text>\newcommand{\emgreen}[1]{ \textcolor{blue}{\emph{#1}} }&#10;</xsl:text>
		<xsl:text>&#10;</xsl:text>
		<xsl:text>%------------------------------------------------------- BfRed Command&#10;</xsl:text>
		<xsl:text>\newcommand{\emred}[1]{ \textcolor{red}{\emph{#1}} }&#10;</xsl:text>
		<xsl:text>&#10;</xsl:text>
		<xsl:text>%------------------------------------------------------- Part Command&#10;</xsl:text>
		<xsl:text>\newcommand{\part}[1]{&#10;</xsl:text>
		<xsl:text>\foilhead{}&#10;</xsl:text>
		<xsl:text>\vspace{2cm}&#10;</xsl:text>
		<xsl:text>\begin{center}&#10;</xsl:text>
		<xsl:text>\Huge{\textcolor{blue}{#1}}&#10;</xsl:text>
		<xsl:text>\end{center}}&#10;</xsl:text>
		<xsl:text> \newcommand{\dbz}{} &#10;</xsl:text>
		<xsl:text>%------------------------------------------------------- Slide Command&#10;</xsl:text>
		<xsl:text>\newcommand{\slide}[1]{&#10;</xsl:text>
		<xsl:text>\foilhead[-0.5in]{\large{\textcolor{blue}{#1}}}&#10;</xsl:text>
		<xsl:text>}&#10;</xsl:text>
		<xsl:text>\newcommand{\id}[1]{&#10;</xsl:text>
		<xsl:text>\label{#1}&#10;</xsl:text>
		<xsl:text>\hypertarget{#1}{}&#10;</xsl:text>
		<xsl:text>}&#10;</xsl:text>
		<xsl:text>% --------------------------------------------&#10;</xsl:text>
		<xsl:text>\newenvironment{admminipage}{&#10;</xsl:text>
		<xsl:text>\begin{Sbox}&#10;</xsl:text>
		<xsl:text>\begin{minipage}&#10;</xsl:text>
		<xsl:text>}{&#10;</xsl:text>
		<xsl:text>\end{minipage}&#10;</xsl:text>
		<xsl:text>\end{Sbox}&#10;</xsl:text>
		<xsl:text>\fbox{\TheSbox}&#10;</xsl:text>
		<xsl:text>}&#10;</xsl:text>
		<xsl:text>\newlength{\admlength}&#10;</xsl:text>
		<xsl:text>\newenvironment{admonition}[2] {&#10;</xsl:text>
		<xsl:text>\hspace{0mm}\newline\hspace*\fill\newline&#10;</xsl:text>
		<xsl:text>\noindent&#10;</xsl:text>
		<xsl:text>\setlength{\fboxsep}{5pt}&#10;</xsl:text>
		<xsl:text>\setlength{\admlength}{\linewidth}&#10;</xsl:text>
		<xsl:text>\addtolength{\admlength}{-10\fboxsep}&#10;</xsl:text>
		<xsl:text>\addtolength{\admlength}{-10\fboxrule}&#10;</xsl:text>
		<xsl:text>\admminipage{\admlength}&#10;</xsl:text>
		<xsl:text>\bfblue{\sc\large{#2}}\newline&#10;</xsl:text>
		<xsl:text>\\[1mm]&#10;</xsl:text>
		<xsl:text>%\sffamily&#10;</xsl:text>
		<xsl:text>\includegraphics[width=1cm]{#1}&#10;</xsl:text>
		<xsl:text>\addtolength{\admlength}{-1cm}&#10;</xsl:text>
		<xsl:text>\addtolength{\admlength}{-20pt}&#10;</xsl:text>
		<xsl:text>\begin{minipage}[lt]{\admlength}&#10;</xsl:text>
		<xsl:text>\parskip=0.5\baselineskip \advance\parskip by 0pt plus 2pt&#10;</xsl:text>
		<xsl:text>}{&#10;</xsl:text>
		<xsl:text>\vspace{5mm}&#10;</xsl:text>
		<xsl:text>\end{minipage}&#10;</xsl:text>
		<xsl:text>\endadmminipage&#10;</xsl:text>
		<xsl:text>\vspace{.5em}&#10;</xsl:text>
		<xsl:text>\par&#10;</xsl:text>
		<xsl:text>}&#10;</xsl:text>

    </xsl:variable>



    <xsl:template match="slides">
<!-- Document class and preamble -->
    <xsl:text>\documentclass[17pt,headrule,footrule,landscape]{foils}&#10;</xsl:text>
    <xsl:value-of select="$db2latex.slides.packages"/>
    <xsl:value-of select="$db2latex.slides.beforebegin"/>
<!-- Process SlidesInfo -->
    <xsl:apply-templates select="slidesinfo"/>
    <xsl:text>\begin{document}&#10;</xsl:text>
    <xsl:text>\maketitle&#10;</xsl:text>
<!-- Process Everything except SlidesInfo -->
    <xsl:apply-templates select="*[not(slidesinfo)]"/>
<!-- <xsl:apply-templates select="foil|foilgroup"/> -->
    <xsl:text>\end{document}&#10;</xsl:text>
    </xsl:template>


<xsl:template match="slidesinfo">
<xsl:text>%--------------------------------------------------SLIDES INFORMATION&#10;</xsl:text>
<xsl:text>\title{{\black </xsl:text>
<xsl:apply-templates select="title"/>
<xsl:text>}}&#10;</xsl:text>
<xsl:text>\author{{\black </xsl:text>
<xsl:apply-templates select="author|authorgroup"/>
<xsl:text>}}&#10;</xsl:text>
<xsl:text> </xsl:text>
</xsl:template>



<xsl:output method="text" encoding="ISO-8859-1" indent="yes"/>
<xsl:variable name="latex.use.babel">1</xsl:variable>
<xsl:variable name="latex.use.fancyvrb">1</xsl:variable>
<xsl:variable name="latex.use.fancybox">1</xsl:variable>
<xsl:variable name="latex.use.fancyhdr">1</xsl:variable>
<xsl:variable name="latex.use.subfigure">1</xsl:variable>
<xsl:variable name="latex.use.rotating">1</xsl:variable>
<xsl:variable name="latex.use.makeidx">1</xsl:variable>
<xsl:variable name="latex.pdf.support">1</xsl:variable>
<xsl:variable name="latex.math.support">1</xsl:variable>

<xsl:variable name="latex.biblio.output">all</xsl:variable>
<xsl:variable name="latex.document.font">default</xsl:variable>
</xsl:stylesheet>
