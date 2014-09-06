config_require(hardware/sensors/hw_sensors)

#if defined(solaris2)
# if defined(HAVE_PICL_H)
config_require(hardware/sensors/picld_sensors)
# else
config_require(hardware/sensors/kstat_sensors)
# endif
#else
#  if defined(NETSNMP_USE_SENSORS_V3)
config_require(hardware/sensors/lmsensors_v3)
#  else
config_require(hardware/sensors/lmsensors_v2)
#  endif
#endif

/* config_require(hardware/sensors/dummy_sensors) */
