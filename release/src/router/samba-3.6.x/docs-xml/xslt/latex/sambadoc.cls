% $Id$
% phstyle.cls -- Macros for formatting Prentice-Hall Books
%               -- Compiled by J.K. Shultis, Dec. 1993
% 		-- Modified and enhanced for LaTeX2e mdh/rlm 1997/02
%       -- Style Modifications Lori Hughes June 2002 
%       -- Modifications for Samba by Jelmer Vernooij 2003-2005

\NeedsTeXFormat{LaTeX2e}%
\ProvidesClass{sambadoc}[2005/06/13 Samba Documentation class]
\newif\if@ph
\@phfalse
\DeclareOption{ph}{ \@phtrue }
\DeclareOption*{\PassOptionsToClass{\CurrentOption}{report}}
\ProcessOptions%

%  Base everything on the default class 'report'.
\LoadClass{report}%
%================================================================================
% 1. Page layout dimensions
% 3. Define new fonts for headings
% 4. Redefines the way "figure" amd "table" are formated in a \caption
% 5. Defines a \capt macro which defines (1) \captwidth, (2) \captsize
%    and \beforetableskip.
% 6. Defines and modifies FANCYHDS.STY for headers and foots
% 7. Change sectioning headings to bold sans serif fonts
% 8. Turn off leader dots in Table of Contents
%======================================================================

%====================== Page Layout
% \oddsidemargin 0.750in   \evensidemargin 0.750in  \textwidth 5.00in
% \topmargin     0.625in    \headheight     0.3in    \headsep  0.2in
% \textheight    7.3in     %\footheight .2in        
% \footskip      0.3in
% \parskip       0ex       \marginparwidth 1.00in  \marginparsep 0.5in
%\setlength\arraycolsep{1pt}

\if@ph
	\RequirePackage{geometry}
	\geometry{
	twoside,
	openleft,
	papersize={7in,9.25in},
	lmargin=.75in,
	rmargin=.75in,
	bmargin=.625in,
	tmargin=.825in,
	width=5.5in,
	height=7.325in, 
	marginparwidth=0.35in,
	headheight=0.2in,
	textwidth=5in,
	headsep=.25in,
	footskip=.025in}
	%\usepackage[letter,cam,cross,center]{crop}
\fi

%====================== Define special fonts
\newfont{\BIG}{cminch}                             %--- One-inch font
\newfont{\sfbHuge}{cmssbx10 scaled\magstep5}       %-- 25pt sans serif bold
\newfont{\sfblarge}{cmssbx10 scaled\magstep1}      %-- 12pt sans serif bold
\newfont{\sfbeleven}{cmssbx10 scaled\magstephalf}  %-- 11pt sans serif bold
\newfont{\sfb}{cmssbx10}                           %-- 10pt sans serif bold
\newfont{\sfeight}{cmss8}                          %-- 8pt sans serif
\newfont{\sfnine}{cmss8 at 9pt}                     %-- 9pt sans serif

\newfont{\defnem}{cmbxti10}                    %-- 10pt sans serif bold
\def\spacingset#1{\def\baselinestretch{#1}\small\normalsize}

%-----------Change section heading font sizes here----
\newfont{\sfbsection}{cmssbx10 at 13pt}
\newfont{\sfbsubsec}{cmssbx10 at 12pt}
\newfont{\sfbsubsub}{cmssbx10 at 11pt}
\newfont{\sfbpara}{cmssbx10}
%---------------------------------------------------


%
% Theorem hack
%

\def\@begintheorem#1#2{\trivlist
   \item[\hskip \labelsep{\bfseries #1\ #2:}]\itshape}

%===================== Redefine the \@makecaption command
   \long\def\@makecaption#1#2{\vskip 10\p@
%% \setbox\@tempboxa\hbox{#1: #2}%         %% original label definition
   \setbox\@tempboxa\hbox{\textbf{ \small #1.} \small #2}%   %% bold and . instead of :
   \ifdim \wd\@tempboxa >\hsize
%%     #1: #2\par                   %% original definition
       \textbf{ \small #1.} \small #2\par             %% bold and . instead of :
   \else
    \hbox to\hsize{\hfil\box\@tempboxa\hfil}%   %% original
%%      \hbox to\hsize{\box\@tempboxa\hfil}%             %% no centering
   \fi}

%===================== New caption macro
\newlength{\captsize}         \let\captsize=\footnotesize
\newlength{\captwidth}        \setlength{\captwidth}{\textwidth}
\newlength{\beforetableskip}  \setlength{\beforetableskip}{.5\baselineskip}
\newcommand{\capt}[1]{\begin{minipage}{\captwidth}
          \let\normalsize=\captsize
          \caption[#1]{#1}
          \end{minipage}\\ \vspace{\beforetableskip}}
		  

%---------- Change Figure Label
\newcommand{\thefigurename}{Figure}   %-- set default: Figure
\def\fnum@figure{\thefigurename\ \thefigure}

%       This is to get the equation numbers in the form (section.equation),
%       and to have the equation counter reset every time the section 
%       counter is incremented.

\def\theequation{\thechapter.\arabic{equation}}
\@addtoreset{equation}{chapter}

\RequirePackage{fancyhdr}

%------------------- Modify Headers for Prentice-Hall Book Style
%---------- Setup headers and footers
\pagestyle{fancyplain}

%--- Redefine Headers
\renewcommand{\sectionmark}[1]{\markright{\thesection\ #1}}
%
\lhead[\fancyplain{}{\bf\thepage}]{\fancyplain{}{\sfnine\rightmark}}
%
%\rhead[\fancyplain{}{\footnotesize\sf\CHHEAD}]{\fancyplain{}{\bf\thepage}}
 \rhead[\fancyplain{}{\sfnine \leftmark}]{\fancyplain{}{\bf\thepage}}
%--- Empty footers on every page (except initial chapter page which has
%--- page number) and optionally a small date stamp
\cfoot[\fancyplain{}{}]{\fancyplain{}{}}
\rfoot[\fancyplain{}{}]{\fancyplain{\hspace{\fill}\\ \bf\thepage}{}}
\lfoot[\fancyplain{\hspace{\fill}\\ \bf\thepage}{}]{\fancyplain{}{}}
%-- replace the two lines above with the two below for a date stamp footer
%\rfoot[\fancyplain{\scriptsize \today}{\scriptsize \today}]{\fancyplain{\bf\thepage}{}}
%\lfoot[\fancyplain{\bf\thepage}{}]{\fancyplain{\scriptsize \today}{\scriptsize \today}}
%--- Change format of section header
%
% rlm added chaptermark 10/11/96
\def\sectionmark#1{\markright{\ifnum \c@secnumdepth >\z@
  {\sfeight Section}~\thesection.\quad \fi #1}}
\def\sectionsmark#1{\markright{#1}{#1}}
%
\def\chaptermark#1{\markboth{#1 \quad \@chapapp{}~\thechapter}{}}
\def\chaptersmark#1{\markboth{#1}{#1}}


%===================== Change Section Heads to Bold Sans Serif
% Create bolds sans serif sectioning headings.  Also the headings have
% been reduced in size with less space before and after the heading.
% Modified from macros given in BK10.STY
%
%----------------------------- part ---------------------------------
\def\@part[#1]#2{%
	\thispagestyle{empty}
	\ifodd\c@page\hbox{}\else\clearpage\fi
	\addtocounter{page}{-2}
	\thispagestyle{empty}
	\ifnum \c@secnumdepth >-2\relax
		\refstepcounter{part}%
		\addcontentsline{toc}{part}{Part \thepart\hspace{1em}#1}%
	\else
		\addcontentsline{toc}{part}{Part #1}%
	\fi
	{\centering
		\interlinepenalty \@M
			\normalfont
			\ifnum \c@secnumdepth >-2\relax
				\huge\bfseries \partname\nobreakspace\thepart
				\par
				\vskip 20\p@
			\fi
			\Huge \bfseries #2\par}%
	\@endpart}

%------------------------- appendix -----------------------------------

\let\oldappendix\appendix
\def\appendix{\oldappendix\def\addchaptertoloe{}}

%------------------------- chapter -----------------------------------
\def\addchaptertoloe{%
	\addtocontents{loe}{\protect\addvspace{14pt}}%
	\addtocontents{loe}{\noindent {\bfseries Chapter~\thechapter}\protect\par}%
	\addtocontents{loe}{\protect\addvspace{10pt}}%
}

\def\chapter{\clearpage \thispagestyle{plain} \global\@topnum\z@
\@afterindentfalse \secdef\@chapter\@schapter} 
\def\@chapter[#1]#2{\ifnum \c@secnumdepth >\m@ne
 \refstepcounter{chapter}
 \typeout{\@chapapp\space\thechapter.}
 \addcontentsline{toc}{chapter}{Chapter~\protect
 \numberline{\thechapter}\uppercase{#1}}\else
 \addcontentsline{toc}{chapter}{Chapter~\uppercase{#1}}\fi
 \chaptermark{#1}
 \addchaptertoloe
 \addtocontents{lof}{\protect\addvspace{10pt}}
\addtocontents{lot}{\protect\addvspace{10pt}} \if@twocolumn
\@topnewpage[\@makechapterhead{#2}] 
 \else \@makechapterhead{#2}
 \@afterheading \fi} 
%
\def\@schapter#1{\if@twocolumn \@topnewpage[\@makeschapterhead{#1}]
 \else \@makeschapterhead{#1}\chaptersmark{#1}
 \@afterheading\fi\addcontentsline{toc}{chapter}{\uppercase{#1}}}



\def\@makechapterhead#1{\vspace*{-20pt}{\parindent0pt\raggedright
\ifnum\c@secnumdepth>\m@ne\LARGE\sf\@chapapp{}~\thechapter\par
\vskip3pt\hrule\vskip3pt\hrule
\vskip30pt\fi\raggedright\Huge\sfbHuge\uppercase{#1}\par
\nobreak\vskip70pt}}%\chaptermark{#1}}

\def\@makeschapterhead#1{\vspace*{4pt}{\parindent0pt
\vskip3pt\hrule\vskip3pt\hrule
\vskip36pt\raggedright\Huge\sfbHuge\uppercase{#1}\par
\nobreak\vskip70pt}}%\chaptersmark{#1}}
\widowpenalty=1000
\clubpenalty=1000

%------------------------ section -----------------------------------
\def\section{\@startsection{section}{1}{\z@}{-3.5ex plus-1ex
  minus -.2ex}{1.8ex}{\sfbsection}}

%--------------------- subsection -----------------------------------
\def\subsection{\@startsection{subsection}{2}{\z@}{-2.75ex plus -1ex
    minus -.2ex}{1.2ex}{\sfbsubsec}}

%-------------------- subsubsection --------------------------------
\def\subsubsection{\@startsection{subsubsection}{3}{\z@}{-2.75ex
  plus -1ex minus -.2ex}{1.2ex}{\sfbsubsub}}

%----------------------- paragraph ----------------------------------
\def\paragraph{\@startsection{paragraph}{4}{\z@}{1.25ex plus
     1ex minus .2ex}{-1em}{\sfbpara}}

%---------------------- subparagraph --------------------------------
\def\subparagraph{\@startsection{subparagraph}{4}{\parindent}{1.25ex
     plus 1ex minus .2ex}{-1em}{\normalsize\sfb}}



%====================== Turn off leader dots in Table of Contents
\renewcommand{\@dotsep}{500}

\def\tableofcontents{\markboth{}{}\@restonecolfalse\if@twocolumn\@restonecoltrue\onecolumn
 \fi\thispagestyle{plain} \@makeschapterhead{Contents} 
 \@afterheading
 \chaptersmark{Contents}
 \sectionsmark{Contents}
 \@starttoc{toc}\if@restonecol\twocolumn\fi}


\def\thebibliography#1{\chapter*{Bibliography}\list
% \addcontentsline{toc}{chapter}{Bibliography}
 {[\arabic{enumi}]}{\settowidth\labelwidth{2em}\leftmargin\labelwidth
% \advance\leftmargin\labelsep
 \usecounter{enumi}}
 \def\newblock{\hskip .11em plus .33em minus .07em}
 \sloppy\clubpenalty4000\widowpenalty4000
 \sfcode`\.=1000\relax}
\let\endthebibliography=\endlist


%% BIBLIOGRAPHY
%
%% separate citations with "], ["
\def\@citex[#1]#2{\if@filesw\immediate\write\@auxout{\string\citation{#2}}\fi
\def\@citea{}\@cite{\@for\@citeb:=#2\do {\@citea\def\@citea{],
[}\@ifundefined {b@\@citeb}{{\bf ?}\@warning {Citation `\@citeb' on
page \thepage \space undefined}}%
{\csname b@\@citeb\endcsname}}}{#1}}
 
\def\@cite#1#2{[{#1\if@tempswa, #2\fi}]}


%% index

\def\theindex{\cleardoublepage\@restonecoltrue\if@twocolumn\@restonecolfalse\fi
\columnseprule \z@
\columnsep 35pt\twocolumn[\@makeschapterhead{Subject Index}]
  \chaptersmark{Subject Index}%
 \addcontentsline{toc}{chapter}{SUBJECT INDEX}
 \thispagestyle{plain}\parindent\z@
 \parskip\z@ plus .3pt\relax\let\item\@idxitem}


\renewcommand{\cleardoublepage}{\clearpage\if@twoside \ifodd\c@page\else
\hbox{}\thispagestyle{empty}\newpage\if@twocolumn\hbox{}\newpage\fi\fi\fi}

\renewcommand{\numberline}[1]{\sbox\@tempboxa{#1\hspace{2pt}}%
	                \ifdim\wd\@tempboxa>\@tempdima \else \wd\@tempboxa\@tempdima\fi
						                           \box\@tempboxa}

% Custom list of examples
\def\listofexamples{\@restonecolfalse\if@twocolumn\@restonecoltrue\onecolumn
  \fi\chapter*{List of Examples}
  \renewcommand\@pnumwidth{0em}
  \@starttoc{loe}\if@restonecol
  \twocolumn\fi}

\def\l@example{\noindent\@dottedtocline{1}{0em}{3.5em}}

\renewcommand\listfigurename{\uppercase{List Of Figures}}
\renewcommand\listtablename{\uppercase{List Of Tables}}

\setlength{\emergencystretch}{0cm}


\cleardoublepage
\pagenumbering{roman}

\setcounter{page}{2}
\setcounter{totalnumber}{8}
\setcounter{bottomnumber}{3}
\setcounter{topnumber}{3}
\renewcommand{\textfraction}{0.1}
\renewcommand{\topfraction}{1.0}
\renewcommand{\bottomfraction}{1.0}

\RequirePackage{samba}
