      * Summary: Unicode character APIs
      * Description: API for the Unicode character APIs
      *
      * Copy: See Copyright for the status of this software.
      *
      * Author: Patrick Monnerat <pm@datasphere.ch>, DATASPHERE S.A.

      /if not defined(XML_UNICODE_H__)
      /define XML_UNICODE_H__

      /include "libxmlrpg/xmlversion"

      /if defined(LIBXML_UNICODE_ENABLED)

     d xmlUCSIsAegeanNumbers...
     d                 pr            10i 0 extproc('xmlUCSIsAegeanNumbers')
     d  code                         10i 0 value

     d xmlUCSIsAlphabeticPresentationForms...
     d                 pr            10i 0 extproc(
     d                                     'xmlUCSIsAlphabeticPresentationForms'
     d                                     )
     d  code                         10i 0 value

     d xmlUCSIsArabic  pr            10i 0 extproc('xmlUCSIsArabic')
     d  code                         10i 0 value

     d xmlUCSIsArabicPresentationFormsA...
     d                 pr            10i 0 extproc(
     d                                     'xmlUCSIsArabicPresentationFormsA')
     d  code                         10i 0 value

     d xmlUCSIsArabicPresentationFormsB...
     d                 pr            10i 0 extproc(
     d                                     'xmlUCSIsArabicPresentationFormsB')
     d  code                         10i 0 value

     d xmlUCSIsArmenian...
     d                 pr            10i 0 extproc('xmlUCSIsArmenian')
     d  code                         10i 0 value

     d xmlUCSIsArrows  pr            10i 0 extproc('xmlUCSIsArrows')
     d  code                         10i 0 value

     d xmlUCSIsBasicLatin...
     d                 pr            10i 0 extproc('xmlUCSIsBasicLatin')
     d  code                         10i 0 value

     d xmlUCSIsBengali...
     d                 pr            10i 0 extproc('xmlUCSIsBengali')
     d  code                         10i 0 value

     d xmlUCSIsBlockElements...
     d                 pr            10i 0 extproc('xmlUCSIsBlockElements')
     d  code                         10i 0 value

     d xmlUCSIsBopomofo...
     d                 pr            10i 0 extproc('xmlUCSIsBopomofo')
     d  code                         10i 0 value

     d xmlUCSIsBopomofoExtended...
     d                 pr            10i 0 extproc('xmlUCSIsBopomofoExtended')
     d  code                         10i 0 value

     d xmlUCSIsBoxDrawing...
     d                 pr            10i 0 extproc('xmlUCSIsBoxDrawing')
     d  code                         10i 0 value

     d xmlUCSIsBraillePatterns...
     d                 pr            10i 0 extproc('xmlUCSIsBraillePatterns')
     d  code                         10i 0 value

     d xmlUCSIsBuhid   pr            10i 0 extproc('xmlUCSIsBuhid')
     d  code                         10i 0 value

     d xmlUCSIsByzantineMusicalSymbols...
     d                 pr            10i 0 extproc(
     d                                     'xmlUCSIsByzantineMusicalSymbols')
     d  code                         10i 0 value

     d xmlUCSIsCJKCompatibility...
     d                 pr            10i 0 extproc('xmlUCSIsCJKCompatibility')
     d  code                         10i 0 value

     d xmlUCSIsCJKCompatibilityForms...
     d                 pr            10i 0 extproc(
     d                                     'xmlUCSIsCJKCompatibilityForms')
     d  code                         10i 0 value

     d xmlUCSIsCJKCompatibilityIdeographs...
     d                 pr            10i 0 extproc(
     d                                     'xmlUCSIsCJKCompatibilityIdeographs')
     d  code                         10i 0 value

     d xmlUCSIsCJKCompatibilityIdeographsSupplement...
     d                 pr            10i 0 extproc('xmlUCSIsCJKCompatibilityIde-
     d                                     ographsSupplement')
     d  code                         10i 0 value

     d xmlUCSIsCJKRadicalsSupplement...
     d                 pr            10i 0 extproc(
     d                                     'xmlUCSIsCJKRadicalsSupplement')
     d  code                         10i 0 value

     d xmlUCSIsCJKSymbolsandPunctuation...
     d                 pr            10i 0 extproc(
     d                                     'xmlUCSIsCJKSymbolsandPunctuation')
     d  code                         10i 0 value

     d xmlUCSIsCJKUnifiedIdeographs...
     d                 pr            10i 0 extproc(
     d                                     'xmlUCSIsCJKUnifiedIdeographs')
     d  code                         10i 0 value

     d xmlUCSIsCJKUnifiedIdeographsExtensionA...
     d                 pr            10i 0 extproc('xmlUCSIsCJKUnifiedIdeograph-
     d                                     sExtensionA')
     d  code                         10i 0 value

     d xmlUCSIsCJKUnifiedIdeographsExtensionB...
     d                 pr            10i 0 extproc('xmlUCSIsCJKUnifiedIdeograph-
     d                                     sExtensionB')
     d  code                         10i 0 value

     d xmlUCSIsCherokee...
     d                 pr            10i 0 extproc('xmlUCSIsCherokee')
     d  code                         10i 0 value

     d xmlUCSIsCombiningDiacriticalMarks...
     d                 pr            10i 0 extproc(
     d                                     'xmlUCSIsCombiningDiacriticalMarks')
     d  code                         10i 0 value

     d xmlUCSIsCombiningDiacriticalMarksforSymbols...
     d                 pr            10i 0 extproc('xmlUCSIsCombiningDiacritica-
     d                                     lMarksforSymbols')
     d  code                         10i 0 value

     d xmlUCSIsCombiningHalfMarks...
     d                 pr            10i 0 extproc('xmlUCSIsCombiningHalfMarks')
     d  code                         10i 0 value

     d xmlUCSIsCombiningMarksforSymbols...
     d                 pr            10i 0 extproc(
     d                                     'xmlUCSIsCombiningMarksforSymbols')
     d  code                         10i 0 value

     d xmlUCSIsControlPictures...
     d                 pr            10i 0 extproc('xmlUCSIsControlPictures')
     d  code                         10i 0 value

     d xmlUCSIsCurrencySymbols...
     d                 pr            10i 0 extproc('xmlUCSIsCurrencySymbols')
     d  code                         10i 0 value

     d xmlUCSIsCypriotSyllabary...
     d                 pr            10i 0 extproc('xmlUCSIsCypriotSyllabary')
     d  code                         10i 0 value

     d xmlUCSIsCyrillic...
     d                 pr            10i 0 extproc('xmlUCSIsCyrillic')
     d  code                         10i 0 value

     d xmlUCSIsCyrillicSupplement...
     d                 pr            10i 0 extproc('xmlUCSIsCyrillicSupplement')
     d  code                         10i 0 value

     d xmlUCSIsDeseret...
     d                 pr            10i 0 extproc('xmlUCSIsDeseret')
     d  code                         10i 0 value

     d xmlUCSIsDevanagari...
     d                 pr            10i 0 extproc('xmlUCSIsDevanagari')
     d  code                         10i 0 value

     d xmlUCSIsDingbats...
     d                 pr            10i 0 extproc('xmlUCSIsDingbats')
     d  code                         10i 0 value

     d xmlUCSIsEnclosedAlphanumerics...
     d                 pr            10i 0 extproc(
     d                                     'xmlUCSIsEnclosedAlphanumerics')
     d  code                         10i 0 value

     d xmlUCSIsEnclosedCJKLettersandMonths...
     d                 pr            10i 0 extproc(
     d                                     'xmlUCSIsEnclosedCJKLettersandMonths'
     d                                     )
     d  code                         10i 0 value

     d xmlUCSIsEthiopic...
     d                 pr            10i 0 extproc('xmlUCSIsEthiopic')
     d  code                         10i 0 value

     d xmlUCSIsGeneralPunctuation...
     d                 pr            10i 0 extproc('xmlUCSIsGeneralPunctuation')
     d  code                         10i 0 value

     d xmlUCSIsGeometricShapes...
     d                 pr            10i 0 extproc('xmlUCSIsGeometricShapes')
     d  code                         10i 0 value

     d xmlUCSIsGeorgian...
     d                 pr            10i 0 extproc('xmlUCSIsGeorgian')
     d  code                         10i 0 value

     d xmlUCSIsGothic  pr            10i 0 extproc('xmlUCSIsGothic')
     d  code                         10i 0 value

     d xmlUCSIsGreek   pr            10i 0 extproc('xmlUCSIsGreek')
     d  code                         10i 0 value

     d xmlUCSIsGreekExtended...
     d                 pr            10i 0 extproc('xmlUCSIsGreekExtended')
     d  code                         10i 0 value

     d xmlUCSIsGreekandCoptic...
     d                 pr            10i 0 extproc('xmlUCSIsGreekandCoptic')
     d  code                         10i 0 value

     d xmlUCSIsGujarati...
     d                 pr            10i 0 extproc('xmlUCSIsGujarati')
     d  code                         10i 0 value

     d xmlUCSIsGurmukhi...
     d                 pr            10i 0 extproc('xmlUCSIsGurmukhi')
     d  code                         10i 0 value

     d xmlUCSIsHalfwidthandFullwidthForms...
     d                 pr            10i 0 extproc(
     d                                     'xmlUCSIsHalfwidthandFullwidthForms')
     d  code                         10i 0 value

     d xmlUCSIsHangulCompatibilityJamo...
     d                 pr            10i 0 extproc(
     d                                     'xmlUCSIsHangulCompatibilityJamo')
     d  code                         10i 0 value

     d xmlUCSIsHangulJamo...
     d                 pr            10i 0 extproc('xmlUCSIsHangulJamo')
     d  code                         10i 0 value

     d xmlUCSIsHangulSyllables...
     d                 pr            10i 0 extproc('xmlUCSIsHangulSyllables')
     d  code                         10i 0 value

     d xmlUCSIsHanunoo...
     d                 pr            10i 0 extproc('xmlUCSIsHanunoo')
     d  code                         10i 0 value

     d xmlUCSIsHebrew  pr            10i 0 extproc('xmlUCSIsHebrew')
     d  code                         10i 0 value

     d xmlUCSIsHighPrivateUseSurrogates...
     d                 pr            10i 0 extproc(
     d                                     'xmlUCSIsHighPrivateUseSurrogates')
     d  code                         10i 0 value

     d xmlUCSIsHighSurrogates...
     d                 pr            10i 0 extproc('xmlUCSIsHighSurrogates')
     d  code                         10i 0 value

     d xmlUCSIsHiragana...
     d                 pr            10i 0 extproc('xmlUCSIsHiragana')
     d  code                         10i 0 value

     d xmlUCSIsIPAExtensions...
     d                 pr            10i 0 extproc('xmlUCSIsIPAExtensions')
     d  code                         10i 0 value

     d xmlUCSIsIdeographicDescriptionCharacters...
     d                 pr            10i 0 extproc('xmlUCSIsIdeographicDescript-
     d                                     ionCharacters')
     d  code                         10i 0 value

     d xmlUCSIsKanbun  pr            10i 0 extproc('xmlUCSIsKanbun')
     d  code                         10i 0 value

     d xmlUCSIsKangxiRadicals...
     d                 pr            10i 0 extproc('xmlUCSIsKangxiRadicals')
     d  code                         10i 0 value

     d xmlUCSIsKannada...
     d                 pr            10i 0 extproc('xmlUCSIsKannada')
     d  code                         10i 0 value

     d xmlUCSIsKatakana...
     d                 pr            10i 0 extproc('xmlUCSIsKatakana')
     d  code                         10i 0 value

     d xmlUCSIsKatakanaPhoneticExtensions...
     d                 pr            10i 0 extproc(
     d                                     'xmlUCSIsKatakanaPhoneticExtensions')
     d  code                         10i 0 value

     d xmlUCSIsKhmer   pr            10i 0 extproc('xmlUCSIsKhmer')
     d  code                         10i 0 value

     d xmlUCSIsKhmerSymbols...
     d                 pr            10i 0 extproc('xmlUCSIsKhmerSymbols')
     d  code                         10i 0 value

     d xmlUCSIsLao     pr            10i 0 extproc('xmlUCSIsLao')
     d  code                         10i 0 value

     d xmlUCSIsLatin1Supplement...
     d                 pr            10i 0 extproc('xmlUCSIsLatin1Supplement')
     d  code                         10i 0 value

     d xmlUCSIsLatinExtendedA...
     d                 pr            10i 0 extproc('xmlUCSIsLatinExtendedA')
     d  code                         10i 0 value

     d xmlUCSIsLatinExtendedB...
     d                 pr            10i 0 extproc('xmlUCSIsLatinExtendedB')
     d  code                         10i 0 value

     d xmlUCSIsLatinExtendedAdditional...
     d                 pr            10i 0 extproc(
     d                                     'xmlUCSIsLatinExtendedAdditional')
     d  code                         10i 0 value

     d xmlUCSIsLetterlikeSymbols...
     d                 pr            10i 0 extproc('xmlUCSIsLetterlikeSymbols')
     d  code                         10i 0 value

     d xmlUCSIsLimbu   pr            10i 0 extproc('xmlUCSIsLimbu')
     d  code                         10i 0 value

     d xmlUCSIsLinearBIdeograms...
     d                 pr            10i 0 extproc('xmlUCSIsLinearBIdeograms')
     d  code                         10i 0 value

     d xmlUCSIsLinearBSyllabary...
     d                 pr            10i 0 extproc('xmlUCSIsLinearBSyllabary')
     d  code                         10i 0 value

     d xmlUCSIsLowSurrogates...
     d                 pr            10i 0 extproc('xmlUCSIsLowSurrogates')
     d  code                         10i 0 value

     d xmlUCSIsMalayalam...
     d                 pr            10i 0 extproc('xmlUCSIsMalayalam')
     d  code                         10i 0 value

     d xmlUCSIsMathematicalAlphanumericSymbols...
     d                 pr            10i 0 extproc('xmlUCSIsMathematicalAlphanu-
     d                                     mericSymbols')
     d  code                         10i 0 value

     d xmlUCSIsMathematicalOperators...
     d                 pr            10i 0 extproc(
     d                                     'xmlUCSIsMathematicalOperators')
     d  code                         10i 0 value

     d xmlUCSIsMiscellaneousMathematicalSymbolsA...
     d                 pr            10i 0 extproc('xmlUCSIsMiscellaneousMathem-
     d                                     aticalSymbolsA')
     d  code                         10i 0 value

     d xmlUCSIsMiscellaneousMathematicalSymbolsB...
     d                 pr            10i 0 extproc('xmlUCSIsMiscellaneousMathem-
     d                                     aticalSymbolsB')
     d  code                         10i 0 value

     d xmlUCSIsMiscellaneousSymbols...
     d                 pr            10i 0 extproc(
     d                                     'xmlUCSIsMiscellaneousSymbols')
     d  code                         10i 0 value

     d xmlUCSIsMiscellaneousSymbolsandArrows...
     d                 pr            10i 0 extproc('xmlUCSIsMiscellaneousSymbol-
     d                                     sandArrows')
     d  code                         10i 0 value

     d xmlUCSIsMiscellaneousTechnical...
     d                 pr            10i 0 extproc(
     d                                     'xmlUCSIsMiscellaneousTechnical')
     d  code                         10i 0 value

     d xmlUCSIsMongolian...
     d                 pr            10i 0 extproc('xmlUCSIsMongolian')
     d  code                         10i 0 value

     d xmlUCSIsMusicalSymbols...
     d                 pr            10i 0 extproc('xmlUCSIsMusicalSymbols')
     d  code                         10i 0 value

     d xmlUCSIsMyanmar...
     d                 pr            10i 0 extproc('xmlUCSIsMyanmar')
     d  code                         10i 0 value

     d xmlUCSIsNumberForms...
     d                 pr            10i 0 extproc('xmlUCSIsNumberForms')
     d  code                         10i 0 value

     d xmlUCSIsOgham   pr            10i 0 extproc('xmlUCSIsOgham')
     d  code                         10i 0 value

     d xmlUCSIsOldItalic...
     d                 pr            10i 0 extproc('xmlUCSIsOldItalic')
     d  code                         10i 0 value

     d xmlUCSIsOpticalCharacterRecognition...
     d                 pr            10i 0 extproc(
     d                                     'xmlUCSIsOpticalCharacterRecognition'
     d                                     )
     d  code                         10i 0 value

     d xmlUCSIsOriya   pr            10i 0 extproc('xmlUCSIsOriya')
     d  code                         10i 0 value

     d xmlUCSIsOsmanya...
     d                 pr            10i 0 extproc('xmlUCSIsOsmanya')
     d  code                         10i 0 value

     d xmlUCSIsPhoneticExtensions...
     d                 pr            10i 0 extproc('xmlUCSIsPhoneticExtensions')
     d  code                         10i 0 value

     d xmlUCSIsPrivateUse...
     d                 pr            10i 0 extproc('xmlUCSIsPrivateUse')
     d  code                         10i 0 value

     d xmlUCSIsPrivateUseArea...
     d                 pr            10i 0 extproc('xmlUCSIsPrivateUseArea')
     d  code                         10i 0 value

     d xmlUCSIsRunic   pr            10i 0 extproc('xmlUCSIsRunic')
     d  code                         10i 0 value

     d xmlUCSIsShavian...
     d                 pr            10i 0 extproc('xmlUCSIsShavian')
     d  code                         10i 0 value

     d xmlUCSIsSinhala...
     d                 pr            10i 0 extproc('xmlUCSIsSinhala')
     d  code                         10i 0 value

     d xmlUCSIsSmallFormVariants...
     d                 pr            10i 0 extproc('xmlUCSIsSmallFormVariants')
     d  code                         10i 0 value

     d xmlUCSIsSpacingModifierLetters...
     d                 pr            10i 0 extproc(
     d                                     'xmlUCSIsSpacingModifierLetters')
     d  code                         10i 0 value

     d xmlUCSIsSpecials...
     d                 pr            10i 0 extproc('xmlUCSIsSpecials')
     d  code                         10i 0 value

     d xmlUCSIsSuperscriptsandSubscripts...
     d                 pr            10i 0 extproc(
     d                                     'xmlUCSIsSuperscriptsandSubscripts')
     d  code                         10i 0 value

     d xmlUCSIsSupplementalArrowsA...
     d                 pr            10i 0 extproc(
     d                                     'xmlUCSIsSupplementalArrowsA')
     d  code                         10i 0 value

     d xmlUCSIsSupplementalArrowsB...
     d                 pr            10i 0 extproc(
     d                                     'xmlUCSIsSupplementalArrowsB')
     d  code                         10i 0 value

     d xmlUCSIsSupplementalMathematicalOperators...
     d                 pr            10i 0 extproc('xmlUCSIsSupplementalMathema-
     d                                     ticalOperators')
     d  code                         10i 0 value

     d xmlUCSIsSupplementaryPrivateUseAreaA...
     d                 pr            10i 0 extproc('xmlUCSIsSupplementaryPrivat-
     d                                     eUseAreaA')
     d  code                         10i 0 value

     d xmlUCSIsSupplementaryPrivateUseAreaB...
     d                 pr            10i 0 extproc('xmlUCSIsSupplementaryPrivat-
     d                                     eUseAreaB')
     d  code                         10i 0 value

     d xmlUCSIsSyriac  pr            10i 0 extproc('xmlUCSIsSyriac')
     d  code                         10i 0 value

     d xmlUCSIsTagalog...
     d                 pr            10i 0 extproc('xmlUCSIsTagalog')
     d  code                         10i 0 value

     d xmlUCSIsTagbanwa...
     d                 pr            10i 0 extproc('xmlUCSIsTagbanwa')
     d  code                         10i 0 value

     d xmlUCSIsTags    pr            10i 0 extproc('xmlUCSIsTags')
     d  code                         10i 0 value

     d xmlUCSIsTaiLe   pr            10i 0 extproc('xmlUCSIsTaiLe')
     d  code                         10i 0 value

     d xmlUCSIsTaiXuanJingSymbols...
     d                 pr            10i 0 extproc('xmlUCSIsTaiXuanJingSymbols')
     d  code                         10i 0 value

     d xmlUCSIsTamil   pr            10i 0 extproc('xmlUCSIsTamil')
     d  code                         10i 0 value

     d xmlUCSIsTelugu  pr            10i 0 extproc('xmlUCSIsTelugu')
     d  code                         10i 0 value

     d xmlUCSIsThaana  pr            10i 0 extproc('xmlUCSIsThaana')
     d  code                         10i 0 value

     d xmlUCSIsThai    pr            10i 0 extproc('xmlUCSIsThai')
     d  code                         10i 0 value

     d xmlUCSIsTibetan...
     d                 pr            10i 0 extproc('xmlUCSIsTibetan')
     d  code                         10i 0 value

     d xmlUCSIsUgaritic...
     d                 pr            10i 0 extproc('xmlUCSIsUgaritic')
     d  code                         10i 0 value

     d xmlUCSIsUnifiedCanadianAboriginalSyllabics...
     d                 pr            10i 0 extproc('xmlUCSIsUnifiedCanadianAbor-
     d                                     iginalSyllabics')
     d  code                         10i 0 value

     d xmlUCSIsVariationSelectors...
     d                 pr            10i 0 extproc('xmlUCSIsVariationSelectors')
     d  code                         10i 0 value

     d xmlUCSIsVariationSelectorsSupplement...
     d                 pr            10i 0 extproc('xmlUCSIsVariationSelectorsS-
     d                                     upplement')
     d  code                         10i 0 value

     d xmlUCSIsYiRadicals...
     d                 pr            10i 0 extproc('xmlUCSIsYiRadicals')
     d  code                         10i 0 value

     d xmlUCSIsYiSyllables...
     d                 pr            10i 0 extproc('xmlUCSIsYiSyllables')
     d  code                         10i 0 value

     d xmlUCSIsYijingHexagramSymbols...
     d                 pr            10i 0 extproc(
     d                                     'xmlUCSIsYijingHexagramSymbols')
     d  code                         10i 0 value

     d xmlUCSIsBlock   pr            10i 0 extproc('xmlUCSIsBlock')
     d  code                         10i 0 value
     d  block                          *   value options(*string)               const char *

     d xmlUCSIsCatC    pr            10i 0 extproc('xmlUCSIsCatC')
     d  code                         10i 0 value

     d xmlUCSIsCatCc   pr            10i 0 extproc('xmlUCSIsCatCc')
     d  code                         10i 0 value

     d xmlUCSIsCatCf   pr            10i 0 extproc('xmlUCSIsCatCf')
     d  code                         10i 0 value

     d xmlUCSIsCatCo   pr            10i 0 extproc('xmlUCSIsCatCo')
     d  code                         10i 0 value

     d xmlUCSIsCatCs   pr            10i 0 extproc('xmlUCSIsCatCs')
     d  code                         10i 0 value

     d xmlUCSIsCatL    pr            10i 0 extproc('xmlUCSIsCatL')
     d  code                         10i 0 value

     d xmlUCSIsCatLl   pr            10i 0 extproc('xmlUCSIsCatLl')
     d  code                         10i 0 value

     d xmlUCSIsCatLm   pr            10i 0 extproc('xmlUCSIsCatLm')
     d  code                         10i 0 value

     d xmlUCSIsCatLo   pr            10i 0 extproc('xmlUCSIsCatLo')
     d  code                         10i 0 value

     d xmlUCSIsCatLt   pr            10i 0 extproc('xmlUCSIsCatLt')
     d  code                         10i 0 value

     d xmlUCSIsCatLu   pr            10i 0 extproc('xmlUCSIsCatLu')
     d  code                         10i 0 value

     d xmlUCSIsCatM    pr            10i 0 extproc('xmlUCSIsCatM')
     d  code                         10i 0 value

     d xmlUCSIsCatMc   pr            10i 0 extproc('xmlUCSIsCatMc')
     d  code                         10i 0 value

     d xmlUCSIsCatMe   pr            10i 0 extproc('xmlUCSIsCatMe')
     d  code                         10i 0 value

     d xmlUCSIsCatMn   pr            10i 0 extproc('xmlUCSIsCatMn')
     d  code                         10i 0 value

     d xmlUCSIsCatN    pr            10i 0 extproc('xmlUCSIsCatN')
     d  code                         10i 0 value

     d xmlUCSIsCatNd   pr            10i 0 extproc('xmlUCSIsCatNd')
     d  code                         10i 0 value

     d xmlUCSIsCatNl   pr            10i 0 extproc('xmlUCSIsCatNl')
     d  code                         10i 0 value

     d xmlUCSIsCatNo   pr            10i 0 extproc('xmlUCSIsCatNo')
     d  code                         10i 0 value

     d xmlUCSIsCatP    pr            10i 0 extproc('xmlUCSIsCatP')
     d  code                         10i 0 value

     d xmlUCSIsCatPc   pr            10i 0 extproc('xmlUCSIsCatPc')
     d  code                         10i 0 value

     d xmlUCSIsCatPd   pr            10i 0 extproc('xmlUCSIsCatPd')
     d  code                         10i 0 value

     d xmlUCSIsCatPe   pr            10i 0 extproc('xmlUCSIsCatPe')
     d  code                         10i 0 value

     d xmlUCSIsCatPf   pr            10i 0 extproc('xmlUCSIsCatPf')
     d  code                         10i 0 value

     d xmlUCSIsCatPi   pr            10i 0 extproc('xmlUCSIsCatPi')
     d  code                         10i 0 value

     d xmlUCSIsCatPo   pr            10i 0 extproc('xmlUCSIsCatPo')
     d  code                         10i 0 value

     d xmlUCSIsCatPs   pr            10i 0 extproc('xmlUCSIsCatPs')
     d  code                         10i 0 value

     d xmlUCSIsCatS    pr            10i 0 extproc('xmlUCSIsCatS')
     d  code                         10i 0 value

     d xmlUCSIsCatSc   pr            10i 0 extproc('xmlUCSIsCatSc')
     d  code                         10i 0 value

     d xmlUCSIsCatSk   pr            10i 0 extproc('xmlUCSIsCatSk')
     d  code                         10i 0 value

     d xmlUCSIsCatSm   pr            10i 0 extproc('xmlUCSIsCatSm')
     d  code                         10i 0 value

     d xmlUCSIsCatSo   pr            10i 0 extproc('xmlUCSIsCatSo')
     d  code                         10i 0 value

     d xmlUCSIsCatZ    pr            10i 0 extproc('xmlUCSIsCatZ')
     d  code                         10i 0 value

     d xmlUCSIsCatZl   pr            10i 0 extproc('xmlUCSIsCatZl')
     d  code                         10i 0 value

     d xmlUCSIsCatZp   pr            10i 0 extproc('xmlUCSIsCatZp')
     d  code                         10i 0 value

     d xmlUCSIsCatZs   pr            10i 0 extproc('xmlUCSIsCatZs')
     d  code                         10i 0 value

     d xmlUCSIsCat     pr            10i 0 extproc('xmlUCSIsCat')
     d  code                         10i 0 value
     d  cat                            *   value options(*string)               const char *

      /endif                                                                    LIBXML_UNICODE_ENBLD
      /endif                                                                    XML_UNICODE_H__
