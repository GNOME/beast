<?xml version="1.0"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
  <xsl:output method="xml" indent="no" doctype-system="markup.dtd"/>
  <xsl:strip-space elements="*"/>
  <xsl:preserve-space elements="keepspace preformat programlisting reference-scheme"/>

  <xsl:param name="revision"/>

  <!-- {{{ start parsing -->
  <xsl:template match="texinfo">
    <tag-span-markup>
      <!-- lower priority tags need to come first -->
      <tagdef name="chapter"        indent="0" />
      <tagdef name="section"        indent="0" />
      <tagdef name="subsection"     indent="0" />
      <tagdef name="subsubsection"  indent="0" />
      <tagdef name="body"           wrap_mode="word" left_margin="5" right_margin="5">
        <xsl:attribute name="family">
	  <xsl:call-template name="document-font"/>
	</xsl:attribute>
      </tagdef>
      <tagdef name="indent-margin"  left_margin="20" right_margin="20" />
      <tagdef name="item-margin"    left_margin="18" />
      <tagdef name="bullet-tag"     indent="-10" />
      <tagdef name="enumerate-item" weight="bold" foreground="#000070" family="monospace" />
      <tagdef name="dline"          underline="double" weight="bold" />
      <tagdef name="sline"          underline="single" weight="bold" />
      <tagdef name="nowrap"         wrap_mode="none" />
      <tagdef name="indented"	    left_margin="20" />
      <tagdef name="tableterm"	    family="monospace"/>
      <tagdef name="tableitem"	    left_margin="45" />

      <!-- Sucks but hey -->
      <tagdef name="table_entry_1"  left_margin="0"   />
      <tagdef name="table_entry_2"  left_margin="20"  />
      <tagdef name="table_entry_3"  left_margin="40"  />
      <tagdef name="table_entry_4"  left_margin="60"  />
      <tagdef name="table_entry_5"  left_margin="80"  />
      <tagdef name="table_entry_6"  left_margin="100" />
      <tagdef name="table_entry_7"  left_margin="120" />
      <tagdef name="table_entry_8"  left_margin="140" />
      <tagdef name="table_entry_9"  left_margin="160" />

      <tagdef name="multitable"     left_margin="20"/>

      <tagdef name="title_page"     justification="center" />
      <tagdef name="doc_title"      underline="double" />
      <tagdef name="doc_author"     weight="bold" />

      <tagdef name="hyperlink"      underline="single" foreground="#0000ff" />

      <!-- contextual tags -->
      <tagdef name="code"           family="monospace" foreground="#000040" />

      <tagdef name="acronym"        />
      <tagdef name="cite"           />
      <tagdef name="command"        />
      <tagdef name="dfn"            />
      <tagdef name="email"          />
      <tagdef name="env"            />
      <tagdef name="file"           />
      <tagdef name="kbd"            />
      <tagdef name="key"            />
      <tagdef name="option"         />
      <tagdef name="samp"           />
      <tagdef name="strong"         weight="bold" />
      <tagdef name="url"            />
      <tagdef name="var"            />

      <!-- reference elements -->
      <tagdef name="reference-docname"	    weight="bold" />
      <tagdef name="reference-scheme"	    />
      <tagdef name="reference-function"	    weight="bold" foreground="#5555cc" />
      <tagdef name="reference-parameter"    weight="bold" foreground="#cc4444" />
      <tagdef name="reference-returns"	    weight="bold" foreground="#228822" />
      <tagdef name="reference-type"	    foreground="#555555" />
      <tagdef name="reference-blurb"	    foreground="#555555" />
      <tagdef name="reference-struct"	    weight="bold" />
      <tagdef name="reference-struct-name"  weight="bold" foreground="#cc4444" />

      <tagdef name="revision"       style="italic" />

      <tagdef name="programlisting" family="monospace" wrap_mode="none" foreground="#000040" />
      <tagdef name="property"       style="italic" />
      <tagdef name="channel"        style="italic" />
      <tagdef name="menupath"       style="italic" weight="bold" background="#e0e0e0" />
      <tagdef name="pagepath"       style="italic" weight="bold" background="#f0f0f0" />
      <tagdef name="object"         family="monospace" style="italic" />

      <!-- high priority markup primitives -->
      <tagdef name="bold"           weight="bold" stretch="expanded" />
      <tagdef name="italic"         style="italic" />
      <tagdef name="mono"           family="monospace" />
      <tagdef name="center"         justification="center" />
      <tagdef name="fill"           justification="fill" />
      <tagdef name="underline"      underline="single" />
      <tagdef name="doubleline"     underline="double" />
      <tagdef name="fg-black"       foreground="#000000" />
      <tagdef name="fg-white"       foreground="#ffffff" />
      <tagdef name="fg-red"         foreground="#ff0000" />
      <tagdef name="fg-green"       foreground="#00ff00" />
      <tagdef name="fg-blue"        foreground="#0000ff" />
      <tagdef name="fg-turquoise"   foreground="#00ffff" />
      <tagdef name="fg-pink"        foreground="#ff00ff" />
      <tagdef name="fg-yellow"      foreground="#ffff00" />
      <tagdef name="bg-black"       background="#000000" />
      <tagdef name="bg-white"       background="#ffffff" />
      <tagdef name="bg-red"         background="#ff0000" />
      <tagdef name="bg-green"       background="#00ff00" />
      <tagdef name="bg-blue"        background="#0000ff" />
      <tagdef name="bg-turquoise"   background="#00ffff" />
      <tagdef name="bg-pink"        background="#ff00ff" />
      <tagdef name="bg-yellow"      background="#ffff00" />

      <!-- generate body -->
      <span tag="body">
	<xsl:call-template name="title_page"/>
	<xsl:apply-templates/>
      </span>
    </tag-span-markup>
  </xsl:template>
  <!-- }}} -->

  <!-- {{{ useless tags -->
  <xsl:template match="setfilename|settitle|document-title|document-author|document-package|document-font|itemfunction|columnfraction"/>
  <!-- }}} -->

  <!-- {{{ setting a default font for documents -->
  <xsl:template name="document-font">
    <xsl:variable name="font" select="string(/texinfo/para/document-font)"/>
    <xsl:choose>
      <xsl:when test="$font='tech' or $font='techstyle' or $font='sans' or $font='sans-serif'"><xsl:text>sans</xsl:text></xsl:when>
      <xsl:when test="$font='story' or $font='storystyle' or $font='serif' or $font=''"><xsl:text>serif</xsl:text></xsl:when>
      <xsl:otherwise>
	<xsl:message>XSL-WARNING: omitting unknown font style '<xsl:value-of select="$font"/>'</xsl:message>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>
  <!-- }}} -->

  <!-- {{{ creating a title page for documents -->
  <xsl:template name="title_page">
    <xsl:if test="string-length(/texinfo/para/document-title) > 0 or count(/texinfo/para/document-author) > 0">
      <newline/>
      <span tag="title_page">
	<xsl:if test="string-length(/texinfo/para/document-title) > 0">
	  <span tag="doc_title">
	    <xsl:value-of select="/texinfo/para/document-title"/>
	  </span>
	  <newline/><newline/>
	</xsl:if>
	<xsl:if test="count(/texinfo/para/document-author) > 0">
	  <span tag="doc_author">
	    <xsl:choose>
	      <xsl:when test="count(/texinfo/para/document-author) > 1">
		<xsl:for-each select="/texinfo/para/document-author">
		  <xsl:if test="position() > 1 and not(position()=last())">
		    <xsl:text>, </xsl:text>
		  </xsl:if>
		  <xsl:if test="position() mod 4 = 0">
		    <breakline/>
		  </xsl:if>
		  <xsl:if test="position() = last()">
		    <xsl:text> and </xsl:text>
		  </xsl:if>
		  <xsl:apply-templates/>
		</xsl:for-each>
	      </xsl:when>
	      <xsl:otherwise>
		<xsl:for-each select="/texinfo/para/document-author">
		  <xsl:apply-templates/>
		</xsl:for-each>
	      </xsl:otherwise>
	    </xsl:choose>
	  </span>
	</xsl:if>
      </span>
      <newline/>
    </xsl:if>
  </xsl:template>
  <!-- }}} -->

  <!-- {{{ revision bit -->
  <xsl:template match="para/revision">
    <xsl:choose>
      <xsl:when test="string-length($revision) > 0">
	<span tag="revision">
	  <xsl:text>Document revised: </xsl:text><xsl:value-of select="$revision"/>
	</span>
      </xsl:when>
      <xsl:otherwise>
        <xsl:message>XSL-WARNING: Skipping Document Revision line, revision date not provided.</xsl:message>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>
  <!-- }}} -->

  <!-- {{{ table of contents related stuff -->
  <xsl:template name="node_number">
    <xsl:text>node-</xsl:text><xsl:number level="multiple" count="chapter|section|subsection|subsubsection|appendix|appendixsec|appendixsubsec|appendixsubsubsec|unnumbered|unnumberedsec|unnumberedsubsec|unnumberedsubsubsec" format="1-1-1-1"/>
  </xsl:template>

  <xsl:template name="node_name">
    <anchor>
      <xsl:attribute name="name">
	<xsl:call-template name="node_number"/>
      </xsl:attribute>
    </anchor>
  </xsl:template>

  <xsl:template match="para/table-of-contents">
    <xsl:for-each select="/texinfo/chapter|/texinfo/unnumbered|/texinfo/appendix">
      <xsl:if test="local-name() = 'chapter'">
	<xsl:call-template name="toc_chapter"/>
      </xsl:if>
      <xsl:if test="local-name() = 'unnumbered'">
	<xsl:call-template name="toc_unnumbered"/>
      </xsl:if>
      <xsl:if test="local-name() = 'appendix'">
	<xsl:call-template name="toc_appendix"/>
      </xsl:if>
    </xsl:for-each>
  </xsl:template>

  <xsl:template name="toc_chapter">
    <breakline/>
    <span tag="hyperlink">
      <xlink>
	<xsl:attribute name="ref">
	  <xsl:text>file:#</xsl:text><xsl:call-template name="node_number"/>
	</xsl:attribute>
	<xsl:number format="1 - "/><xsl:value-of select="title"/>
      </xlink>
    </span>
    <xsl:if test="count(./section) > 0">
      <span tag="indented">
	<xsl:for-each select="./section">
	  <xsl:call-template name="toc_section"/>
	</xsl:for-each>
      </span>
    </xsl:if>
  </xsl:template>

  <xsl:template name="toc_section">
    <breakline/>
    <span tag="hyperlink">
      <xlink>
	<xsl:attribute name="ref">
	  <xsl:text>file:#</xsl:text><xsl:call-template name="node_number"/>
	</xsl:attribute>
	<xsl:number level="multiple" count="chapter|section" format="1.1 - "/><xsl:value-of select="title"/>
      </xlink>
    </span>
    <xsl:if test="count(./subsection) > 0">
      <span tag="indented">
	<xsl:for-each select="./subsection">
	  <xsl:call-template name="toc_subsection"/>
	</xsl:for-each>
      </span>
    </xsl:if>
  </xsl:template>

  <xsl:template name="toc_subsection">
    <breakline/>
    <span tag="hyperlink">
      <xlink>
	<xsl:attribute name="ref">
	  <xsl:text>file:#</xsl:text><xsl:call-template name="node_number"/>
	</xsl:attribute>
	<xsl:number level="multiple" count="chapter|section|subsection" format="1.1.1 - "/><xsl:value-of select="title"/>
      </xlink>
    </span>
    <xsl:if test="count(./subsubsection) > 0">
      <span tag="indented">
	<xsl:for-each select="./subsubsection">
	  <xsl:call-template name="toc_subsubsection"/>
	</xsl:for-each>
      </span>
    </xsl:if>
  </xsl:template>

  <xsl:template name="toc_subsubsection">
    <breakline/>
    <span tag="hyperlink">
      <xlink>
	<xsl:attribute name="ref">
	  <xsl:text>file:#</xsl:text><xsl:call-template name="node_number"/>
	</xsl:attribute>
	<xsl:number level="multiple" count="chapter|section|subsection|subsubsection" format="1.1.1.1 - "/><xsl:value-of select="title"/>
      </xlink>
    </span>
  </xsl:template>

  <xsl:template name="toc_appendix">
    <breakline/>
    <span tag="hyperlink">
      <xlink>
	<xsl:attribute name="ref">
	  <xsl:text>file:#</xsl:text><xsl:call-template name="node_number"/>
	</xsl:attribute>
	<xsl:text>Appendix </xsl:text><xsl:number format="A - "/><xsl:value-of select="title"/>
      </xlink>
    </span>
    <xsl:if test="count(./appendixsec) > 0">
      <span tag="indented">
	<xsl:for-each select="./appendixsec">
	  <xsl:call-template name="toc_appendixsec"/>
	</xsl:for-each>
      </span>
    </xsl:if>
  </xsl:template>

  <xsl:template name="toc_appendixsec">
    <breakline/>
    <span tag="hyperlink">
      <xlink>
	<xsl:attribute name="ref">
	  <xsl:text>file:#</xsl:text><xsl:call-template name="node_number"/>
	</xsl:attribute>
	<xsl:number level="multiple" count="appendix|appendixsec" format="A.1 - "/><xsl:value-of select="title"/>
      </xlink>
    </span>
    <xsl:if test="count(./appendixsubsec) > 0">
      <span tag="indented">
	<xsl:for-each select="./appendixsubsec">
	  <xsl:call-template name="toc_appendixsubsec"/>
	</xsl:for-each>
      </span>
    </xsl:if>
  </xsl:template>

  <xsl:template name="toc_appendixsubsec">
    <breakline/>
    <span tag="hyperlink">
      <xlink>
	<xsl:attribute name="ref">
	  <xsl:text>file:#</xsl:text><xsl:call-template name="node_number"/>
	</xsl:attribute>
	<xsl:number level="multiple" count="appendix|appendixsec|appendixsubsec" format="A.1.1 - "/><xsl:value-of select="title"/>
      </xlink>
    </span>
    <xsl:if test="count(./appendixsubsubsec) > 0">
      <span tag="indented">
	<xsl:for-each select="./appendixsubsubsec">
	  <xsl:call-template name="toc_appendixsubsubsec"/>
	</xsl:for-each>
      </span>
    </xsl:if>
  </xsl:template>

  <xsl:template name="toc_appendixsubsubsec">
    <breakline/>
    <span tag="hyperlink">
      <xlink>
	<xsl:attribute name="ref">
	  <xsl:text>file:#</xsl:text><xsl:call-template name="node_number"/>
	</xsl:attribute>
	<xsl:number level="multiple" count="appendix|appendixsec|appendixsubsec|appendixsubsubsec" format="A.1.1.1 - "/><xsl:value-of select="title"/>
      </xlink>
    </span>
  </xsl:template>

  <xsl:template name="toc_unnumbered">
    <breakline/>
    <span tag="hyperlink">
      <xlink>
	<xsl:attribute name="ref">
	  <xsl:text>file:#</xsl:text><xsl:call-template name="node_number"/>
	</xsl:attribute>
	<xsl:value-of select="title"/>
      </xlink>
    </span>
    <xsl:if test="count(./unnumberedsec) > 0">
      <span tag="indented">
	<xsl:for-each select="./unnumberedsec">
	  <xsl:call-template name="toc_unnumberedsec"/>
	</xsl:for-each>
      </span>
    </xsl:if>
  </xsl:template>

  <xsl:template name="toc_unnumberedsec">
    <breakline/>
    <span tag="hyperlink">
      <xlink>
	<xsl:attribute name="ref">
	  <xsl:text>file:#</xsl:text><xsl:call-template name="node_number"/>
	</xsl:attribute>
	<xsl:value-of select="title"/>
      </xlink>
    </span>
    <xsl:if test="count(./unnumberedsubsec) > 0">
      <span tag="indented">
	<xsl:for-each select="./unnumberedsubsec">
	  <xsl:call-template name="toc_unnumberedsubsec"/>
	</xsl:for-each>
      </span>
    </xsl:if>
  </xsl:template>

  <xsl:template name="toc_unnumberedsubsec">
    <span tag="hyperlink">
      <xlink>
	<xsl:attribute name="ref">
	  <xsl:text>file:#</xsl:text><xsl:call-template name="node_number"/>
	</xsl:attribute>
	<xsl:value-of select="title"/>
      </xlink>
    </span>
    <xsl:if test="count(./unnumberedsubsubsec) > 0">
      <span tag="indented">
	<xsl:for-each select="./unnumberedsubsubsec">
	  <xsl:call-template name="toc_unnumberedsubsubsec"/>
	</xsl:for-each>
      </span>
    </xsl:if>
  </xsl:template>

  <xsl:template name="toc_unnumberedsubsubsec">
    <breakline/>
    <span tag="hyperlink">
      <xlink>
	<xsl:attribute name="ref">
	  <xsl:text>file:#</xsl:text><xsl:call-template name="node_number"/>
	</xsl:attribute>
	<xsl:value-of select="title"/>
      </xlink>
    </span>
  </xsl:template>

  <!-- }}} -->

  <!-- {{{ document sections -->
  <xsl:template match="chapter|appendix|unnumbered">
    <span tag="chapter">
      <xsl:apply-templates/>
    </span>
    <xsl:if test="not(position()=last())">
      <newline/><newline/>
    </xsl:if>
  </xsl:template>

  <xsl:template match="section|appendixsec|unnumberedsec">
    <xsl:if test="position()>2">
      <newline/>
    </xsl:if>
    <span tag="section">
      <xsl:apply-templates/>
    </span>
  </xsl:template>

  <xsl:template match="subsection|appendixsubsec|unnumberedsubsec">
    <xsl:if test="position()>2">
      <newline/>
    </xsl:if>
    <span tag="subsection">
      <xsl:apply-templates/>
    </span>
  </xsl:template>

  <xsl:template match="subsubsection|appendixsubsubsec|unnumberedsubsubsec">
    <xsl:if test="position()>2">
      <newline/>
    </xsl:if>
    <span tag="subsubsection">
      <xsl:apply-templates/>
    </span>
  </xsl:template>
  <!-- }}} -->

  <!-- {{{ section titles stuff -->
  <xsl:template match="chapter/title">
    <span tag="dline">
      <span tag="center">
	<xsl:call-template name="node_name"/>
	<xsl:number count="chapter" format="1 - "/><xsl:apply-templates/>
      </span>
    </span>
    <breakline/><newline/>
  </xsl:template>

  <xsl:template match="section/title">
    <span tag="sline">
      <xsl:call-template name="node_name"/>
      <xsl:number level="multiple" count="chapter|section" format="1.1 - "/><xsl:apply-templates/>
    </span>
    <breakline/><newline/>
  </xsl:template>

  <xsl:template match="subsection/title">
    <span tag="sline">
      <xsl:call-template name="node_name"/>
      <xsl:number level="multiple" count="chapter|section|subsection" format="1.1.1 - "/><xsl:apply-templates/>
    </span>
    <breakline/>
  </xsl:template>

  <xsl:template match="subsubsection/title">
    <span tag="sline">
      <xsl:call-template name="node_name"/>
      <xsl:number level="multiple" count="chapter|section|subsection|subsubsection" format="1.1.1.1 - "/><xsl:apply-templates/>
    </span>
    <breakline/>
  </xsl:template>

  <xsl:template match="appendix/title">
    <span tag="dline">
      <span tag="center">
	<xsl:number count="appendix" format="A - "/><xsl:apply-templates/>
      </span>
    </span>
    <breakline/><newline/>
  </xsl:template>

  <xsl:template match="appendixsec/title">
    <span tag="sline">
      <xsl:call-template name="node_name"/>
      <xsl:number level="multiple" count="appendix|appendixsec" format="A.1 - "/><xsl:apply-templates/>
    </span>
    <breakline/><newline/>
  </xsl:template>

  <xsl:template match="appendixsubsec/title">
    <span tag="sline">
      <xsl:call-template name="node_name"/>
      <xsl:number level="multiple" count="appendix|appendixsec|appendixsubsec" format="A.1.1 - "/><xsl:apply-templates/>
    </span>
    <breakline/>
  </xsl:template>

  <xsl:template match="appendixsubsubsec/title">
    <span tag="sline">
      <xsl:call-template name="node_name"/>
      <xsl:number level="multiple" count="appendix|appendixsec|appendixsubsec|appendixsubsubsec" format="A.1.1.1 - "/><xsl:apply-templates/>
    </span>
    <breakline/>
  </xsl:template>

  <xsl:template match="unnumbered/title|chapheading/title|majorheading/title">
    <span tag="dline">
      <span tag="center">
	<xsl:apply-templates/>
      </span>
    </span>
    <breakline/><newline/>
  </xsl:template>

  <xsl:template match="unnumberedsec/title|unnumberedsubsec/title|unnumberedsubsubsec/title">
    <span tag="sline">
      <xsl:call-template name="node_name"/>
      <xsl:apply-templates/>
    </span>
    <breakline/>
    <xsl:if test="local-name(..) = 'unnumberedsec'">
      <newline/>
    </xsl:if>
  </xsl:template>

  <xsl:template match="heading/title|subheading/title|subsubheading/title">
    <span tag="sline">
      <xsl:apply-templates/>
    </span>
    <breakline/>
    <xsl:if test="local-name(..) = 'heading'">
      <newline/>
    </xsl:if>
  </xsl:template>
  <!-- }}} -->

  <!-- {{{ reference generation -->
  <xsl:template match="*[position()>2 and self::para]/reference-title">
    <newline/>
  </xsl:template>

  <xsl:template match="reference-docname|reference-function|reference-scheme|reference-parameter|reference-returns|reference-type|reference-blurb|reference-struct-name">
    <xsl:if test="local-name() = 'reference-struct-name'">
      <xsl:text> </xsl:text>
    </xsl:if>
    <span>
      <xsl:attribute name="tag">
	<xsl:value-of select="local-name()"/>
      </xsl:attribute>
      <xsl:apply-templates/>
    </span>
    <xsl:if test="local-name()='reference-scheme'">
      <breakline/>
    </xsl:if>
  </xsl:template>

  <xsl:template match="reference-struct-open">
    <span tag="reference-struct"> {</span>
  </xsl:template>

  <xsl:template match="reference-struct-close">
    <span tag="reference-struct">};</span>
  </xsl:template>
  <!-- }}} -->

  <!-- {{{ paragrapghs -->
  <xsl:template match="para">
    <xsl:apply-templates/>
    <newline/>
  </xsl:template>
  <!-- }}} -->

  <!-- {{{ line breaks -->
  <xsl:template match="linebreak">
    <breakline/>
  </xsl:template>
  <!-- }}} -->

  <!-- {{{ old para handling. too complicated and causes more troubles than benefits -->
  <xsl:template match="para-disabled">
    <!-- If paragrapgh is bogus (ie. white-space only), skip it -->
    <xsl:choose>
      <xsl:when test="(count(./revision) + count(./table-of-contents) + count(./reference-title) +  count(./reference-struct-open) + count(./reference-struct-close)) > 0">
        <xsl:apply-templates/>
      </xsl:when>
      <!-- If this is an item, then it's not a real paragraph -->
      <xsl:when test="local-name(..)='item'">
	<xsl:apply-templates/>
      </xsl:when>
      <xsl:when test="(count(./document-author) + count(./document-title) + count(./document-font)) > 0"/>
      <xsl:when test="normalize-space(.) = ''"/>
      <xsl:otherwise>
        <xsl:apply-templates/>
	<newline/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>
  <!-- }}} -->

  <!-- {{{ contextual tags -->
  <xsl:template match="acronym|cite|dfn|kbd|samp|var|strong|url|email|key|env|file|command|option">
    <span>
      <xsl:attribute name="tag">
	<xsl:value-of select="local-name()"/>
      </xsl:attribute>
      <span tag="code">
	<xsl:apply-templates/>
      </span>
    </span>
  </xsl:template>

  <xsl:template match="programlisting">
    <breakline/>
    <span tag="programlisting">
      <keep-space><xsl:apply-templates/></keep-space>
    </span>
    <breakline/>
  </xsl:template>

  <xsl:template match="code">
    <span tag="code">
      <xsl:apply-templates/>
    </span>
  </xsl:template>

  <xsl:template match="menupath">
    <span tag="menupath">
      <xsl:apply-templates/>
    </span>
  </xsl:template>

  <xsl:template match="pagepath">
    <span tag="pagepath"><xsl:apply-templates/></span>
  </xsl:template>

  <xsl:template match="property">
    <span tag="property">
      "<xsl:apply-templates/>"
    </span>
  </xsl:template>

  <xsl:template match="object">
    <span tag="object">
      <xsl:apply-templates/>
    </span>
  </xsl:template>

  <xsl:template match="module">
    <span tag="object">
      <xsl:apply-templates/>
    </span>
  </xsl:template>

  <xsl:template match="channel">
    <span tag="channel">
      <xsl:apply-templates/>
    </span>
  </xsl:template>

  <xsl:template match="emph|emphasize">
    <span tag="italic">
      <xsl:apply-templates/>
    </span>
  </xsl:template>

  <xsl:template match="important">
    <span tag="italic">
      <span tag="underline">
	<xsl:apply-templates/>
      </span>
    </span>
  </xsl:template>

  <xsl:template match="center">
    <breakline/>
    <span tag="center">
      <xsl:apply-templates/>
    </span>
    <breakline/>
  </xsl:template>

  <xsl:template match="indent">
    <breakline/>
    <span tag="indent-margin">
      <xsl:apply-templates/>
    </span>
    <breakline/>
  </xsl:template>

  <xsl:template match="fill">
    <breakline/>
    <!-- grumbl, text-widget fill is not implemented -->
    <keep-space><xsl:apply-templates/></keep-space>
    <breakline/>
  </xsl:template>

  <xsl:template match="preformat">
    <breakline/>
    <span tag="nowrap">
      <span tag="mono">
	<keep-space><xsl:apply-templates/></keep-space>
      </span>
    </span>
    <breakline/>
  </xsl:template>

  <xsl:template match="keepspace">
    <breakline/>
    <span tag="nowrap">
      <keep-space><xsl:apply-templates/></keep-space>
    </span>
    <breakline/>
  </xsl:template>
  <!-- }}} -->

  <!-- {{{ enumeration and itemization handling -->
  <xsl:template match="itemize|enumerate">
    <breakline/>
    <xsl:apply-templates/>
    <xsl:if test="not(position()=last())">
      <newline/>
    </xsl:if>
  </xsl:template>

  <xsl:template match="itemize/item">
    <span tag="item-margin">
      <span tag="bullet-tag"><image stock="gtk-yes" size="10x10"/></span>
      <xsl:apply-templates/>
    </span>
    <breakline/>
  </xsl:template>

  <xsl:template match="enumerate/item">
    <span tag="item-margin">
      <span tag="enumerate-item">
        <xsl:number format="1."/>
      </span>
      <xsl:apply-templates/>
    </span>
    <breakline/>
  </xsl:template>
  <!-- }}} -->

<!-- This is the old uref check for emails -->
<!--
  <span tag="hyperlink">
    <xlink>
      <xsl:attribute name="ref">
	<xsl:value-of select="urefurl"/>
      </xsl:attribute>
      <xsl:choose>
	<xsl:when test="count(child::urefreplacement)"><xsl:apply-templates select="urefreplacement"/></xsl:when>
	<xsl:when test="count(child::urefdesc)"><xsl:apply-templates select="urefdesc"/> (<xsl:apply-templates select="urefurl"/>)</xsl:when>
	<xsl:otherwise><xsl:apply-templates select="urefurl"/></xsl:otherwise>
      </xsl:choose>
    </xlink>
  </span>
-->

  <!-- {{{ parsing and printing urefs according to their protocols -->
  <xsl:template match="uref">
    <!-- protocol for this link type -->
    <xsl:variable name="protocol" select="substring-before(urefurl, '://')"/>
    <xsl:if test="$protocol=''">
      <xsl:message terminate="yes">XSL-ERROR: unset protocol for <xsl:value-of select="urefurl"/></xsl:message>
    </xsl:if>

    <!-- actual link -->
    <xsl:variable name="url" select="substring-after(urefurl, '://')"/>

    <!-- feedback -->
    <!-- <xsl:message>DEBUG: protocol is <xsl:value-of select="$protocol"/> for <xsl:value-of select="urefurl"/></xsl:message> -->

    <xsl:choose>
      <!-- PROTOCOL: HTTP FTP FILE -->
      <xsl:when test="$protocol='http' or $protocol='ftp' or $protocol='file'">
	<span tag="hyperlink">
	  <xlink>
	    <xsl:attribute name="ref">
	      <xsl:value-of select="concat($protocol, '://', $url)"/>
	    </xsl:attribute>
	    <xsl:choose>
	      <xsl:when test="count(child::urefreplacement)"><xsl:apply-templates select="urefreplacement"/></xsl:when>
	      <xsl:when test="count(child::urefdesc)"><xsl:apply-templates select="urefdesc"/> (<xsl:value-of select="concat($protocol, '://', $url)"/>)</xsl:when>
	      <xsl:otherwise><xsl:value-of select="concat($protocol, '://', $url)"/></xsl:otherwise>
	    </xsl:choose>
	  </xlink>
	</span>
      </xsl:when>
      <!-- PROTOCOL: MAILTO NEWS -->
      <xsl:when test="$protocol='mailto' or $protocol='news'">
	<span tag="hyperlink">
	  <xlink>
	    <xsl:attribute name="ref">
	      <xsl:value-of select="concat($protocol, ':', $url)"/>
	    </xsl:attribute>
	    <xsl:choose>
	      <xsl:when test="count(child::urefreplacement)"><xsl:apply-templates select="urefreplacement"/></xsl:when>
	      <xsl:when test="count(child::urefdesc)"><xsl:apply-templates select="urefdesc"/> (<xsl:value-of select="$url"/>)</xsl:when>
	      <xsl:otherwise><xsl:value-of select="$url"/></xsl:otherwise>
	    </xsl:choose>
	  </xlink>
	</span>
      </xsl:when>
      <!-- PROTOCOL: System and BEAST Man Pages -->
      <xsl:when test="$protocol='man' or $protocol='beast-man'">
	<!-- Get the section the man page belongs to -->
	<xsl:variable name="section">
	  <xsl:choose>
	    <xsl:when test="substring-before($url, '/') = ''">
	      <xsl:message>XSL-WARNING: unset man section in <xsl:value-of select="urefurl"/>, using default (1)</xsl:message>
	      <xsl:value-of select="1"/>
	    </xsl:when>
	    <xsl:otherwise>
	      <xsl:value-of select="substring-before($url, '/')"/>
	    </xsl:otherwise>
	  </xsl:choose>
	</xsl:variable>
	<!-- Name of the page -->
	<xsl:variable name="page">
	  <xsl:choose>
	    <xsl:when test="substring-after($url, '/') = ''">
	      <xsl:value-of select="$url"/>
	    </xsl:when>
	    <xsl:otherwise>
	      <xsl:value-of select="substring-after($url, '/')"/>
	    </xsl:otherwise>
	  </xsl:choose>
	</xsl:variable>
	<xsl:choose>
	  <xsl:when test="$protocol='man'">
	    <!-- Print System Man Page (it's not a link) -->
	    <xsl:choose>
	      <xsl:when test="count(child::urefreplacement)"><xsl:apply-templates select="urefreplacement"/></xsl:when>
	      <xsl:when test="count(child::urefdesc)"><xsl:apply-templates select="urefdesc"/> (<xsl:value-of select="concat($page, '(', $section, ')')"/>)</xsl:when>
	      <xsl:otherwise><xsl:value-of select="concat($page, '(', $section, ')')"/></xsl:otherwise>
	    </xsl:choose>
	  </xsl:when>
	  <xsl:when test="$protocol='beast-man'">
	    <!-- Print BEAST Man Page (it's a link) -->
	    <span tag="hyperlink">
	      <xlink>
		<xsl:attribute name="ref">
		  <xsl:value-of select="concat($page, '.', $section, '.markup')"/>
		</xsl:attribute>
		<xsl:choose>
		  <xsl:when test="count(child::urefreplacement)"><xsl:apply-templates select="urefreplacement"/></xsl:when>
		  <xsl:when test="count(child::urefdesc)"><xsl:apply-templates select="urefdesc"/> (<xsl:value-of select="concat($page, '.', $section, '.markup')"/>)</xsl:when>
		  <xsl:otherwise><xsl:value-of select="concat($page, '.', $section, '.markup')"/></xsl:otherwise>
		</xsl:choose>
	      </xlink>
	    </span>
	  </xsl:when>
	</xsl:choose>
      </xsl:when>
      <!-- PROTOCOL: Beast Document -->
      <xsl:when test="$protocol='beast-doc'">
	<!-- Get the file name and append the target specific extension (markup) -->
	<xsl:variable name="filename">
	  <xsl:choose>
	    <xsl:when test="substring-before($url, '#') = ''">
	      <xsl:value-of select="concat($url, '.markup')"/>
	    </xsl:when>
	    <xsl:otherwise>
	      <xsl:value-of select="concat(substring-before($url, '#'), '.markup')"/>
	    </xsl:otherwise>
	  </xsl:choose>
	</xsl:variable>
	<!-- Get the anchor -->
	<xsl:variable name="anchor">
	  <xsl:choose>
	    <xsl:when test="substring-after($url, '#') = ''"/>
	    <xsl:otherwise>
	      <xsl:value-of select="concat('#', substring-after($url, '#'))"/>
	    </xsl:otherwise>
	  </xsl:choose>
	</xsl:variable>
	<!-- Print the link -->
	<span tag="hyperlink">
	  <xlink>
	    <xsl:attribute name="ref">
	      <xsl:value-of select="concat($filename, $anchor)"/>
	    </xsl:attribute>
	    <xsl:choose>
	      <xsl:when test="count(child::urefreplacement)"><xsl:apply-templates select="urefreplacement"/></xsl:when>
	      <xsl:when test="count(child::urefdesc)"><xsl:apply-templates select="urefdesc"/> (<xsl:value-of select="concat($filename, $anchor)"/>)</xsl:when>
	      <xsl:otherwise><xsl:value-of select="concat($filename, $anchor)"/></xsl:otherwise>
	    </xsl:choose>
	  </xlink>
	</span>
      </xsl:when>
      <!-- Unknown Protocol -->
      <xsl:otherwise>
	<xsl:message>XSL-WARNING: unknown protocol '<xsl:value-of select="$protocol"/>' in <xsl:value-of select="urefurl"/>, using as-is</xsl:message>
	<span tag="hyperlink">
	  <xlink>
	    <xsl:attribute name="ref">
	      <xsl:value-of select="urefurl"/>
	    </xsl:attribute>
	    <xsl:choose>
	      <xsl:when test="count(child::urefreplacement)"><xsl:apply-templates select="urefreplacement"/></xsl:when>
	      <xsl:when test="count(child::urefdesc)"><xsl:apply-templates select="urefdesc"/> (<xsl:value-of select="urefurl"/>)</xsl:when>
	      <xsl:otherwise><xsl:value-of select="urefurl"/></xsl:otherwise>
	    </xsl:choose>
	  </xlink>
	</span>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>
  <!-- }}} -->

  <!-- {{{ inline images -->
  <xsl:template match="image">
    <!-- <xlink ref="error:DEADEND"> -->
    <image>
      <xsl:attribute name="file">
	<xsl:value-of select="."/>.<xsl:value-of select="@extension"/>
      </xsl:attribute>
      [<xsl:value-of select="@alttext"/>]
    </image>
    <!-- </xlink> -->
  </xsl:template>
  <!-- }}} -->

  <!-- {{{ table handling -->

  <!-- {{{ simple definition tables -->
  <xsl:template match="tableterm">
    <span tag="tableterm">
      <xsl:apply-templates/>
    </span>
    <breakline/>
  </xsl:template>

  <xsl:template match="tableitem/item/para">
    <span tag="tableitem">
      <xsl:apply-templates/>
    </span>
    <breakline/>
  </xsl:template>
  <!-- }}} -->

  <!-- {{{ multicolumn tables -->
  <xsl:template match="multitable">
    <breakline/>
    <span tag="multitable">
      <xsl:apply-templates/>
    </span>
  </xsl:template>

  <xsl:template match="multitable/row">
    <xsl:apply-templates/>
    <breakline/>
  </xsl:template>

  <xsl:template match="multitable/row/entry">
    <!-- Spanning each entry in a new line is disabled -->
    <span>
      <!-- <xsl:attribute name="tag"> -->
	<!-- <xsl:text>table_entry_</xsl:text><xsl:number/> -->
      <!-- </xsl:attribute> -->
      <xsl:apply-templates/>
      <xsl:if test="not(position()=last())">
        <xsl:text> </xsl:text>
      </xsl:if>
    </span>
    <!-- <newline/> -->
  </xsl:template>
  <!-- }}} -->

  <!-- }}} -->

  <!-- {{{ indice generation -->
  <xsl:template match="para/indexterm">
    <anchor>
      <xsl:attribute name="name">
	<xsl:value-of select="@index"/><xsl:text>index-</xsl:text><xsl:number level="any"/>
      </xsl:attribute>
    </anchor>
  </xsl:template>

  <xsl:template match="printindex">
    <xsl:variable name="type" select="."/>
    <xsl:for-each select="//para/indexterm[@index=$type]">
      <xsl:sort/>
      <span tag="hyperlink">
	<xlink>
	  <xsl:attribute name="ref">
	    <xsl:text>file:#</xsl:text><xsl:value-of select="$type"/><xsl:text>index-</xsl:text><xsl:number level="any"/>
	  </xsl:attribute>
	  <xsl:apply-templates/>
	</xlink>
      </span>
      <keep-space>    (<xsl:value-of select="../../title"/>)</keep-space>
      <breakline/>
    </xsl:for-each>
  </xsl:template>

  <xsl:template match="para/printplainindex">
    <xsl:variable name="type" select="."/>
    <xsl:for-each select="//para/indexterm[@index=$type]">
      <xsl:sort/>
      <span tag="hyperlink">
	<xlink>
	  <xsl:attribute name="ref">
	    <xsl:text>file:#</xsl:text><xsl:value-of select="$type"/><xsl:text>index-</xsl:text><xsl:number level="any"/>
	  </xsl:attribute>
	  <xsl:apply-templates/>
	</xlink>
      </span>
      <xsl:if test="not(position()=last())">
	<breakline/>
      </xsl:if>
    </xsl:for-each>
  </xsl:template>
  <!-- }}} -->

</xsl:stylesheet>
<!-- vim: set fdm=marker: -->
