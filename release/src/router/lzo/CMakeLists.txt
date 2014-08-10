#
# CMakeLists.txt --- a simple "cmake" file for building LZO
#
# This file is part of the LZO data compression library.
#
# Copyright (C) 1996-2014 Markus Franz Xaver Johannes Oberhumer
# All Rights Reserved.
#

#
# simple usage:
#     mkdir -p build && cd build && cmake .. && make
#
# another usage example:
#     mkdir -p build/release-i686
#     cd       build/release-i686
#     cmake ../.. -DENABLE_STATIC=0 -DENABLE_SHARED=1 \
#         -DCMAKE_C_COMPILER=gcc -DCMAKE_C_FLAGS="-m32 -march=i686" \
#         -DCMAKE_INSTALL_PREFIX=/opt/local/prefix-i686
#     make VERBOSE=1
#     make install
#

#
# init
#

cmake_minimum_required(VERSION 2.6 FATAL_ERROR)

# Disallow in-source builds. Note that you will still have to manually
# clean up a few files if you accidentally try an in-source build.
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_DISABLE_SOURCE_CHANGES  ON)
if(",${CMAKE_SOURCE_DIR}," STREQUAL ",${CMAKE_BINARY_DIR},")
    message(FATAL_ERROR "ERROR: In-source builds are not allowed.")
endif()

project(lzo C)

#
# configuration options
#

option(ENABLE_STATIC "Build static LZO library." ON)
option(ENABLE_SHARED "Build shared LZO library." OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()
if(NOT CMAKE_INSTALL_PREFIX)
    set(CMAKE_INSTALL_PREFIX "/usr/local" CACHE PATH "" FORCE)
endif()

#
# targets
#

file(GLOB lzo_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
list(SORT lzo_SOURCES)

if(NOT ENABLE_STATIC AND NOT ENABLE_SHARED)
    set(ENABLE_STATIC ON)
endif()
if(ENABLE_STATIC)
    add_library(lzo_static STATIC ${lzo_SOURCES})
    set_target_properties(lzo_static PROPERTIES OUTPUT_NAME lzo2)
endif()
if(ENABLE_SHARED)
    add_library(lzo_shared SHARED ${lzo_SOURCES})
    set_target_properties(lzo_shared PROPERTIES OUTPUT_NAME lzo2)
    # TODO: VERSION, SOVERSION
endif()

macro(lzo_add_executable t)
    add_executable(${t} ${ARGN})
    if(ENABLE_STATIC)
        target_link_libraries(${t} lzo_static)
    else()
        target_link_libraries(${t} lzo_shared)
    endif()
endmacro()
# main test driver
lzo_add_executable(lzotest  lzotest/lzotest.c)
# examples
lzo_add_executable(dict     examples/dict.c)
lzo_add_executable(lzopack  examples/lzopack.c)
lzo_add_executable(overlap  examples/overlap.c)
lzo_add_executable(precomp  examples/precomp.c)
lzo_add_executable(precomp2 examples/precomp2.c)
lzo_add_executable(simple   examples/simple.c)
if(0)
# some boring test programs
lzo_add_executable(align    tests/align.c)
lzo_add_executable(chksum   tests/chksum.c)
lzo_add_executable(promote  tests/promote.c)
lzo_add_executable(sizes    tests/sizes.c)
endif()

#
# compilation flags
#

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckLibraryExists)
include(CheckSymbolExists)
include(CheckTypeSize)
include(TestBigEndian)

# Checks for header files
macro(lzo_check_include_file f var)
    check_include_file("${f}" "${var}")
    if(NOT ",${${var}}," STREQUAL ",,")
        add_definitions(-D${var}=1)
    endif()
endmacro()
lzo_check_include_file(stdint.h             HAVE_STDINT_H)
lzo_check_include_file(sys/stat.h           HAVE_SYS_STAT_H)
lzo_check_include_file(sys/types.h          HAVE_SYS_TYPES_H)

# Checks for typedefs and structures
macro(lzo_check_type_size type var)
    check_type_size("${type}" "${var}")
    if("${${var}}" MATCHES "^[0-9]+$")
        add_definitions(-D${var}=${${var}})
    endif()
endmacro()
lzo_check_type_size("short"                 SIZEOF_SHORT)
lzo_check_type_size("int"                   SIZEOF_INT)
lzo_check_type_size("long"                  SIZEOF_LONG)
lzo_check_type_size("long long"             SIZEOF_LONG_LONG)
lzo_check_type_size("ptrdiff_t"             SIZEOF_PTRDIFF_T)
lzo_check_type_size("size_t"                SIZEOF_SIZE_T)
lzo_check_type_size("void *"                SIZEOF_VOID_P)
lzo_check_type_size("uintptr_t"             SIZEOF_UINTPTR_T)
lzo_check_type_size("__int16"               SIZEOF___INT16)
lzo_check_type_size("__int32"               SIZEOF___INT32)
lzo_check_type_size("__int64"               SIZEOF___INT64)
lzo_check_type_size("off_t"                 SIZEOF_OFF_T)
##lzo_check_type_size("off64_t"               SIZEOF_OFF64_T)

# Checks for library functions
macro(lzo_check_function_exists func var)
    check_function_exists("${func}" "${var}")
    if(NOT ",${${var}}," STREQUAL ",,")
        add_definitions(-D${var}=1)
    endif()
endmacro()
lzo_check_function_exists(clock_gettime     HAVE_CLOCK_GETTIME)
lzo_check_function_exists(stat              HAVE_STAT)
lzo_check_function_exists(strncasecmp       HAVE_STRNCASECMP)
lzo_check_function_exists(strnicmp          HAVE_STRNICMP)

set(big_endian)
TEST_BIG_ENDIAN(big_endian)
if ("${big_endian}" MATCHES "^1$")
    add_definitions(-DLZO_ABI_BIG_ENDIAN=1)
elseif ("${big_endian}" MATCHES "^0$")
    add_definitions(-DLZO_ABI_LITTLE_ENDIAN=1)
else()
    message(FATAL_ERROR "ERROR: TEST_BIG_ENDIAN failed.")
endif()


#
# "make install"
#

# subdirs relative to CMAKE_INSTALL_PREFIX
if(NOT DEFINED install_doc_subdir)
    set(install_doc_subdir doc)
endif()
if(NOT DEFINED install_include_subdir)
    set(install_include_subdir include)
endif()
if(NOT DEFINED install_lib_subdir)
    set(install_lib_subdir lib)
endif()
if(NOT DEFINED install_examples_subdir)
    set(install_examples_subdir libexec/lzo-examples)
endif()

set(doc_DATA AUTHORS COPYING NEWS THANKS doc/LZO.FAQ doc/LZO.TXT doc/LZOAPI.TXT)
set(pkginclude_HEADERS
    include/lzo/lzo1.h include/lzo/lzo1a.h include/lzo/lzo1b.h
    include/lzo/lzo1c.h include/lzo/lzo1f.h include/lzo/lzo1x.h
    include/lzo/lzo1y.h include/lzo/lzo1z.h include/lzo/lzo2a.h
    include/lzo/lzo_asm.h include/lzo/lzoconf.h include/lzo/lzodefs.h
    include/lzo/lzoutil.h
)

install(FILES ${doc_DATA} DESTINATION ${install_doc_subdir})
install(FILES ${pkginclude_HEADERS} DESTINATION ${install_include_subdir}/lzo)
if(ENABLE_STATIC)
    install(TARGETS lzo_static DESTINATION ${install_lib_subdir})
endif()
if(ENABLE_SHARED)
    install(TARGETS lzo_shared DESTINATION ${install_lib_subdir})
endif()
if(0)
    set(lzo_EXAMPLES lzopack lzotest simple)
    if(NOT ENABLE_STATIC)
        set(d "${CMAKE_INSTALL_PREFIX}/${install_lib_subdir}")
        set_target_properties(${lzo_EXAMPLES} PROPERTIES INSTALL_RPATH "${d}")
    endif()
    install(TARGETS ${lzo_EXAMPLES} DESTINATION ${install_examples_subdir})
endif()

# vim:set ft=cmake ts=4 sw=4 tw=0 et:
