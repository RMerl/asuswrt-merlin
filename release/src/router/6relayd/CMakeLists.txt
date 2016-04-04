cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0015 NEW)

# Project Definition
project(6relayd C)

set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -std=c99")
add_definitions(-D_GNU_SOURCE -Wall -Werror -Wextra -Wno-extended-offsetof -pedantic)

add_executable(6relayd src/6relayd.c src/router.c src/dhcpv6.c src/ndp.c src/md5.c src/dhcpv6-ia.c)
target_link_libraries(6relayd resolv)

# Installation
install(TARGETS 6relayd DESTINATION sbin/)

# Packaging information
set(CPACK_PACKAGE_VERSION "1")
set(CPACK_PACKAGE_CONTACT "Steven Barth <steven@midlink.org>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "6relayd")
set(CPACK_GENERATOR "DEB;RPM;STGZ")
set(CPACK_STRIP_FILES true)

SET(CPACK_DEBIAN_PACKAGE_VERSION ${CPACK_PACKAGE_VERSION})
set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}_${CPACK_DEBIAN_PACKAGE_VERSION}")

include(CPack)

