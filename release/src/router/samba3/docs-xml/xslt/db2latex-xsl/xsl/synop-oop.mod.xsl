<?xml version='1.0'?>
<!DOCTYPE xsl:stylesheet [ <!ENTITY RE "&#10;"> ]>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version='1.0'>
    <!--############################################################################# 
    |	$Id: synop-oop.mod.xsl,v 1.1.1.1 2003/03/14 10:42:53 rcasellas Exp $
    |- #############################################################################
    |	$Author: rcasellas $
    |														
    |   PURPOSE:
    + ############################################################################## -->



    <xsl:template match="classsynopsis">
	<!-- PARAM language : -->
	<xsl:param name="language">
	    <xsl:choose>
		<xsl:when test="@language">
		    <xsl:value-of select="@language"/>
		</xsl:when>
		<xsl:otherwise>
		    <xsl:value-of select="$default-classsynopsis-language"/>
		</xsl:otherwise>
	    </xsl:choose>
	</xsl:param>

	<xsl:choose>
	    <xsl:when test="$language='java'"> <xsl:apply-templates select="." mode="java"/> </xsl:when>
	    <xsl:when test="$language='perl'"> <xsl:apply-templates select="." mode="perl"/> </xsl:when>
	    <xsl:when test="$language='idl'">  <xsl:apply-templates select="." mode="idl"/>  </xsl:when>
	    <xsl:when test="$language='cpp'">  <xsl:apply-templates select="." mode="cpp"/>  </xsl:when>
	    <xsl:otherwise>
		<xsl:message>Unrecognized language on classsynopsis: <xsl:value-of select="$language"/> </xsl:message>
		<xsl:apply-templates select=".">
		    <xsl:with-param name="language"  select="$default-classsynopsis-language"/>
		</xsl:apply-templates>
	    </xsl:otherwise>
	</xsl:choose>
    </xsl:template>





    <!-- ===== Java ======================================================== -->

    <xsl:template match="classsynopsis" mode="java">
	<pre class="{name(.)}">
	    <xsl:apply-templates select="ooclass[1]" mode="java"/>
	    <xsl:if test="ooclass[position() &gt; 1]">
		<xsl:text> extends</xsl:text>
		<xsl:apply-templates select="ooclass[position() &gt; 1]" mode="java"/>
		<xsl:if test="oointerface|ooexception">
		    <xsl:text>&RE;    </xsl:text>
		</xsl:if>
	    </xsl:if>
	    <xsl:if test="oointerface">
		<xsl:text>implements</xsl:text>
		<xsl:apply-templates select="oointerface" mode="java"/>
		<xsl:if test="ooexception">
		    <xsl:text>&RE;    </xsl:text>
		</xsl:if>
	    </xsl:if>
	    <xsl:if test="ooexception">
		<xsl:text>throws</xsl:text>
		<xsl:apply-templates select="ooexception" mode="java"/>
	    </xsl:if>
	    <xsl:text> {&RE;&RE;</xsl:text>
	    <xsl:apply-templates select="constructorsynopsis
		|destructorsynopsis
		|fieldsynopsis
		|methodsynopsis
		|classsynopsisinfo" mode="java"/>
	    <xsl:text>}</xsl:text>
	</pre>
    </xsl:template>

    <xsl:template match="classsynopsisinfo" mode="java">
	<xsl:apply-templates mode="java"/>
    </xsl:template>

    <xsl:template match="ooclass|oointerface|ooexception" mode="java">
	<xsl:choose>
	    <xsl:when test="position() &gt; 1">
		<xsl:text>, </xsl:text>
	    </xsl:when>
	    <xsl:otherwise>
		<xsl:text> </xsl:text>
	    </xsl:otherwise>
	</xsl:choose>

	<xsl:apply-templates mode="java"/>

    </xsl:template>

    <xsl:template match="modifier" mode="java">

	<xsl:apply-templates mode="java"/>
	<xsl:text> </xsl:text>

    </xsl:template>

    <xsl:template match="classname" mode="java">
	<xsl:if test="name(preceding-sibling::*[1]) = 'classname'">
	    <xsl:text>, </xsl:text>
	</xsl:if>

	<xsl:apply-templates mode="java"/>

    </xsl:template>

    <xsl:template match="interfacename" mode="java">
	<xsl:if test="name(preceding-sibling::*[1]) = 'interfacename'">
	    <xsl:text>, </xsl:text>
	</xsl:if>

	<xsl:apply-templates mode="java"/>

    </xsl:template>

    <xsl:template match="exceptionname" mode="java">
	<xsl:if test="name(preceding-sibling::*[1]) = 'exceptionname'">
	    <xsl:text>, </xsl:text>
	</xsl:if>

	<xsl:apply-templates mode="java"/>

    </xsl:template>

    <xsl:template match="fieldsynopsis" mode="java">

	<xsl:text>  </xsl:text>
	<xsl:apply-templates mode="java"/>
	<xsl:text>;</xsl:text>

    </xsl:template>

    <xsl:template match="type" mode="java">

	<xsl:apply-templates mode="java"/>
	<xsl:text> </xsl:text>

    </xsl:template>

    <xsl:template match="varname" mode="java">

	<xsl:apply-templates mode="java"/>
	<xsl:text> </xsl:text>

    </xsl:template>

    <xsl:template match="initializer" mode="java">

	<xsl:text>= </xsl:text>
	<xsl:apply-templates mode="java"/>

    </xsl:template>

    <xsl:template match="void" mode="java">

	<xsl:text>void </xsl:text>

    </xsl:template>

    <xsl:template match="methodname" mode="java">

	<xsl:apply-templates mode="java"/>
    </xsl:template>




    <xsl:template match="methodparam" mode="java">
	<!-- PARAM: indent := 0 -->
	<xsl:param name="indent">0</xsl:param>
	<xsl:if test="position() &gt; 1">
	    <xsl:text>,&RE;</xsl:text>
	    <xsl:if test="$indent &gt; 0">
		<!-- RCAS FIXME: copy-string does not exist
		<xsl:call-template name="copy-string">
		    <xsl:with-param name="string"> </xsl:with-param>
		    <xsl:with-param name="count" select="$indent + 1"/>
		</xsl:call-template>
		-->
	    </xsl:if>
	</xsl:if>
	<xsl:apply-templates mode="java"/>
    </xsl:template>



    <xsl:template match="parameter" mode="java">
	<xsl:apply-templates mode="java"/>
    </xsl:template>

    <xsl:template mode="java"
	match="constructorsynopsis|destructorsynopsis|methodsynopsis">
	<xsl:variable name="modifiers" select="modifier"/>
	<xsl:variable name="notmod" select="*[name(.) != 'modifier']"/>
	<xsl:variable name="decl">
	    <xsl:text>  </xsl:text>
	    <xsl:apply-templates select="$modifiers" mode="java"/>

	    <!-- type -->
	    <xsl:if test="name($notmod[1]) != 'methodname'">
		<xsl:apply-templates select="$notmod[1]" mode="java"/>
	    </xsl:if>

	    <xsl:apply-templates select="methodname" mode="java"/>
	</xsl:variable>


	<xsl:copy-of select="$decl"/>
	<xsl:text>(</xsl:text>
	<xsl:apply-templates select="methodparam" mode="java">
	    <xsl:with-param name="indent" select="string-length($decl)"/>
	</xsl:apply-templates>
	<xsl:text>)</xsl:text>
	<xsl:if test="exceptionname">
	    <xsl:text>&RE;    throws </xsl:text>
	    <xsl:apply-templates select="exceptionname" mode="java"/>
	</xsl:if>
	<xsl:text>;</xsl:text>

    </xsl:template>

    <!-- ===== C++ ========================================================= -->

    <xsl:template match="classsynopsis" mode="cpp">
	<pre class="{name(.)}">
	    <xsl:apply-templates select="ooclass[1]" mode="cpp"/>
	    <xsl:if test="ooclass[position() &gt; 1]">
		<xsl:text>: </xsl:text>
		<xsl:apply-templates select="ooclass[position() &gt; 1]" mode="cpp"/>
		<xsl:if test="oointerface|ooexception">
		    <xsl:text>&RE;    </xsl:text>
		</xsl:if>
	    </xsl:if>
	    <xsl:if test="oointerface">
		<xsl:text> implements</xsl:text>
		<xsl:apply-templates select="oointerface" mode="cpp"/>
		<xsl:if test="ooexception">
		    <xsl:text>&RE;    </xsl:text>
		</xsl:if>
	    </xsl:if>
	    <xsl:if test="ooexception">
		<xsl:text> throws</xsl:text>
		<xsl:apply-templates select="ooexception" mode="cpp"/>
	    </xsl:if>
	    <xsl:text> {&RE;&RE;</xsl:text>
	    <xsl:apply-templates select="constructorsynopsis
		|destructorsynopsis
		|fieldsynopsis
		|methodsynopsis
		|classsynopsisinfo" mode="cpp"/>
	    <xsl:text>}</xsl:text>
	</pre>
    </xsl:template>

    <xsl:template match="classsynopsisinfo" mode="cpp">
	<xsl:apply-templates mode="cpp"/>
    </xsl:template>

    <xsl:template match="ooclass|oointerface|ooexception" mode="cpp">
	<xsl:if test="position() &gt; 1">
	    <xsl:text>, </xsl:text>
	</xsl:if>

	<xsl:apply-templates mode="cpp"/>

    </xsl:template>

    <xsl:template match="modifier" mode="cpp">

	<xsl:apply-templates mode="cpp"/>
	<xsl:text> </xsl:text>

    </xsl:template>

    <xsl:template match="classname" mode="cpp">
	<xsl:if test="name(preceding-sibling::*[1]) = 'classname'">
	    <xsl:text>, </xsl:text>
	</xsl:if>

	<xsl:apply-templates mode="cpp"/>

    </xsl:template>

    <xsl:template match="interfacename" mode="cpp">
	<xsl:if test="name(preceding-sibling::*[1]) = 'interfacename'">
	    <xsl:text>, </xsl:text>
	</xsl:if>

	<xsl:apply-templates mode="cpp"/>

    </xsl:template>

    <xsl:template match="exceptionname" mode="cpp">
	<xsl:if test="name(preceding-sibling::*[1]) = 'exceptionname'">
	    <xsl:text>, </xsl:text>
	</xsl:if>

	<xsl:apply-templates mode="cpp"/>

    </xsl:template>

    <xsl:template match="fieldsynopsis" mode="cpp">

	<xsl:text>  </xsl:text>
	<xsl:apply-templates mode="cpp"/>
	<xsl:text>;</xsl:text>

    </xsl:template>

    <xsl:template match="type" mode="cpp">

	<xsl:apply-templates mode="cpp"/>
	<xsl:text> </xsl:text>

    </xsl:template>

    <xsl:template match="varname" mode="cpp">

	<xsl:apply-templates mode="cpp"/>
	<xsl:text> </xsl:text>

    </xsl:template>

    <xsl:template match="initializer" mode="cpp">

	<xsl:text>= </xsl:text>
	<xsl:apply-templates mode="cpp"/>

    </xsl:template>

    <xsl:template match="void" mode="cpp">

	<xsl:text>void </xsl:text>

    </xsl:template>

    <xsl:template match="methodname" mode="cpp">

	<xsl:apply-templates mode="cpp"/>

    </xsl:template>

    <xsl:template match="methodparam" mode="cpp">
	<xsl:if test="position() &gt; 1">
	    <xsl:text>, </xsl:text>
	</xsl:if>

	<xsl:apply-templates mode="cpp"/>

    </xsl:template>

    <xsl:template match="parameter" mode="cpp">

	<xsl:apply-templates mode="cpp"/>

    </xsl:template>

    <xsl:template mode="cpp"
	match="constructorsynopsis|destructorsynopsis|methodsynopsis">
	<xsl:variable name="modifiers" select="modifier"/>
	<xsl:variable name="notmod" select="*[name(.) != 'modifier']"/>
	<xsl:variable name="type">
	</xsl:variable>

	<xsl:text>  </xsl:text>
	<xsl:apply-templates select="$modifiers" mode="cpp"/>

	<!-- type -->
	<xsl:if test="name($notmod[1]) != 'methodname'">
	    <xsl:apply-templates select="$notmod[1]" mode="cpp"/>
	</xsl:if>

	<xsl:apply-templates select="methodname" mode="cpp"/>
	<xsl:text>(</xsl:text>
	<xsl:apply-templates select="methodparam" mode="cpp"/>
	<xsl:text>)</xsl:text>
	<xsl:if test="exceptionname">
	    <xsl:text>&RE;    throws </xsl:text>
	    <xsl:apply-templates select="exceptionname" mode="cpp"/>
	</xsl:if>
	<xsl:text>;</xsl:text>

    </xsl:template>

    <!-- ===== IDL ========================================================= -->

    <xsl:template match="classsynopsis" mode="idl">
	<pre class="{name(.)}">
	    <xsl:text>interface </xsl:text>
	    <xsl:apply-templates select="ooclass[1]" mode="idl"/>
	    <xsl:if test="ooclass[position() &gt; 1]">
		<xsl:text>: </xsl:text>
		<xsl:apply-templates select="ooclass[position() &gt; 1]" mode="idl"/>
		<xsl:if test="oointerface|ooexception">
		    <xsl:text>&RE;    </xsl:text>
		</xsl:if>
	    </xsl:if>
	    <xsl:if test="oointerface">
		<xsl:text> implements</xsl:text>
		<xsl:apply-templates select="oointerface" mode="idl"/>
		<xsl:if test="ooexception">
		    <xsl:text>&RE;    </xsl:text>
		</xsl:if>
	    </xsl:if>
	    <xsl:if test="ooexception">
		<xsl:text> throws</xsl:text>
		<xsl:apply-templates select="ooexception" mode="idl"/>
	    </xsl:if>
	    <xsl:text> {&RE;&RE;</xsl:text>
	    <xsl:apply-templates select="constructorsynopsis
		|destructorsynopsis
		|fieldsynopsis
		|methodsynopsis
		|classsynopsisinfo" mode="idl"/>
	    <xsl:text>}</xsl:text>
	</pre>
    </xsl:template>

    <xsl:template match="classsynopsisinfo" mode="idl">
	<xsl:apply-templates mode="idl"/>
    </xsl:template>

    <xsl:template match="ooclass|oointerface|ooexception" mode="idl">
	<xsl:if test="position() &gt; 1">
	    <xsl:text>, </xsl:text>
	</xsl:if>

	<xsl:apply-templates mode="idl"/>

    </xsl:template>

    <xsl:template match="modifier" mode="idl">

	<xsl:apply-templates mode="idl"/>
	<xsl:text> </xsl:text>

    </xsl:template>

    <xsl:template match="classname" mode="idl">
	<xsl:if test="name(preceding-sibling::*[1]) = 'classname'">
	    <xsl:text>, </xsl:text>
	</xsl:if>

	<xsl:apply-templates mode="idl"/>

    </xsl:template>

    <xsl:template match="interfacename" mode="idl">
	<xsl:if test="name(preceding-sibling::*[1]) = 'interfacename'">
	    <xsl:text>, </xsl:text>
	</xsl:if>

	<xsl:apply-templates mode="idl"/>

    </xsl:template>

    <xsl:template match="exceptionname" mode="idl">
	<xsl:if test="name(preceding-sibling::*[1]) = 'exceptionname'">
	    <xsl:text>, </xsl:text>
	</xsl:if>

	<xsl:apply-templates mode="idl"/>

    </xsl:template>

    <xsl:template match="fieldsynopsis" mode="idl">

	<xsl:text>  </xsl:text>
	<xsl:apply-templates mode="idl"/>
	<xsl:text>;</xsl:text>

    </xsl:template>

    <xsl:template match="type" mode="idl">

	<xsl:apply-templates mode="idl"/>
	<xsl:text> </xsl:text>

    </xsl:template>

    <xsl:template match="varname" mode="idl">

	<xsl:apply-templates mode="idl"/>
	<xsl:text> </xsl:text>

    </xsl:template>

    <xsl:template match="initializer" mode="idl">

	<xsl:text>= </xsl:text>
	<xsl:apply-templates mode="idl"/>

    </xsl:template>

    <xsl:template match="void" mode="idl">

	<xsl:text>void </xsl:text>

    </xsl:template>

    <xsl:template match="methodname" mode="idl">

	<xsl:apply-templates mode="idl"/>

    </xsl:template>

    <xsl:template match="methodparam" mode="idl">
	<xsl:if test="position() &gt; 1">
	    <xsl:text>, </xsl:text>
	</xsl:if>

	<xsl:apply-templates mode="idl"/>

    </xsl:template>

    <xsl:template match="parameter" mode="idl">

	<xsl:apply-templates mode="idl"/>

    </xsl:template>

    <xsl:template mode="idl"
	match="constructorsynopsis|destructorsynopsis|methodsynopsis">
	<xsl:variable name="modifiers" select="modifier"/>
	<xsl:variable name="notmod" select="*[name(.) != 'modifier']"/>
	<xsl:variable name="type">
	</xsl:variable>

	<xsl:text>  </xsl:text>
	<xsl:apply-templates select="$modifiers" mode="idl"/>

	<!-- type -->
	<xsl:if test="name($notmod[1]) != 'methodname'">
	    <xsl:apply-templates select="$notmod[1]" mode="idl"/>
	</xsl:if>

	<xsl:apply-templates select="methodname" mode="idl"/>
	<xsl:text>(</xsl:text>
	<xsl:apply-templates select="methodparam" mode="idl"/>
	<xsl:text>)</xsl:text>
	<xsl:if test="exceptionname">
	    <xsl:text>&RE;    raises(</xsl:text>
	    <xsl:apply-templates select="exceptionname" mode="idl"/>
	    <xsl:text>)</xsl:text>
	</xsl:if>
	<xsl:text>;</xsl:text>

    </xsl:template>





    <!-- ===== Perl ======================================================== -->

    <xsl:template match="classsynopsis" mode="perl">
	<pre class="{name(.)}">
	    <xsl:text>package </xsl:text>
	    <xsl:apply-templates select="ooclass[1]" mode="perl"/>
	    <xsl:text>;&RE;</xsl:text>

	    <xsl:if test="ooclass[position() &gt; 1]">
		<xsl:text>@ISA = (</xsl:text>
		<xsl:apply-templates select="ooclass[position() &gt; 1]" mode="perl"/>
		<xsl:text>);&RE;</xsl:text>
	    </xsl:if>

	    <xsl:apply-templates select="constructorsynopsis
		|destructorsynopsis
		|fieldsynopsis
		|methodsynopsis
		|classsynopsisinfo" mode="perl"/>
	</pre>
    </xsl:template>

    <xsl:template match="classsynopsisinfo" mode="perl">
	<xsl:apply-templates mode="perl"/>
    </xsl:template>

    <xsl:template match="ooclass|oointerface|ooexception" mode="perl">
	<xsl:if test="position() &gt; 1">
	    <xsl:text>, </xsl:text>
	</xsl:if>

	<xsl:apply-templates mode="perl"/>

    </xsl:template>

    <xsl:template match="modifier" mode="perl">

	<xsl:apply-templates mode="perl"/>
	<xsl:text> </xsl:text>

    </xsl:template>

    <xsl:template match="classname" mode="perl">
	<xsl:if test="name(preceding-sibling::*[1]) = 'classname'">
	    <xsl:text>, </xsl:text>
	</xsl:if>

	<xsl:apply-templates mode="perl"/>

    </xsl:template>

    <xsl:template match="interfacename" mode="perl">
	<xsl:if test="name(preceding-sibling::*[1]) = 'interfacename'">
	    <xsl:text>, </xsl:text>
	</xsl:if>

	<xsl:apply-templates mode="perl"/>

    </xsl:template>

    <xsl:template match="exceptionname" mode="perl">
	<xsl:if test="name(preceding-sibling::*[1]) = 'exceptionname'">
	    <xsl:text>, </xsl:text>
	</xsl:if>

	<xsl:apply-templates mode="perl"/>

    </xsl:template>

    <xsl:template match="fieldsynopsis" mode="perl">

	<xsl:text>  </xsl:text>
	<xsl:apply-templates mode="perl"/>
	<xsl:text>;</xsl:text>

    </xsl:template>

    <xsl:template match="type" mode="perl">

	<xsl:apply-templates mode="perl"/>
	<xsl:text> </xsl:text>

    </xsl:template>

    <xsl:template match="varname" mode="perl">

	<xsl:apply-templates mode="perl"/>
	<xsl:text> </xsl:text>

    </xsl:template>

    <xsl:template match="initializer" mode="perl">

	<xsl:text>= </xsl:text>
	<xsl:apply-templates mode="perl"/>

    </xsl:template>

    <xsl:template match="void" mode="perl">

	<xsl:text>void </xsl:text>

    </xsl:template>



    <xsl:template match="methodname" mode="perl">
	<xsl:apply-templates mode="perl"/>
    </xsl:template>

    <xsl:template match="methodparam" mode="perl">
	<xsl:if test="position() &gt; 1"> <xsl:text>, </xsl:text> </xsl:if>
	<xsl:apply-templates mode="perl"/>
    </xsl:template>


    <xsl:template match="parameter" mode="perl">
	<xsl:apply-templates mode="perl"/>
    </xsl:template>


    <xsl:template mode="perl" match="constructorsynopsis|destructorsynopsis|methodsynopsis">
	<xsl:variable name="modifiers" select="modifier"/>
	<xsl:variable name="notmod" select="*[name(.) != 'modifier']"/>
	<xsl:variable name="type"> </xsl:variable>

	<xsl:text>sub </xsl:text>

	<xsl:apply-templates select="methodname" mode="perl"/>
	<xsl:text> { ... };</xsl:text>
    </xsl:template>

</xsl:stylesheet>
