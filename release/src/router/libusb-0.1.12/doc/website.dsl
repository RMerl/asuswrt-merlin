<!DOCTYPE style-sheet PUBLIC "-//James Clark//DTD DSSSL Style Sheet//EN" [
<!ENTITY docbook.dsl PUBLIC "-//Norman Walsh//DOCUMENT DocBook HTML Stylesheet//EN" CDATA DSSSL>
]>

<!-- 

Document: website.dsl
Version: 0.2

Author: Johannes Erdfelt <johannes@erdfelt.com>

This stylesheet handles the website (html) and rtf/ps (print) stylesheets

-->

<style-sheet>

<style-specification id="print" use="docbook">
<style-specification-body> ;; ==============================
;; customize the print stylesheet
;; ==============================

(declare-characteristic preserve-sdata?
  ;; this is necessary because right now jadetex does not understand
  ;; symbolic entities, whereas things work well with numeric entities.
  "UNREGISTERED::James Clark//Characteristic::preserve-sdata?"
  #f)

(define %generate-article-toc%
  ;; Should a Table of Contents be produced for Articles?
  #t)

(define (toc-depth nd)
  2)

(define %generate-article-titlepage-on-separate-page%
  ;; Should the article title page be on a separate page?
  #t)

(define %section-autolabel%
  ;; Are sections enumerated?
  #t)

(define %footnote-ulinks%
  ;; Generate footnotes for ULinks?
  #f)

(define %bop-footnotes%
  ;; Make "bottom-of-page" footnotes?
  #f)

(define %body-start-indent%
  ;; Default indent of body text
  0pi)

(define %para-indent-firstpara%
  ;; First line start-indent for the first paragraph
  0pt)

(define %para-indent%
  ;; First line start-indent for paragraphs (other than the first)
  0pt)

(define %block-start-indent%
  ;; Extra start-indent for block-elements
  0pt)

(define formal-object-float
  ;; Do formal objects float?
  #t)

(define %hyphenation%
  ;; Allow automatic hyphenation?
  #t)

(define %admon-graphics%
  ;; Use graphics in admonitions?
  #f)

</style-specification-body>
</style-specification>


<!--
;; ===================================================
;; customize the html stylesheet; borrowed from Cygnus
;; at http://sourceware.cygnus.com/ (cygnus-both.dsl)
;; ===================================================
-->


<style-specification id="html" use="docbook">
<style-specification-body>


;; =========================
;; Indexes

;; Returns the depth of the auto-generated TOC (table of contents) that
;; should be made at the nd-level
(define (toc-depth nd)
  (if (string=? (gi nd) "book")
      2 ; the depth of the top-level TOC
      2 ; the depth of all other TOCs
      ))

(define %page-n-columns%
  ;; Sets the number of columns on each page
  2)

(define %generate-article-toc% 
  #t)


(define %header-navigation%
  #t)

(define %footer-navigation%
  #t)

(define %gentext-nav-use-tables%
  #t)

(define %gentext-nav-tblwidth% 
  "100%")


(define %indent-programlisting-lines%
  "    ")

(define %indent-screen-lines%
  "    ")

(define %shade-verbatim%  
  #t)

(define ($shade-verbatim-attr$)
  (list
   (list "BORDER" "0")
   (list "BGCOLOR" "#E0E0E0")
   (list "WIDTH" ($table-width$))))

(define %callout-default-col% 
  70)

(define biblio-number
  #t)

(define %graphic-default-extension% 
  "jpg")

(define %graphic-extensions% 
  '("gif" "jpg" "jpeg" "png" "tif" "tiff" "eps" "epsf"))

(define %stylesheet%
  "/base.css")

(define %stylesheet-type%
  "text/css")

(define %use-id-as-filename%
  #t)

(define use-output-dir
  #t)

(define %output-dir%
  "html")

(define %html-ext% 
  ".html")

(define %root-filename%
  "index")

(define %html-use-lang-in-filename% 
  #f)

(define %html40%
  #t)

(define %fix-para-wrappers%
  #t)

(define %section-autolabel%
  #t)

(define (chunk-skip-first-element-list)
  '())

</style-specification-body>
</style-specification>

<external-specification id="docbook" document="docbook.dsl">

</style-sheet>

