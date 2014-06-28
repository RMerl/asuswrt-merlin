cmake_minimum_required(VERSION 2.6)

PROJECT(uqmi C)
ADD_DEFINITIONS(-Os -ggdb -Wall -Werror --std=gnu99 -Wmissing-declarations)

SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

SET(SOURCES main.c dev.c commands.c qmi-message.c)

SET(LIBS ubox)

IF(DEBUG_PACKET)
  ADD_DEFINITIONS(-DDEBUG_PACKET)
ENDIF()

IF(DEBUG)
  ADD_DEFINITIONS(-DDEBUG -g3)
ENDIF()

SET(service_headers)
SET(service_sources)
FOREACH(service ctl dms nas pds wds wms)
	ADD_CUSTOM_COMMAND(
		OUTPUT qmi-message-${service}.h
		COMMAND ./data/gen-header.pl ${service}_ ./data/qmi-service-${service}.json > qmi-message-${service}.h
		DEPENDS ./data/gen-header.pl ./data/qmi-service-${service}.json ./data/gen-common.pm
	)
	SET(service_headers ${service_headers} qmi-message-${service}.h)
	ADD_CUSTOM_COMMAND(
		OUTPUT qmi-message-${service}.c
		COMMAND ./data/gen-code.pl ${service}_ ./data/qmi-service-${service}.json > qmi-message-${service}.c
		DEPENDS ./data/gen-code.pl ./data/qmi-service-${service}.json ./data/gen-common.pm
	)
	SET(service_sources ${service_sources} qmi-message-${service}.c)
	set_property(SOURCE qmi-message-${service}.c PROPERTY COMPILE_FLAGS "-Wno-unused")
ENDFOREACH()

ADD_CUSTOM_COMMAND(
	OUTPUT qmi-errors.c
	COMMAND ./data/gen-error-list.pl ./qmi-errors.h > qmi-errors.c
	DEPENDS ./data/gen-error-list.pl ./qmi-errors.h
)
ADD_CUSTOM_TARGET(gen-errors DEPENDS qmi-errors.c)
ADD_CUSTOM_TARGET(gen-headers DEPENDS ${service_headers})

ADD_EXECUTABLE(uqmi ${SOURCES} ${service_sources})
ADD_DEPENDENCIES(uqmi gen-headers gen-errors)

TARGET_LINK_LIBRARIES(uqmi ${LIBS})

INSTALL(TARGETS uqmi
	RUNTIME DESTINATION sbin
)
