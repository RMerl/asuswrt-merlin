<?xml version="1.0" encoding="utf-8"?>
<!-- This file was generated automatically. -->
<!-- Developers should not commit sundry patches against this file. -->
<!-- The source file (with documentation!) is in the admin directory. -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
  <doc:reference xmlns:doc="http://nwalsh.com/xsl/documentation/1.0" id="scape">
    <referenceinfo>
      <releaseinfo role="meta">
        $Id: scape.mod.xsl,v 1.4 2004/01/23 11:36:06 j-devenish Exp $
      </releaseinfo>
      <authorgroup>
        <author>
          <firstname>James</firstname>
          <surname>Devenish</surname>
        </author>
      </authorgroup>
      <copyright>
        <year>2003</year>
        <year>2004</year>
        <holder>Ramon Casellas</holder>
      </copyright>
    </referenceinfo>
    <title><productname condition="noindex">LaTeX</productname> Active-character Escaping</title>
  </doc:reference>
  <doc:template xmlns:doc="http://nwalsh.com/xsl/documentation/1.0">
    <refpurpose>General text escaping for <productname condition="noindex">LaTeX</productname> active characters</refpurpose>
    <doc:description>
      <para>
        Certain characters must be escaped so that <productname condition="noindex">LaTeX</productname> will typeset
        them verbatim rather than attempting to interpret them as commands.
      </para>
    </doc:description>
    <refsection>
      <title>Mapping Source</title>
      <programlisting><![CDATA[<template name="scape">
    
               <map from="&lt;" to="\textless{}"/>
    
               <map from="&gt;" to="\textgreater{}"/>
    
               <map from="~" to="\textasciitilde{}"/>
    
               <map from="^" to="\textasciicircum{}"/>
    
               <map from="&amp;" to="\&amp;"/>
    
               <map from="#" to="\#"/>
    
               <map from="_" to="\_"/>
    
               <map from="$" to="\$"/>
    
               <map from="%" to="\%"/>
    
               <map from="|" to="\docbooktolatexpipe{}"/>
    
               <map from="{" to="\{"/>
    
               <map from="}" to="\}"/>
    
               <map from="\textbackslash  " to="\textbackslash \ "/>
    
               <map from="\" to="\textbackslash "/>
  
            </template>]]></programlisting>
    </refsection>
  </doc:template>
  <xsl:template name="scape">
    <xsl:param name="string"/>
    <xsl:call-template name="string-replace">
      <xsl:with-param name="from">&lt;</xsl:with-param>
      <xsl:with-param name="to">\textless{}</xsl:with-param>
      <xsl:with-param name="string">
        <xsl:call-template name="string-replace">
          <xsl:with-param name="from">&gt;</xsl:with-param>
          <xsl:with-param name="to">\textgreater{}</xsl:with-param>
          <xsl:with-param name="string">
            <xsl:call-template name="string-replace">
              <xsl:with-param name="from">~</xsl:with-param>
              <xsl:with-param name="to">\textasciitilde{}</xsl:with-param>
              <xsl:with-param name="string">
                <xsl:call-template name="string-replace">
                  <xsl:with-param name="from">^</xsl:with-param>
                  <xsl:with-param name="to">\textasciicircum{}</xsl:with-param>
                  <xsl:with-param name="string">
                    <xsl:call-template name="string-replace">
                      <xsl:with-param name="from">&amp;</xsl:with-param>
                      <xsl:with-param name="to">\&amp;</xsl:with-param>
                      <xsl:with-param name="string">
                        <xsl:call-template name="string-replace">
                          <xsl:with-param name="from">#</xsl:with-param>
                          <xsl:with-param name="to">\#</xsl:with-param>
                          <xsl:with-param name="string">
                            <xsl:call-template name="string-replace">
                              <xsl:with-param name="from">_</xsl:with-param>
                              <xsl:with-param name="to">\_</xsl:with-param>
                              <xsl:with-param name="string">
                                <xsl:call-template name="string-replace">
                                  <xsl:with-param name="from">$</xsl:with-param>
                                  <xsl:with-param name="to">\$</xsl:with-param>
                                  <xsl:with-param name="string">
                                    <xsl:call-template name="string-replace">
                                      <xsl:with-param name="from">%</xsl:with-param>
                                      <xsl:with-param name="to">\%</xsl:with-param>
                                      <xsl:with-param name="string">
                                        <xsl:call-template name="string-replace">
                                          <xsl:with-param name="from">|</xsl:with-param>
                                          <xsl:with-param name="to">\docbooktolatexpipe{}</xsl:with-param>
                                          <xsl:with-param name="string">
                                            <xsl:call-template name="string-replace">
                                              <xsl:with-param name="from">{</xsl:with-param>
                                              <xsl:with-param name="to">\{</xsl:with-param>
                                              <xsl:with-param name="string">
                                                <xsl:call-template name="string-replace">
                                                  <xsl:with-param name="from">}</xsl:with-param>
                                                  <xsl:with-param name="to">\}</xsl:with-param>
                                                  <xsl:with-param name="string">
                                                    <xsl:call-template name="string-replace">
                                                      <xsl:with-param name="from">\textbackslash  </xsl:with-param>
                                                      <xsl:with-param name="to">\textbackslash \ </xsl:with-param>
                                                      <xsl:with-param name="string">
                                                        <xsl:call-template name="string-replace">
                                                          <xsl:with-param name="from">\</xsl:with-param>
                                                          <xsl:with-param name="to">\textbackslash </xsl:with-param>
                                                          <xsl:with-param name="string" select="$string"/>
                                                        </xsl:call-template>
                                                      </xsl:with-param>
                                                    </xsl:call-template>
                                                  </xsl:with-param>
                                                </xsl:call-template>
                                              </xsl:with-param>
                                            </xsl:call-template>
                                          </xsl:with-param>
                                        </xsl:call-template>
                                      </xsl:with-param>
                                    </xsl:call-template>
                                  </xsl:with-param>
                                </xsl:call-template>
                              </xsl:with-param>
                            </xsl:call-template>
                          </xsl:with-param>
                        </xsl:call-template>
                      </xsl:with-param>
                    </xsl:call-template>
                  </xsl:with-param>
                </xsl:call-template>
              </xsl:with-param>
            </xsl:call-template>
          </xsl:with-param>
        </xsl:call-template>
      </xsl:with-param>
    </xsl:call-template>
  </xsl:template>
  <doc:template xmlns:doc="http://nwalsh.com/xsl/documentation/1.0">
    <refpurpose>Escape characters for use with <function condition="latex">index</function> 
         <productname condition="noindex">LaTeX</productname> command</refpurpose>
    <doc:description>
      <para>
        In addition to the characters from <xref linkend="template.scape"/>,
        certain extra characters must be escaped so that <productname condition="noindex">LaTeX</productname> will not treat
        them as indexing directives.
      </para>
    </doc:description>
    <refsection>
      <title>Mapping Source</title>
      <programlisting><![CDATA[<template name="scape-indexterm">
    
               <map from="!" to="&#34;!"/>
    
               <map from="|" to="\ensuremath{&#34;|}"/>
    
               <map from="@" to="&#34;@"/>
    
               <map from="&lt;" to="\textless{}"/>
    
               <map from="&gt;" to="\textgreater{}"/>
    
               <map from="~" to="\textasciitilde{}"/>
    
               <map from="^" to="\textasciicircum{}"/>
    
               <map from="&amp;" to="\&amp;"/>
    
               <map from="#" to="\#"/>
    
               <map from="_" to="\_"/>
    
               <map from="$" to="\$"/>
    
               <map from="%" to="\%"/>
    
               <map from="\}" to="\textbraceright{}"/>
    
               <map from="{" to="\textbraceleft{}"/>
    
               <map from="}" to="\}"/>
    
               <map from="&#34;" to="&#34;&#34;"/>
    
               <map from="\textbackslash  " to="\textbackslash \ "/>
    
               <map from="\" to="\textbackslash "/>
  
            </template>]]></programlisting>
    </refsection>
  </doc:template>
  <xsl:template name="scape-indexterm">
    <xsl:param name="string"/>
    <xsl:call-template name="string-replace">
      <xsl:with-param name="from">!</xsl:with-param>
      <xsl:with-param name="to">"!</xsl:with-param>
      <xsl:with-param name="string">
        <xsl:call-template name="string-replace">
          <xsl:with-param name="from">|</xsl:with-param>
          <xsl:with-param name="to">\ensuremath{"|}</xsl:with-param>
          <xsl:with-param name="string">
            <xsl:call-template name="string-replace">
              <xsl:with-param name="from">@</xsl:with-param>
              <xsl:with-param name="to">"@</xsl:with-param>
              <xsl:with-param name="string">
                <xsl:call-template name="string-replace">
                  <xsl:with-param name="from">&lt;</xsl:with-param>
                  <xsl:with-param name="to">\textless{}</xsl:with-param>
                  <xsl:with-param name="string">
                    <xsl:call-template name="string-replace">
                      <xsl:with-param name="from">&gt;</xsl:with-param>
                      <xsl:with-param name="to">\textgreater{}</xsl:with-param>
                      <xsl:with-param name="string">
                        <xsl:call-template name="string-replace">
                          <xsl:with-param name="from">~</xsl:with-param>
                          <xsl:with-param name="to">\textasciitilde{}</xsl:with-param>
                          <xsl:with-param name="string">
                            <xsl:call-template name="string-replace">
                              <xsl:with-param name="from">^</xsl:with-param>
                              <xsl:with-param name="to">\textasciicircum{}</xsl:with-param>
                              <xsl:with-param name="string">
                                <xsl:call-template name="string-replace">
                                  <xsl:with-param name="from">&amp;</xsl:with-param>
                                  <xsl:with-param name="to">\&amp;</xsl:with-param>
                                  <xsl:with-param name="string">
                                    <xsl:call-template name="string-replace">
                                      <xsl:with-param name="from">#</xsl:with-param>
                                      <xsl:with-param name="to">\#</xsl:with-param>
                                      <xsl:with-param name="string">
                                        <xsl:call-template name="string-replace">
                                          <xsl:with-param name="from">_</xsl:with-param>
                                          <xsl:with-param name="to">\_</xsl:with-param>
                                          <xsl:with-param name="string">
                                            <xsl:call-template name="string-replace">
                                              <xsl:with-param name="from">$</xsl:with-param>
                                              <xsl:with-param name="to">\$</xsl:with-param>
                                              <xsl:with-param name="string">
                                                <xsl:call-template name="string-replace">
                                                  <xsl:with-param name="from">%</xsl:with-param>
                                                  <xsl:with-param name="to">\%</xsl:with-param>
                                                  <xsl:with-param name="string">
                                                    <xsl:call-template name="string-replace">
                                                      <xsl:with-param name="from">\}</xsl:with-param>
                                                      <xsl:with-param name="to">\textbraceright{}</xsl:with-param>
                                                      <xsl:with-param name="string">
                                                        <xsl:call-template name="string-replace">
                                                          <xsl:with-param name="from">{</xsl:with-param>
                                                          <xsl:with-param name="to">\textbraceleft{}</xsl:with-param>
                                                          <xsl:with-param name="string">
                                                            <xsl:call-template name="string-replace">
                                                              <xsl:with-param name="from">}</xsl:with-param>
                                                              <xsl:with-param name="to">\}</xsl:with-param>
                                                              <xsl:with-param name="string">
                                                                <xsl:call-template name="string-replace">
                                                                  <xsl:with-param name="from">"</xsl:with-param>
                                                                  <xsl:with-param name="to">""</xsl:with-param>
                                                                  <xsl:with-param name="string">
                                                                    <xsl:call-template name="string-replace">
                                                                      <xsl:with-param name="from">\textbackslash  </xsl:with-param>
                                                                      <xsl:with-param name="to">\textbackslash \ </xsl:with-param>
                                                                      <xsl:with-param name="string">
                                                                        <xsl:call-template name="string-replace">
                                                                          <xsl:with-param name="from">\</xsl:with-param>
                                                                          <xsl:with-param name="to">\textbackslash </xsl:with-param>
                                                                          <xsl:with-param name="string" select="$string"/>
                                                                        </xsl:call-template>
                                                                      </xsl:with-param>
                                                                    </xsl:call-template>
                                                                  </xsl:with-param>
                                                                </xsl:call-template>
                                                              </xsl:with-param>
                                                            </xsl:call-template>
                                                          </xsl:with-param>
                                                        </xsl:call-template>
                                                      </xsl:with-param>
                                                    </xsl:call-template>
                                                  </xsl:with-param>
                                                </xsl:call-template>
                                              </xsl:with-param>
                                            </xsl:call-template>
                                          </xsl:with-param>
                                        </xsl:call-template>
                                      </xsl:with-param>
                                    </xsl:call-template>
                                  </xsl:with-param>
                                </xsl:call-template>
                              </xsl:with-param>
                            </xsl:call-template>
                          </xsl:with-param>
                        </xsl:call-template>
                      </xsl:with-param>
                    </xsl:call-template>
                  </xsl:with-param>
                </xsl:call-template>
              </xsl:with-param>
            </xsl:call-template>
          </xsl:with-param>
        </xsl:call-template>
      </xsl:with-param>
    </xsl:call-template>
  </xsl:template>
  <doc:template xmlns:doc="http://nwalsh.com/xsl/documentation/1.0">
    <refpurpose>Verbatim-text escaping for <productname condition="noindex">LaTeX</productname> active characters</refpurpose>
    <refsection>
      <title>Mapping Source</title>
      <programlisting><![CDATA[<template name="scape-verbatim">
    
               <map from="~" to="\textasciitilde{}"/>
    
               <map from="^" to="\textasciicircum{}"/>
    
               <map from="&amp;" to="\&amp;"/>
    
               <map from="#" to="\#"/>
    
               <map from="_" to="\_\dbz{}"/>
    
               <map from="$" to="\$"/>
    
               <map from="%" to="\%"/>
    
               <map from="/" to="/\dbz{}"/>
    
               <map from="-" to="-\dbz{}"/>
    
               <map from="+" to="+\dbz{}"/>
    
               <map from="." to=".\dbz{}"/>
    
               <map from="(" to="(\dbz{}"/>
    
               <map from=")" to=")\dbz{}"/>
    
               <map from="{" to="\docbooktolatexgobble\string\{"/>
    
               <map from="}" to="\docbooktolatexgobble\string\}"/>
    
               <map from="\" to="\docbooktolatexgobble\string\\"/>
  
            </template>]]></programlisting>
    </refsection>
  </doc:template>
  <xsl:template name="scape-verbatim">
    <xsl:param name="string"/>
    <xsl:call-template name="string-replace">
      <xsl:with-param name="from">~</xsl:with-param>
      <xsl:with-param name="to">\textasciitilde{}</xsl:with-param>
      <xsl:with-param name="string">
        <xsl:call-template name="string-replace">
          <xsl:with-param name="from">^</xsl:with-param>
          <xsl:with-param name="to">\textasciicircum{}</xsl:with-param>
          <xsl:with-param name="string">
            <xsl:call-template name="string-replace">
              <xsl:with-param name="from">&amp;</xsl:with-param>
              <xsl:with-param name="to">\&amp;</xsl:with-param>
              <xsl:with-param name="string">
                <xsl:call-template name="string-replace">
                  <xsl:with-param name="from">#</xsl:with-param>
                  <xsl:with-param name="to">\#</xsl:with-param>
                  <xsl:with-param name="string">
                    <xsl:call-template name="string-replace">
                      <xsl:with-param name="from">_</xsl:with-param>
                      <xsl:with-param name="to">\_\dbz{}</xsl:with-param>
                      <xsl:with-param name="string">
                        <xsl:call-template name="string-replace">
                          <xsl:with-param name="from">$</xsl:with-param>
                          <xsl:with-param name="to">\$</xsl:with-param>
                          <xsl:with-param name="string">
                            <xsl:call-template name="string-replace">
                              <xsl:with-param name="from">%</xsl:with-param>
                              <xsl:with-param name="to">\%</xsl:with-param>
                              <xsl:with-param name="string">
                                <xsl:call-template name="string-replace">
                                  <xsl:with-param name="from">/</xsl:with-param>
                                  <xsl:with-param name="to">/\dbz{}</xsl:with-param>
                                  <xsl:with-param name="string">
                                    <xsl:call-template name="string-replace">
                                      <xsl:with-param name="from">-</xsl:with-param>
                                      <xsl:with-param name="to">-\dbz{}</xsl:with-param>
                                      <xsl:with-param name="string">
                                        <xsl:call-template name="string-replace">
                                          <xsl:with-param name="from">+</xsl:with-param>
                                          <xsl:with-param name="to">+\dbz{}</xsl:with-param>
                                          <xsl:with-param name="string">
                                            <xsl:call-template name="string-replace">
                                              <xsl:with-param name="from">.</xsl:with-param>
                                              <xsl:with-param name="to">.\dbz{}</xsl:with-param>
                                              <xsl:with-param name="string">
                                                <xsl:call-template name="string-replace">
                                                  <xsl:with-param name="from">(</xsl:with-param>
                                                  <xsl:with-param name="to">(\dbz{}</xsl:with-param>
                                                  <xsl:with-param name="string">
                                                    <xsl:call-template name="string-replace">
                                                      <xsl:with-param name="from">)</xsl:with-param>
                                                      <xsl:with-param name="to">)\dbz{}</xsl:with-param>
                                                      <xsl:with-param name="string">
                                                        <xsl:call-template name="string-replace">
                                                          <xsl:with-param name="from">{</xsl:with-param>
                                                          <xsl:with-param name="to">\docbooktolatexgobble\string\{</xsl:with-param>
                                                          <xsl:with-param name="string">
                                                            <xsl:call-template name="string-replace">
                                                              <xsl:with-param name="from">}</xsl:with-param>
                                                              <xsl:with-param name="to">\docbooktolatexgobble\string\}</xsl:with-param>
                                                              <xsl:with-param name="string">
                                                                <xsl:call-template name="string-replace">
                                                                  <xsl:with-param name="from">\</xsl:with-param>
                                                                  <xsl:with-param name="to">\docbooktolatexgobble\string\\</xsl:with-param>
                                                                  <xsl:with-param name="string" select="$string"/>
                                                                </xsl:call-template>
                                                              </xsl:with-param>
                                                            </xsl:call-template>
                                                          </xsl:with-param>
                                                        </xsl:call-template>
                                                      </xsl:with-param>
                                                    </xsl:call-template>
                                                  </xsl:with-param>
                                                </xsl:call-template>
                                              </xsl:with-param>
                                            </xsl:call-template>
                                          </xsl:with-param>
                                        </xsl:call-template>
                                      </xsl:with-param>
                                    </xsl:call-template>
                                  </xsl:with-param>
                                </xsl:call-template>
                              </xsl:with-param>
                            </xsl:call-template>
                          </xsl:with-param>
                        </xsl:call-template>
                      </xsl:with-param>
                    </xsl:call-template>
                  </xsl:with-param>
                </xsl:call-template>
              </xsl:with-param>
            </xsl:call-template>
          </xsl:with-param>
        </xsl:call-template>
      </xsl:with-param>
    </xsl:call-template>
  </xsl:template>
  <doc:template xmlns:doc="http://nwalsh.com/xsl/documentation/1.0">
    <refpurpose>Escape characters for use with the <productname>hyperref</productname> 
         <productname condition="noindex">LaTeX</productname> package</refpurpose>
    <refsection>
      <title>Mapping Source</title>
      <programlisting><![CDATA[<template name="scape-href">
    
               <map from="&amp;" to="\&amp;"/>
    
               <map from="%" to="\%"/>
    
               <map from="[" to="\["/>
    
               <map from="]" to="\]"/>
    
               <map from="{" to="\{"/>
    
               <map from="}" to="\}"/>
    
               <map from="\" to="\docbooktolatexgobble\string\\"/>
  
            </template>]]></programlisting>
    </refsection>
  </doc:template>
  <xsl:template name="scape-href">
    <xsl:param name="string"/>
    <xsl:call-template name="string-replace">
      <xsl:with-param name="from">&amp;</xsl:with-param>
      <xsl:with-param name="to">\&amp;</xsl:with-param>
      <xsl:with-param name="string">
        <xsl:call-template name="string-replace">
          <xsl:with-param name="from">%</xsl:with-param>
          <xsl:with-param name="to">\%</xsl:with-param>
          <xsl:with-param name="string">
            <xsl:call-template name="string-replace">
              <xsl:with-param name="from">[</xsl:with-param>
              <xsl:with-param name="to">\[</xsl:with-param>
              <xsl:with-param name="string">
                <xsl:call-template name="string-replace">
                  <xsl:with-param name="from">]</xsl:with-param>
                  <xsl:with-param name="to">\]</xsl:with-param>
                  <xsl:with-param name="string">
                    <xsl:call-template name="string-replace">
                      <xsl:with-param name="from">{</xsl:with-param>
                      <xsl:with-param name="to">\{</xsl:with-param>
                      <xsl:with-param name="string">
                        <xsl:call-template name="string-replace">
                          <xsl:with-param name="from">}</xsl:with-param>
                          <xsl:with-param name="to">\}</xsl:with-param>
                          <xsl:with-param name="string">
                            <xsl:call-template name="string-replace">
                              <xsl:with-param name="from">\</xsl:with-param>
                              <xsl:with-param name="to">\docbooktolatexgobble\string\\</xsl:with-param>
                              <xsl:with-param name="string" select="$string"/>
                            </xsl:call-template>
                          </xsl:with-param>
                        </xsl:call-template>
                      </xsl:with-param>
                    </xsl:call-template>
                  </xsl:with-param>
                </xsl:call-template>
              </xsl:with-param>
            </xsl:call-template>
          </xsl:with-param>
        </xsl:call-template>
      </xsl:with-param>
    </xsl:call-template>
  </xsl:template>
  <doc:template xmlns:doc="http://nwalsh.com/xsl/documentation/1.0">
    <refpurpose>Escape characters for use with the <productname>url</productname> 
         <productname condition="noindex">LaTeX</productname> package</refpurpose>
    <refsection>
      <title>Mapping Source</title>
      <programlisting><![CDATA[<template name="scape-url">
    
               <map from="&amp;" to="\string&amp;"/>
    
               <map from="%" to="\%"/>
    
               <map from="{" to="\{"/>
    
               <map from="}" to="\}"/>
    
               <map from="\" to="\docbooktolatexgobble\string\\"/>
  
            </template>]]></programlisting>
    </refsection>
  </doc:template>
  <xsl:template name="scape-url">
    <xsl:param name="string"/>
    <xsl:call-template name="string-replace">
      <xsl:with-param name="from">&amp;</xsl:with-param>
      <xsl:with-param name="to">\string&amp;</xsl:with-param>
      <xsl:with-param name="string">
        <xsl:call-template name="string-replace">
          <xsl:with-param name="from">%</xsl:with-param>
          <xsl:with-param name="to">\%</xsl:with-param>
          <xsl:with-param name="string">
            <xsl:call-template name="string-replace">
              <xsl:with-param name="from">{</xsl:with-param>
              <xsl:with-param name="to">\{</xsl:with-param>
              <xsl:with-param name="string">
                <xsl:call-template name="string-replace">
                  <xsl:with-param name="from">}</xsl:with-param>
                  <xsl:with-param name="to">\}</xsl:with-param>
                  <xsl:with-param name="string">
                    <xsl:call-template name="string-replace">
                      <xsl:with-param name="from">\</xsl:with-param>
                      <xsl:with-param name="to">\docbooktolatexgobble\string\\</xsl:with-param>
                      <xsl:with-param name="string" select="$string"/>
                    </xsl:call-template>
                  </xsl:with-param>
                </xsl:call-template>
              </xsl:with-param>
            </xsl:call-template>
          </xsl:with-param>
        </xsl:call-template>
      </xsl:with-param>
    </xsl:call-template>
  </xsl:template>
  <doc:template xmlns:doc="http://nwalsh.com/xsl/documentation/1.0">
    <refpurpose>Escape the ] character in <productname condition="noindex">LaTeX</productname> optional arguments (experimental)</refpurpose>
    <refsection>
      <title>Mapping Source</title>
      <programlisting><![CDATA[<template name="scape-optionalarg">
    
               <map from="]" to="{\rbrack}"/>
  
            </template>]]></programlisting>
    </refsection>
  </doc:template>
  <xsl:template name="scape-optionalarg">
    <xsl:param name="string"/>
    <xsl:call-template name="string-replace">
      <xsl:with-param name="from">]</xsl:with-param>
      <xsl:with-param name="to">{\rbrack}</xsl:with-param>
      <xsl:with-param name="string" select="$string"/>
    </xsl:call-template>
  </xsl:template>
  <doc:template xmlns:doc="http://nwalsh.com/xsl/documentation/1.0">
    <refpurpose>Basic line-breaking for verbatim text</refpurpose>
    <doc:description>
      <para>
        Allow line breaking after certain characters.
        Text should be escaped with the <xref linkend="template.scape"/>
        template before being passed to this template.
      </para>
    </doc:description>
    <refsection>
      <title>Mapping Source</title>
      <programlisting><![CDATA[<template name="scape-slash">
    
               <map from="." to=".\dbz{}"/>
    
               <map from="/" to="/\dbz{}"/>
  
            </template>]]></programlisting>
    </refsection>
  </doc:template>
  <xsl:template name="scape-slash">
    <xsl:param name="string"/>
    <xsl:call-template name="string-replace">
      <xsl:with-param name="from">.</xsl:with-param>
      <xsl:with-param name="to">.\dbz{}</xsl:with-param>
      <xsl:with-param name="string">
        <xsl:call-template name="string-replace">
          <xsl:with-param name="from">/</xsl:with-param>
          <xsl:with-param name="to">/\dbz{}</xsl:with-param>
          <xsl:with-param name="string" select="$string"/>
        </xsl:call-template>
      </xsl:with-param>
    </xsl:call-template>
  </xsl:template>
</xsl:stylesheet>
