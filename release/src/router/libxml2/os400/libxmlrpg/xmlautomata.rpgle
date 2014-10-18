      * Summary: API to build regexp automata
      * Description: the API to build regexp automata
      *
      * Copy: See Copyright for the status of this software.
      *
      * Author: Patrick Monnerat <pm@datasphere.ch>, DATASPHERE S.A.

      /if not defined(XML_AUTOMATA_H__)
      /define XML_AUTOMATA_H__

      /include "libxmlrpg/xmlversion"
      /include "libxmlrpg/tree"

      /if defined(LIBXML_REGEXP_ENABLED)
      /if defined(LIBXML_AUTOMATA_ENABLED)

      /include "libxmlrpg/xmlregexp"

      * xmlAutomataPtr:
      *
      * A libxml automata description, It can be compiled into a regexp

     d xmlAutomataPtr  s               *   based(######typedef######)

      * xmlAutomataStatePtr:
      *
      * A state int the automata description,

     d xmlAutomataStatePtr...
     d                 s               *   based(######typedef######)

      * Building API

     d xmlNewAutomata  pr                  extproc('xmlNewAutomata')
     d                                     like(xmlAutomataPtr)

     d xmlFreeAutomata...
     d                 pr                  extproc('xmlFreeAutomata')
     d  am                                 value like(xmlAutomataPtr)

     d xmlAutomataGetInitState...
     d                 pr                  extproc('xmlAutomataGetInitState')
     d                                     like(xmlAutomataStatePtr)
     d  am                                 value like(xmlAutomataPtr)

     d xmlAutomataSetFinalState...
     d                 pr            10i 0 extproc('xmlAutomataSetFinalState')
     d  am                                 value like(xmlAutomataPtr)
     d  state                              value like(xmlAutomataStatePtr)

     d xmlAutomataNewState...
     d                 pr                  extproc('xmlAutomataNewState')
     d                                     like(xmlAutomataStatePtr)
     d  am                                 value like(xmlAutomataPtr)

     d xmlAutomataNewTransition...
     d                 pr                  extproc('xmlAutomataNewTransition')
     d                                     like(xmlAutomataStatePtr)
     d  am                                 value like(xmlAutomataPtr)
     d  from                               value like(xmlAutomataStatePtr)
     d  to                                 value like(xmlAutomataStatePtr)
     d  token                          *   value options(*string)               const xmlChar *
     d  data                           *   value options(*string)               void *

     d xmlAutomataNewTransition2...
     d                 pr                  extproc('xmlAutomataNewTransition2')
     d                                     like(xmlAutomataStatePtr)
     d  am                                 value like(xmlAutomataPtr)
     d  from                               value like(xmlAutomataStatePtr)
     d  to                                 value like(xmlAutomataStatePtr)
     d  token                          *   value options(*string)               const xmlChar *
     d  token2                         *   value options(*string)               const xmlChar *
     d  data                           *   value options(*string)               void *

     d xmlAutomataNewNegTrans...
     d                 pr                  extproc('xmlAutomataNewNegTrans')
     d                                     like(xmlAutomataStatePtr)
     d  am                                 value like(xmlAutomataPtr)
     d  from                               value like(xmlAutomataStatePtr)
     d  to                                 value like(xmlAutomataStatePtr)
     d  token                          *   value options(*string)               const xmlChar *
     d  token2                         *   value options(*string)               const xmlChar *
     d  data                           *   value options(*string)               void *

     d xmlAutomataNewCountTrans...
     d                 pr                  extproc('xmlAutomataNewCountTrans')
     d                                     like(xmlAutomataStatePtr)
     d  am                                 value like(xmlAutomataPtr)
     d  from                               value like(xmlAutomataStatePtr)
     d  to                                 value like(xmlAutomataStatePtr)
     d  token                          *   value options(*string)               const xmlChar *
     d  min                          10i 0 value
     d  max                          10i 0 value
     d  data                           *   value options(*string)               void *

     d xmlAutomataNewCountTrans2...
     d                 pr                  extproc('xmlAutomataNewCountTrans2')
     d                                     like(xmlAutomataStatePtr)
     d  am                                 value like(xmlAutomataPtr)
     d  from                               value like(xmlAutomataStatePtr)
     d  to                                 value like(xmlAutomataStatePtr)
     d  token                          *   value options(*string)               const xmlChar *
     d  token2                         *   value options(*string)               const xmlChar *
     d  min                          10i 0 value
     d  max                          10i 0 value
     d  data                           *   value options(*string)               void *

     d xmlAutomataNewOnceTrans...
     d                 pr                  extproc('xmlAutomataNewOnceTrans')
     d                                     like(xmlAutomataStatePtr)
     d  am                                 value like(xmlAutomataPtr)
     d  from                               value like(xmlAutomataStatePtr)
     d  to                                 value like(xmlAutomataStatePtr)
     d  token                          *   value options(*string)               const xmlChar *
     d  min                          10i 0 value
     d  max                          10i 0 value
     d  data                           *   value options(*string)               void *

     d xmlAutomataNewOnceTrans2...
     d                 pr                  extproc('xmlAutomataNewOnceTrans2')
     d                                     like(xmlAutomataStatePtr)
     d  am                                 value like(xmlAutomataPtr)
     d  from                               value like(xmlAutomataStatePtr)
     d  to                                 value like(xmlAutomataStatePtr)
     d  token                          *   value options(*string)               const xmlChar *
     d  token2                         *   value options(*string)               const xmlChar *
     d  min                          10i 0 value
     d  max                          10i 0 value
     d  data                           *   value options(*string)               void *

     d xmlAutomataNewAllTrans...
     d                 pr                  extproc('xmlAutomataNewAllTrans')
     d                                     like(xmlAutomataStatePtr)
     d  am                                 value like(xmlAutomataPtr)
     d  from                               value like(xmlAutomataStatePtr)
     d  to                                 value like(xmlAutomataStatePtr)
     d  lax                          10i 0 value

     d xmlAutomataNewEpsilon...
     d                 pr                  extproc('xmlAutomataNewEpsilon')
     d                                     like(xmlAutomataStatePtr)
     d  am                                 value like(xmlAutomataPtr)
     d  from                               value like(xmlAutomataStatePtr)
     d  to                                 value like(xmlAutomataStatePtr)

     d xmlAutomataNewCountedTrans...
     d                 pr                  extproc('xmlAutomataNewCountedTrans')
     d                                     like(xmlAutomataStatePtr)
     d  am                                 value like(xmlAutomataPtr)
     d  from                               value like(xmlAutomataStatePtr)
     d  to                                 value like(xmlAutomataStatePtr)
     d  counter                      10i 0 value

     d xmlAutomataNewCounterTrans...
     d                 pr                  extproc('xmlAutomataNewCounterTrans')
     d                                     like(xmlAutomataStatePtr)
     d  am                                 value like(xmlAutomataPtr)
     d  from                               value like(xmlAutomataStatePtr)
     d  to                                 value like(xmlAutomataStatePtr)
     d  counter                      10i 0 value

     d xmlAutomataNewCounter...
     d                 pr            10i 0 extproc('xmlAutomataNewCounter')
     d  am                                 value like(xmlAutomataPtr)
     d  min                          10i 0 value
     d  max                          10i 0 value

     d xmlAutomataCompile...
     d                 pr                  extproc('xmlAutomataCompile')
     d                                     like(xmlRegexpPtr)
     d  am                                 value like(xmlAutomataPtr)

     d xmlAutomataIsDeterminist...
     d                 pr            10i 0 extproc('xmlAutomataIsDeterminist')
     d  am                                 value like(xmlAutomataPtr)

      /endif                                                                    AUTOMATA_ENABLED
      /endif                                                                    LIBXML_REGEXP_ENABLD
      /endif                                                                    XML_AUTOMATA_H__
