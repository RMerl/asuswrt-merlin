cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0015 NEW)

# Project Definition
project(odhcp6c C)
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -std=c99")
add_definitions(-D_GNU_SOURCE -Wall -Werror -Wextra -pedantic)

if(${EXT_PREFIX_CLASS})
	add_definitions(-DEXT_PREFIX_CLASS=${EXT_PREFIX_CLASS})
endif(${EXT_PREFIX_CLASS})

if(${EXT_CER_ID})
	add_definitions(-DEXT_CER_ID=${EXT_CER_ID})
endif(${EXT_CER_ID})

add_executable(odhcp6c src/odhcp6c.c src/dhcpv6.c src/ra.c src/script.c src/md5.c)
target_link_libraries(odhcp6c resolv)

# Installation
install(TARGETS odhcp6c DESTINATION sbin/)

# Packaging information
set(CPACK_PACKAGE_VERSION "1")
set(CPACK_PACKAGE_CONTACT "Steven Barth <steven@midlink.org>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "6ac")
set(CPACK_GENERATOR "DEB;RPM;STGZ")
set(CPACK_STRIP_FILES true)

SET(CPACK_DEBIAN_PACKAGE_VERSION ${CPACK_PACKAGE_VERSION})
set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}_${CPACK_DEBIAN_PACKAGE_VERSION}")

include(CPack)

