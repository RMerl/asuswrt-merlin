config_add_mib(DISMAN-EVENT-MIB)

/*
 * wrapper for the original disman event mib implementation code files 
 */
config_require(disman/mteTriggerTable)
config_require(disman/mteTriggerDeltaTable)
config_require(disman/mteTriggerExistenceTable)
config_require(disman/mteTriggerBooleanTable)
config_require(disman/mteTriggerThresholdTable)
config_require(disman/mteObjectsTable)
config_require(disman/mteEventTable)
config_require(disman/mteEventNotificationTable)

/*
 * conflicts with the new implementation
 */
config_exclude(disman/event/mteScalars)
config_exclude(disman/event/mteTrigger)
config_exclude(disman/event/mteTriggerTable)
config_exclude(disman/event/mteTriggerDeltaTable)
config_exclude(disman/event/mteTriggerExistenceTable)
config_exclude(disman/event/mteTriggerBooleanTable)
config_exclude(disman/event/mteTriggerThresholdTable)
config_exclude(disman/event/mteTriggerConf)
config_exclude(disman/event/mteEvent)
config_exclude(disman/event/mteEventTable)
config_exclude(disman/event/mteEventSetTable)
config_exclude(disman/event/mteEventNotificationTable)
config_exclude(disman/event/mteEventConf)
config_exclude(disman/event/mteObjects)
config_exclude(disman/event/mteObjectsTable)
config_exclude(disman/event/mteObjectsConf)
