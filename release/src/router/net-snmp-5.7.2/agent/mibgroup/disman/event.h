config_add_mib(DISMAN-EVENT-MIB)

/*
 * wrapper for the new disman event mib implementation code files 
 */
config_require(disman/event/mteScalars)
config_require(disman/event/mteTrigger)
config_require(disman/event/mteTriggerTable)
config_require(disman/event/mteTriggerDeltaTable)
config_require(disman/event/mteTriggerExistenceTable)
config_require(disman/event/mteTriggerBooleanTable)
config_require(disman/event/mteTriggerThresholdTable)
config_require(disman/event/mteTriggerConf)
config_require(disman/event/mteEvent)
config_require(disman/event/mteEventTable)
config_require(disman/event/mteEventSetTable)
config_require(disman/event/mteEventNotificationTable)
config_require(disman/event/mteEventConf)
config_require(disman/event/mteObjects)
config_require(disman/event/mteObjectsTable)
config_require(disman/event/mteObjectsConf)

/*
 * conflicts with the previous implementation
 */
config_exclude(disman/mteTriggerTable)
config_exclude(disman/mteTriggerDeltaTable)
config_exclude(disman/mteTriggerExistenceTable)
config_exclude(disman/mteTriggerBooleanTable)
config_exclude(disman/mteTriggerThresholdTable)
config_exclude(disman/mteObjectsTable)
config_exclude(disman/mteEventTable)
config_exclude(disman/mteEventNotificationTable)

