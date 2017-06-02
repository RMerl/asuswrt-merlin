#
# Setup
#

cmake_minimum_required(VERSION 2.8.8)
# Use LINK_INTERFACE_LIBRARIES instead of INTERFACE_LINK_LIBRARIES
if(POLICY CMP0022)
  cmake_policy(SET CMP0022 OLD)
endif()

project(libjpeg-turbo C)
set(VERSION 1.4.2)

if(CYGWIN OR NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
  execute_process(COMMAND "date" "+%Y%m%d" OUTPUT_VARIABLE BUILD)
  string(REGEX REPLACE "\n" "" BUILD ${BUILD})
elseif(WIN32)
  execute_process(COMMAND "wmic.exe" "os" "get" "LocalDateTime" OUTPUT_VARIABLE
    BUILD)
  string(REGEX REPLACE "[^0-9]" "" BUILD "${BUILD}")
  if (BUILD STREQUAL "")
    execute_process(COMMAND "cmd.exe" "/C" "DATE" "/T" OUTPUT_VARIABLE BUILD)
    string(REGEX REPLACE ".*[ ]([0-9]*)[/.]([0-9]*)[/.]([0-9]*).*" "\\3\\2\\1" BUILD "${BUILD}")
  else()
    string(SUBSTRING "${BUILD}" 0 8 BUILD)
  endif()
else()
  message(FATAL_ERROR "Platform not supported by this build system.  Use autotools instead.")
endif()

# This does nothing except when using MinGW.  CMAKE_BUILD_TYPE has no meaning
# in Visual Studio, and it always defaults to Debug when using NMake.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")

# This only works if building from the command line.  There is currently no way
# to set a variable's value based on the build type when using Visual Studio.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(BUILD "${BUILD}d")
endif()

message(STATUS "VERSION = ${VERSION}, BUILD = ${BUILD}")

option(WITH_SIMD "Include SIMD extensions" TRUE)
option(WITH_ARITH_ENC "Include arithmetic encoding support" TRUE)
option(WITH_ARITH_DEC "Include arithmetic decoding support" TRUE)
option(WITH_JPEG7 "Emulate libjpeg v7 API/ABI (this makes libjpeg-turbo backward incompatible with libjpeg v6b)" FALSE)
option(WITH_JPEG8 "Emulate libjpeg v8 API/ABI (this makes libjpeg-turbo backward incompatible with libjpeg v6b)" FALSE)
option(WITH_MEM_SRCDST "Include in-memory source/destination manager functions when emulating the libjpeg v6b or v7 API/ABI" TRUE)
option(WITH_TURBOJPEG "Include the TurboJPEG wrapper library and associated test programs" TRUE)
option(WITH_JAVA "Build Java wrapper for the TurboJPEG library" FALSE)
option(WITH_12BIT "Encode/decode JPEG images with 12-bit samples (implies WITH_SIMD=0 WITH_TURBOJPEG=0 WITH_ARITH_ENC=0 WITH_ARITH_DEC=0)" FALSE)
option(ENABLE_STATIC "Build static libraries" TRUE)
option(ENABLE_SHARED "Build shared libraries" TRUE)

if(WITH_12BIT)
  set(WITH_SIMD FALSE)
  set(WITH_TURBOJPEG FALSE)
  set(WITH_JAVA FALSE)
  set(WITH_ARITH_ENC FALSE)
  set(WITH_ARITH_DEC FALSE)
  set(BITS_IN_JSAMPLE 12)
  message(STATUS "12-bit JPEG support enabled")
else()
  set(BITS_IN_JSAMPLE 8)
endif()

if(WITH_ARITH_ENC)
  set(C_ARITH_CODING_SUPPORTED 1)
  message(STATUS "Arithmetic encoding support enabled")
else()
  message(STATUS "Arithmetic encoding support disabled")
endif()

if(WITH_ARITH_DEC)
  set(D_ARITH_CODING_SUPPORTED 1)
  message(STATUS "Arithmetic decoding support enabled")
else()
  message(STATUS "Arithmetic decoding support disabled")
endif()

if(WITH_TURBOJPEG)
  message(STATUS "TurboJPEG C wrapper enabled")
else()
  message(STATUS "TurboJPEG C wrapper disabled")
endif()

if(WITH_JAVA)
  message(STATUS "TurboJPEG Java wrapper enabled")
else()
  message(STATUS "TurboJPEG Java wrapper disabled")
endif()

set(SO_AGE 0)
if(WITH_MEM_SRCDST)
  set(SO_AGE 1)
endif()

set(JPEG_LIB_VERSION 62)
set(DLL_VERSION ${JPEG_LIB_VERSION})
set(FULLVERSION ${DLL_VERSION}.${SO_AGE}.0)
if(WITH_JPEG8)
  set(JPEG_LIB_VERSION 80)
  set(DLL_VERSION 8)
  set(FULLVERSION ${DLL_VERSION}.0.2)
  message(STATUS "Emulating libjpeg v8 API/ABI")
elseif(WITH_JPEG7)
  set(JPEG_LIB_VERSION 70)
  set(DLL_VERSION 7)
  set(FULLVERSION ${DLL_VERSION}.${SO_AGE}.0)
  message(STATUS "Emulating libjpeg v7 API/ABI")
endif(WITH_JPEG8)

if(WITH_MEM_SRCDST)
  set(MEM_SRCDST_SUPPORTED 1)
  message(STATUS "In-memory source/destination managers enabled")
else()
  message(STATUS "In-memory source/destination managers disabled")
endif()

if(MSVC)
  option(WITH_CRT_DLL
    "Link all libjpeg-turbo libraries and executables with the C run-time DLL (msvcr*.dll) instead of the static C run-time library (libcmt*.lib.)  The default is to use the C run-time DLL only with the libraries and executables that need it."
    FALSE)
  if(NOT WITH_CRT_DLL)
    # Use the static C library for all build types
    foreach(var CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
      CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)
      if(${var} MATCHES "/MD")
        string(REGEX REPLACE "/MD" "/MT" ${var} "${${var}}")
      endif()
    endforeach()
  endif()
  add_definitions(-W3 -wd4996)
endif()

# Detect whether compiler is 64-bit
if(MSVC AND CMAKE_CL_64)
  set(SIMD_X86_64 1)
  set(64BIT 1)
elseif(CMAKE_SIZEOF_VOID_P MATCHES 8)
  set(SIMD_X86_64 1)
  set(64BIT 1)
endif()

if(64BIT)
  message(STATUS "64-bit build")
else()
  message(STATUS "32-bit build")
endif()

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  if(MSVC)
    set(CMAKE_INSTALL_PREFIX_DEFAULT ${CMAKE_PROJECT_NAME})
  else()
    set(CMAKE_INSTALL_PREFIX_DEFAULT ${CMAKE_PROJECT_NAME}-gcc)
  endif()
  if(64BIT)
    set(CMAKE_INSTALL_PREFIX_DEFAULT ${CMAKE_INSTALL_PREFIX_DEFAULT}64)
  endif()
  set(CMAKE_INSTALL_PREFIX "c:/${CMAKE_INSTALL_PREFIX_DEFAULT}" CACHE PATH
    "Directory into which to install libjpeg-turbo (default: c:/${CMAKE_INSTALL_PREFIX_DEFAULT})"
    FORCE)
endif()

message(STATUS "Install directory = ${CMAKE_INSTALL_PREFIX}")

configure_file(win/jconfig.h.in jconfig.h)
configure_file(win/jconfigint.h.in jconfigint.h)

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_SOURCE_DIR})

if(WITH_JAVA)
  find_package(Java)
  find_package(JNI)
  if(DEFINED JAVACFLAGS)
    message(STATUS "Java compiler flags = ${JAVACFLAGS}")
  endif()
endif()


#
# Targets
#

set(JPEG_SOURCES jcapimin.c jcapistd.c jccoefct.c jccolor.c jcdctmgr.c jchuff.c
  jcinit.c jcmainct.c jcmarker.c jcmaster.c jcomapi.c jcparam.c jcphuff.c
  jcprepct.c jcsample.c jctrans.c jdapimin.c jdapistd.c jdatadst.c jdatasrc.c
  jdcoefct.c jdcolor.c jddctmgr.c jdhuff.c jdinput.c jdmainct.c jdmarker.c
  jdmaster.c jdmerge.c jdphuff.c jdpostct.c jdsample.c jdtrans.c jerror.c
  jfdctflt.c jfdctfst.c jfdctint.c jidctflt.c jidctfst.c jidctint.c jidctred.c
  jquant1.c jquant2.c jutils.c jmemmgr.c jmemnobs.c)

if(WITH_ARITH_ENC OR WITH_ARITH_DEC)
  set(JPEG_SOURCES ${JPEG_SOURCES} jaricom.c)
endif()

if(WITH_ARITH_ENC)
  set(JPEG_SOURCES ${JPEG_SOURCES} jcarith.c)
endif()

if(WITH_ARITH_DEC)
  set(JPEG_SOURCES ${JPEG_SOURCES} jdarith.c)
endif()

if(WITH_SIMD)
  add_definitions(-DWITH_SIMD)
  add_subdirectory(simd)
  if(SIMD_X86_64)
    set(JPEG_SOURCES ${JPEG_SOURCES} simd/jsimd_x86_64.c)
  else()
    set(JPEG_SOURCES ${JPEG_SOURCES} simd/jsimd_i386.c)
  endif()
  # This tells CMake that the "source" files haven't been generated yet
  set_source_files_properties(${SIMD_OBJS} PROPERTIES GENERATED 1)
else()
  set(JPEG_SOURCES ${JPEG_SOURCES} jsimd_none.c)
  message(STATUS "Not using SIMD acceleration")
endif()

if(WITH_JAVA)
  add_subdirectory(java)
  set(ENABLE_SHARED TRUE)
endif()

if(ENABLE_SHARED)
  add_subdirectory(sharedlib)
endif()

if(ENABLE_STATIC OR WITH_TURBOJPEG)
  add_library(jpeg-static STATIC ${JPEG_SOURCES} ${SIMD_OBJS})
  if(NOT MSVC)
    set_target_properties(jpeg-static PROPERTIES OUTPUT_NAME jpeg)
  endif()
  if(WITH_SIMD)
    add_dependencies(jpeg-static simd)
  endif()
endif()

if(WITH_TURBOJPEG)
  set(TURBOJPEG_SOURCES turbojpeg.c transupp.c jdatadst-tj.c jdatasrc-tj.c)
  if(WITH_JAVA)
    set(TURBOJPEG_SOURCES ${TURBOJPEG_SOURCES} turbojpeg-jni.c)
    include_directories(${JAVA_INCLUDE_PATH} ${JAVA_INCLUDE_PATH2})
  endif()

  if(ENABLE_SHARED)
    add_library(turbojpeg SHARED ${TURBOJPEG_SOURCES})
    set_target_properties(turbojpeg PROPERTIES DEFINE_SYMBOL DLLDEFINE)
    if(MINGW)
      set_target_properties(turbojpeg PROPERTIES LINK_FLAGS -Wl,--kill-at)
    endif()
    target_link_libraries(turbojpeg jpeg-static)
    set_target_properties(turbojpeg PROPERTIES LINK_INTERFACE_LIBRARIES "")

    add_executable(tjunittest tjunittest.c tjutil.c)
    target_link_libraries(tjunittest turbojpeg)

    add_executable(tjbench tjbench.c bmp.c tjutil.c rdbmp.c rdppm.c wrbmp.c
      wrppm.c)
    target_link_libraries(tjbench turbojpeg jpeg-static)
    set_property(TARGET tjbench PROPERTY COMPILE_FLAGS
      "-DBMP_SUPPORTED -DPPM_SUPPORTED")
  endif()

  if(ENABLE_STATIC)
    add_library(turbojpeg-static STATIC ${JPEG_SOURCES} ${SIMD_OBJS}
      turbojpeg.c transupp.c jdatadst-tj.c jdatasrc-tj.c)
    if(NOT MSVC)
      set_target_properties(turbojpeg-static PROPERTIES OUTPUT_NAME turbojpeg)
    endif()
    if(WITH_SIMD)
      add_dependencies(turbojpeg-static simd)
    endif()

    add_executable(tjunittest-static tjunittest.c tjutil.c)
    target_link_libraries(tjunittest-static turbojpeg-static)

    add_executable(tjbench-static tjbench.c bmp.c tjutil.c rdbmp.c rdppm.c
      wrbmp.c wrppm.c)
    target_link_libraries(tjbench-static turbojpeg-static jpeg-static)
    set_property(TARGET tjbench-static PROPERTY COMPILE_FLAGS
      "-DBMP_SUPPORTED -DPPM_SUPPORTED")
  endif()
endif()

if(WITH_12BIT)
  set(COMPILE_FLAGS "-DGIF_SUPPORTED -DPPM_SUPPORTED -DUSE_SETMODE")
else()
  set(COMPILE_FLAGS "-DBMP_SUPPORTED -DGIF_SUPPORTED -DPPM_SUPPORTED -DTARGA_SUPPORTED -DUSE_SETMODE")
  set(CJPEG_BMP_SOURCES rdbmp.c rdtarga.c)
  set(DJPEG_BMP_SOURCES wrbmp.c wrtarga.c)
endif()

if(ENABLE_STATIC)
  add_executable(cjpeg-static cjpeg.c cdjpeg.c rdgif.c rdppm.c rdswitch.c
    ${CJPEG_BMP_SOURCES})
  set_property(TARGET cjpeg-static PROPERTY COMPILE_FLAGS ${COMPILE_FLAGS})
  target_link_libraries(cjpeg-static jpeg-static)

  add_executable(djpeg-static djpeg.c cdjpeg.c rdcolmap.c rdswitch.c wrgif.c
    wrppm.c ${DJPEG_BMP_SOURCES})
  set_property(TARGET djpeg-static PROPERTY COMPILE_FLAGS ${COMPILE_FLAGS})
  target_link_libraries(djpeg-static jpeg-static)

  add_executable(jpegtran-static jpegtran.c cdjpeg.c rdswitch.c transupp.c)
  target_link_libraries(jpegtran-static jpeg-static)
  set_property(TARGET jpegtran-static PROPERTY COMPILE_FLAGS "-DUSE_SETMODE")
endif()

add_executable(rdjpgcom rdjpgcom.c)

add_executable(wrjpgcom wrjpgcom.c)


#
# Tests
#

if(MSVC_IDE)
  set(OBJDIR "\${CTEST_CONFIGURATION_TYPE}/")
else()
  set(OBJDIR "")
endif()

enable_testing()

if(WITH_12BIT)
  set(TESTORIG testorig12.jpg)
  set(MD5_JPEG_RGB_ISLOW 9620f424569594bb9242b48498ad801f)
  set(MD5_PPM_RGB_ISLOW f3301d2219783b8b3d942b7239fa50c0)
  set(MD5_JPEG_422_IFAST_OPT 7322e3bd2f127f7de4b40d4480ce60e4)
  set(MD5_PPM_422_IFAST 79807fa552899e66a04708f533e16950)
  set(MD5_PPM_422M_IFAST 07737bfe8a7c1c87aaa393a0098d16b0)
  set(MD5_JPEG_420_IFAST_Q100_PROG a1da220b5604081863a504297ed59e55)
  set(MD5_PPM_420_Q100_IFAST 1b3730122709f53d007255e8dfd3305e)
  set(MD5_PPM_420M_Q100_IFAST 980a1a3c5bf9510022869d30b7d26566)
  set(MD5_JPEG_GRAY_ISLOW 235c90707b16e2e069f37c888b2636d9)
  set(MD5_PPM_GRAY_ISLOW 7213c10af507ad467da5578ca5ee1fca)
  set(MD5_PPM_GRAY_ISLOW_RGB e96ee81c30a6ed422d466338bd3de65d)
  set(MD5_JPEG_420S_IFAST_OPT 7af8e60be4d9c227ec63ac9b6630855e)
  set(MD5_JPEG_3x2_FLOAT_PROG a8c17daf77b457725ec929e215b603f8)
  set(MD5_PPM_3x2_FLOAT 42876ab9e5c2f76a87d08db5fbd57956)
  set(MD5_PPM_420M_ISLOW_2_1 4ca6be2a6f326ff9eaab63e70a8259c0)
  set(MD5_PPM_420M_ISLOW_15_8 12aa9f9534c1b3d7ba047322226365eb)
  set(MD5_PPM_420M_ISLOW_13_8 f7e22817c7b25e1393e4ec101e9d4e96)
  set(MD5_PPM_420M_ISLOW_11_8 800a16f9f4dc9b293197bfe11be10a82)
  set(MD5_PPM_420M_ISLOW_9_8 06b7a92a9bc69f4dc36ec40f1937d55c)
  set(MD5_PPM_420M_ISLOW_7_8 3ec444a14a4ab4eab88ffc49c48eca43)
  set(MD5_PPM_420M_ISLOW_3_4 3e726b7ea872445b19437d1c1d4f0d93)
  set(MD5_PPM_420M_ISLOW_5_8 a8a771abdc94301d20ffac119b2caccd)
  set(MD5_PPM_420M_ISLOW_1_2 b419124dd5568b085787234866102866)
  set(MD5_PPM_420M_ISLOW_3_8 343d19015531b7bbe746124127244fa8)
  set(MD5_PPM_420M_ISLOW_1_4 35fd59d866e44659edfa3c18db2a3edb)
  set(MD5_PPM_420M_ISLOW_1_8 ccaed48ac0aedefda5d4abe4013f4ad7)
  set(MD5_JPEG_CROP cdb35ff4b4519392690ea040c56ea99c)
else()
  set(TESTORIG testorig.jpg)
  set(MD5_JPEG_RGB_ISLOW 768e970dd57b340ff1b83c9d3d47c77b)
  set(MD5_PPM_RGB_ISLOW 00a257f5393fef8821f2b88ac7421291)
  set(MD5_BMP_RGB_ISLOW_565 f07d2e75073e4bb10f6c6f4d36e2e3be)
  set(MD5_BMP_RGB_ISLOW_565D 4cfa0928ef3e6bb626d7728c924cfda4)
  set(MD5_JPEG_422_IFAST_OPT 2540287b79d913f91665e660303ab2c8)
  set(MD5_PPM_422_IFAST 35bd6b3f833bad23de82acea847129fa)
  set(MD5_PPM_422M_IFAST 8dbc65323d62cca7c91ba02dd1cfa81d)
  set(MD5_BMP_422M_IFAST_565 3294bd4d9a1f2b3d08ea6020d0db7065)
  set(MD5_BMP_422M_IFAST_565D da98c9c7b6039511be4a79a878a9abc1)
  set(MD5_JPEG_420_IFAST_Q100_PROG 990cbe0329c882420a2094da7e5adade)
  set(MD5_PPM_420_Q100_IFAST 5a732542015c278ff43635e473a8a294)
  set(MD5_PPM_420M_Q100_IFAST ff692ee9323a3b424894862557c092f1)
  set(MD5_JPEG_GRAY_ISLOW 72b51f894b8f4a10b3ee3066770aa38d)
  set(MD5_PPM_GRAY_ISLOW 8d3596c56eace32f205deccc229aa5ed)
  set(MD5_PPM_GRAY_ISLOW_RGB 116424ac07b79e5e801f00508eab48ec)
  set(MD5_BMP_GRAY_ISLOW_565 12f78118e56a2f48b966f792fedf23cc)
  set(MD5_BMP_GRAY_ISLOW_565D bdbbd616441a24354c98553df5dc82db)
  set(MD5_JPEG_420S_IFAST_OPT 388708217ac46273ca33086b22827ed8)
  if(WITH_SIMD)
    set(MD5_JPEG_3x2_FLOAT_PROG 343e3f8caf8af5986ebaf0bdc13b5c71)
    set(MD5_PPM_3x2_FLOAT 1a75f36e5904d6fc3a85a43da9ad89bb)
  else()
    set(MD5_JPEG_3x2_FLOAT_PROG 9bca803d2042bd1eb03819e2bf92b3e5)
    set(MD5_PPM_3x2_FLOAT f6bfab038438ed8f5522fbd33595dcdc)
  endif()
  set(MD5_JPEG_420_ISLOW_ARI e986fb0a637a8d833d96e8a6d6d84ea1)
  set(MD5_JPEG_444_ISLOW_PROGARI 0a8f1c8f66e113c3cf635df0a475a617)
  set(MD5_PPM_420M_IFAST_ARI 72b59a99bcf1de24c5b27d151bde2437)
  set(MD5_JPEG_420_ISLOW 9a68f56bc76e466aa7e52f415d0f4a5f)
  set(MD5_PPM_420M_ISLOW_2_1 9f9de8c0612f8d06869b960b05abf9c9)
  set(MD5_PPM_420M_ISLOW_15_8 b6875bc070720b899566cc06459b63b7)
  set(MD5_PPM_420M_ISLOW_13_8 bc3452573c8152f6ae552939ee19f82f)
  set(MD5_PPM_420M_ISLOW_11_8 d8cc73c0aaacd4556569b59437ba00a5)
  set(MD5_PPM_420M_ISLOW_9_8 d25e61bc7eac0002f5b393aa223747b6)
  set(MD5_PPM_420M_ISLOW_7_8 ddb564b7c74a09494016d6cd7502a946)
  set(MD5_PPM_420M_ISLOW_3_4 8ed8e68808c3fbc4ea764fc9d2968646)
  set(MD5_PPM_420M_ISLOW_5_8 a3363274999da2366a024efae6d16c9b)
  set(MD5_PPM_420M_ISLOW_1_2 e692a315cea26b988c8e8b29a5dbcd81)
  set(MD5_PPM_420M_ISLOW_3_8 79eca9175652ced755155c90e785a996)
  set(MD5_PPM_420M_ISLOW_1_4 79cd778f8bf1a117690052cacdd54eca)
  set(MD5_PPM_420M_ISLOW_1_8 391b3d4aca640c8567d6f8745eb2142f)
  set(MD5_BMP_420_ISLOW_256 4980185e3776e89bd931736e1cddeee6)
  set(MD5_BMP_420_ISLOW_565 bf9d13e16c4923b92e1faa604d7922cb)
  set(MD5_BMP_420_ISLOW_565D 6bde71526acc44bcff76f696df8638d2)
  set(MD5_BMP_420M_ISLOW_565 8dc0185245353cfa32ad97027342216f)
  set(MD5_BMP_420M_ISLOW_565D d1be3a3339166255e76fa50a0d70d73e)
  set(MD5_JPEG_CROP b4197f377e621c4e9b1d20471432610d)
endif()

if(WITH_JAVA)
  add_test(TJUnitTest
    ${JAVA_RUNTIME} -cp java/${OBJDIR}turbojpeg.jar
      -Djava.library.path=${CMAKE_CURRENT_BINARY_DIR}/${OBJDIR}
      TJUnitTest)
  add_test(TJUnitTest-yuv
    ${JAVA_RUNTIME} -cp java/${OBJDIR}turbojpeg.jar
      -Djava.library.path=${CMAKE_CURRENT_BINARY_DIR}/${OBJDIR}
      TJUnitTest -yuv)
  add_test(TJUnitTest-yuv-nopad
    ${JAVA_RUNTIME} -cp java/${OBJDIR}turbojpeg.jar
      -Djava.library.path=${CMAKE_CURRENT_BINARY_DIR}/${OBJDIR}
      TJUnitTest -yuv -noyuvpad)
  add_test(TJUnitTest-bi
    ${JAVA_RUNTIME} -cp java/${OBJDIR}turbojpeg.jar
      -Djava.library.path=${CMAKE_CURRENT_BINARY_DIR}/${OBJDIR}
      TJUnitTest -bi)
  add_test(TJUnitTest-bi-yuv
    ${JAVA_RUNTIME} -cp java/${OBJDIR}turbojpeg.jar
      -Djava.library.path=${CMAKE_CURRENT_BINARY_DIR}/${OBJDIR}
      TJUnitTest -bi -yuv)
  add_test(TJUnitTest-bi-yuv-nopad
    ${JAVA_RUNTIME} -cp java/${OBJDIR}turbojpeg.jar
      -Djava.library.path=${CMAKE_CURRENT_BINARY_DIR}/${OBJDIR}
      TJUnitTest -bi -yuv -noyuvpad)
endif()

set(TEST_LIBTYPES "")
if(ENABLE_SHARED)
  set(TEST_LIBTYPES ${TEST_LIBTYPES} shared)
endif()
if(ENABLE_STATIC)
  set(TEST_LIBTYPES ${TEST_LIBTYPES} static)
endif()

foreach(libtype ${TEST_LIBTYPES})
  if(libtype STREQUAL "shared")
    set(dir sharedlib/)
  else()
    set(dir "")
    set(suffix -static)
  endif()
  if(WITH_TURBOJPEG)
    add_test(tjunittest${suffix} tjunittest${suffix})
    add_test(tjunittest${suffix}-alloc tjunittest${suffix} -alloc)
    add_test(tjunittest${suffix}-yuv tjunittest${suffix} -yuv)
    add_test(tjunittest${suffix}-yuv-alloc tjunittest${suffix} -yuv -alloc)
    add_test(tjunittest${suffix}-yuv-nopad tjunittest${suffix} -yuv -noyuvpad)
  endif()

  # These tests are carefully chosen to provide full coverage of as many of the
  # underlying algorithms as possible (including all of the SIMD-accelerated
  # ones.)

  # CC: null  SAMP: fullsize  FDCT: islow  ENT: huff
  add_test(cjpeg${suffix}-rgb-islow
    ${dir}cjpeg${suffix} -rgb -dct int -outfile testout_rgb_islow.jpg
      ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
  add_test(cjpeg${suffix}-rgb-islow-cmp
    ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_RGB_ISLOW} -DFILE=testout_rgb_islow.jpg
      -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
  # CC: null  SAMP: fullsize  IDCT: islow  ENT: huff
  add_test(djpeg${suffix}-rgb-islow
    ${dir}djpeg${suffix} -dct int -ppm -outfile testout_rgb_islow.ppm
      testout_rgb_islow.jpg)
  add_test(djpeg${suffix}-rgb-islow-cmp
    ${CMAKE_COMMAND} -DMD5=${MD5_PPM_RGB_ISLOW} -DFILE=testout_rgb_islow.ppm
      -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
  if(NOT WITH_12BIT)
    # CC: RGB->RGB565  SAMP: fullsize  IDCT: islow  ENT: huff
    add_test(djpeg${suffix}-rgb-islow-565
      ${dir}djpeg${suffix} -dct int -rgb565 -dither none -bmp
        -outfile testout_rgb_islow_565.bmp testout_rgb_islow.jpg)
    add_test(djpeg${suffix}-rgb-islow-565-cmp
      ${CMAKE_COMMAND} -DMD5=${MD5_BMP_RGB_ISLOW_565}
        -DFILE=testout_rgb_islow_565.bmp
        -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
    # CC: RGB->RGB565 (dithered)  SAMP: fullsize  IDCT: islow  ENT: huff
    add_test(djpeg${suffix}-rgb-islow-565D
      ${dir}djpeg${suffix} -dct int -rgb565 -bmp
        -outfile testout_rgb_islow_565D.bmp testout_rgb_islow.jpg)
    add_test(djpeg${suffix}-rgb-islow-565D-cmp
      ${CMAKE_COMMAND} -DMD5=${MD5_BMP_RGB_ISLOW_565D}
        -DFILE=testout_rgb_islow_565D.bmp
        -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
  endif()

  # CC: RGB->YCC  SAMP: fullsize/h2v1  FDCT: ifast  ENT: 2-pass huff
  add_test(cjpeg${suffix}-422-ifast-opt
    ${dir}cjpeg${suffix} -sample 2x1 -dct fast -opt
      -outfile testout_422_ifast_opt.jpg
      ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
  add_test(cjpeg${suffix}-422-ifast-opt-cmp
    ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_422_IFAST_OPT}
      -DFILE=testout_422_ifast_opt.jpg
      -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
  # CC: YCC->RGB  SAMP: fullsize/h2v1 fancy  IDCT: ifast  ENT: huff
  add_test(djpeg${suffix}-422-ifast
    ${dir}djpeg${suffix} -dct fast -outfile testout_422_ifast.ppm
      testout_422_ifast_opt.jpg)
  add_test(djpeg${suffix}-422-ifast-cmp
    ${CMAKE_COMMAND} -DMD5=${MD5_PPM_422_IFAST} -DFILE=testout_422_ifast.ppm
      -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
  # CC: YCC->RGB  SAMP: h2v1 merged  IDCT: ifast  ENT: huff
  add_test(djpeg${suffix}-422m-ifast
    ${dir}djpeg${suffix} -dct fast -nosmooth -outfile testout_422m_ifast.ppm
      testout_422_ifast_opt.jpg)
  add_test(djpeg${suffix}-422m-ifast-cmp
    ${CMAKE_COMMAND} -DMD5=${MD5_PPM_422M_IFAST} -DFILE=testout_422m_ifast.ppm
      -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
  if(NOT WITH_12BIT)
    # CC: YCC->RGB565  SAMP: h2v1 merged  IDCT: ifast  ENT: huff
    add_test(djpeg${suffix}-422m-ifast-565
      ${dir}djpeg${suffix} -dct int -nosmooth -rgb565 -dither none -bmp
        -outfile testout_422m_ifast_565.bmp testout_422_ifast_opt.jpg)
    add_test(djpeg${suffix}-422m-ifast-565-cmp
      ${CMAKE_COMMAND} -DMD5=${MD5_BMP_422M_IFAST_565}
        -DFILE=testout_422m_ifast_565.bmp
        -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
    # CC: YCC->RGB565 (dithered)  SAMP: h2v1 merged  IDCT: ifast  ENT: huff
    add_test(djpeg${suffix}-422m-ifast-565D
      ${dir}djpeg${suffix} -dct int -nosmooth -rgb565 -bmp
        -outfile testout_422m_ifast_565D.bmp testout_422_ifast_opt.jpg)
    add_test(djpeg${suffix}-422m-ifast-565D-cmp
      ${CMAKE_COMMAND} -DMD5=${MD5_BMP_422M_IFAST_565D}
        -DFILE=testout_422m_ifast_565D.bmp
        -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
  endif()

  # CC: RGB->YCC  SAMP: fullsize/h2v2  FDCT: ifast  ENT: prog huff
  add_test(cjpeg${suffix}-420-q100-ifast-prog
    ${dir}cjpeg${suffix} -sample 2x2 -quality 100 -dct fast -prog
      -outfile testout_420_q100_ifast_prog.jpg
      ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
  add_test(cjpeg${suffix}-420-q100-ifast-prog-cmp
    ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_420_IFAST_Q100_PROG}
      -DFILE=testout_420_q100_ifast_prog.jpg
      -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
  # CC: YCC->RGB  SAMP: fullsize/h2v2 fancy  IDCT: ifast  ENT: prog huff
  add_test(djpeg${suffix}-420-q100-ifast-prog
    ${dir}djpeg${suffix} -dct fast -outfile testout_420_q100_ifast.ppm
      testout_420_q100_ifast_prog.jpg)
  add_test(djpeg${suffix}-420-q100-ifast-prog-cmp
    ${CMAKE_COMMAND} -DMD5=${MD5_PPM_420_Q100_IFAST}
      -DFILE=testout_420_q100_ifast.ppm
      -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
  # CC: YCC->RGB  SAMP: h2v2 merged  IDCT: ifast  ENT: prog huff
  add_test(djpeg${suffix}-420m-q100-ifast-prog
    ${dir}djpeg${suffix} -dct fast -nosmooth
      -outfile testout_420m_q100_ifast.ppm testout_420_q100_ifast_prog.jpg)
  add_test(djpeg${suffix}-420m-q100-ifast-prog-cmp
    ${CMAKE_COMMAND} -DMD5=${MD5_PPM_420M_Q100_IFAST}
      -DFILE=testout_420m_q100_ifast.ppm
      -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)

  # CC: RGB->Gray  SAMP: fullsize  FDCT: islow  ENT: huff
  add_test(cjpeg${suffix}-gray-islow
    ${dir}cjpeg${suffix} -gray -dct int -outfile testout_gray_islow.jpg
      ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
  add_test(cjpeg${suffix}-gray-islow-cmp
    ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_GRAY_ISLOW}
      -DFILE=testout_gray_islow.jpg
      -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
  # CC: Gray->Gray  SAMP: fullsize  IDCT: islow  ENT: huff
  add_test(djpeg${suffix}-gray-islow
    ${dir}djpeg${suffix} -dct int -outfile testout_gray_islow.ppm
      testout_gray_islow.jpg)
  add_test(djpeg${suffix}-gray-islow-cmp
    ${CMAKE_COMMAND} -DMD5=${MD5_PPM_GRAY_ISLOW}
      -DFILE=testout_gray_islow.ppm
      -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
  # CC: Gray->RGB  SAMP: fullsize  IDCT: islow  ENT: huff
  add_test(djpeg${suffix}-gray-islow-rgb
    ${dir}djpeg${suffix} -dct int -rgb -outfile testout_gray_islow_rgb.ppm
      testout_gray_islow.jpg)
  add_test(djpeg${suffix}-gray-islow-rgb-cmp
    ${CMAKE_COMMAND} -DMD5=${MD5_PPM_GRAY_ISLOW_RGB}
      -DFILE=testout_gray_islow_rgb.ppm
      -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
  if(NOT WITH_12BIT)
    # CC: Gray->RGB565  SAMP: fullsize  IDCT: islow  ENT: huff
    add_test(djpeg${suffix}-gray-islow-565
      ${dir}djpeg${suffix} -dct int -rgb565 -dither none -bmp
        -outfile testout_gray_islow_565.bmp testout_gray_islow.jpg)
    add_test(djpeg${suffix}-gray-islow-565-cmp
      ${CMAKE_COMMAND} -DMD5=${MD5_BMP_GRAY_ISLOW_565}
        -DFILE=testout_gray_islow_565.bmp
        -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
    # CC: Gray->RGB565 (dithered)  SAMP: fullsize  IDCT: islow  ENT: huff
    add_test(djpeg${suffix}-gray-islow-565D
      ${dir}djpeg${suffix} -dct int -rgb565 -bmp
        -outfile testout_gray_islow_565D.bmp testout_gray_islow.jpg)
    add_test(djpeg${suffix}-gray-islow-565D-cmp
      ${CMAKE_COMMAND} -DMD5=${MD5_BMP_GRAY_ISLOW_565D}
        -DFILE=testout_gray_islow_565D.bmp
        -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
  endif()

  # CC: RGB->YCC  SAMP: fullsize smooth/h2v2 smooth  FDCT: islow
  # ENT: 2-pass huff
  add_test(cjpeg${suffix}-420s-ifast-opt
    ${dir}cjpeg${suffix} -sample 2x2 -smooth 1 -dct int -opt -outfile
      testout_420s_ifast_opt.jpg ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
  add_test(cjpeg${suffix}-420s-ifast-opt-cmp
    ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_420S_IFAST_OPT}
      -DFILE=testout_420s_ifast_opt.jpg
      -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)

  # CC: RGB->YCC  SAMP: fullsize/int  FDCT: float  ENT: prog huff
  add_test(cjpeg${suffix}-3x2-float-prog
    ${dir}cjpeg${suffix} -sample 3x2 -dct float -prog
      -outfile testout_3x2_float_prog.jpg
      ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
  add_test(cjpeg${suffix}-3x2-float-prog-cmp
    ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_3x2_FLOAT_PROG}
      -DFILE=testout_3x2_float_prog.jpg
      -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
  # CC: YCC->RGB  SAMP: fullsize/int  IDCT: float  ENT: prog huff
  add_test(djpeg${suffix}-3x2-float-prog
    ${dir}djpeg${suffix} -dct float -outfile testout_3x2_float.ppm
      testout_3x2_float_prog.jpg)
  add_test(djpeg${suffix}-3x2-float-prog-cmp
    ${CMAKE_COMMAND} -DMD5=${MD5_PPM_3x2_FLOAT} -DFILE=testout_3x2_float.ppm
      -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)

  if(WITH_ARITH_ENC)
    # CC: YCC->RGB  SAMP: fullsize/h2v2  FDCT: islow  ENT: arith
    add_test(cjpeg${suffix}-420-islow-ari
      ${dir}cjpeg${suffix} -dct int -arithmetic
        -outfile testout_420_islow_ari.jpg
        ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
    add_test(cjpeg${suffix}-420-islow-ari-cmp
      ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_420_ISLOW_ARI}
        -DFILE=testout_420_islow_ari.jpg
        -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
    add_test(jpegtran${suffix}-420-islow-ari
      ${dir}jpegtran${suffix} -arithmetic
        -outfile testout_420_islow_ari.jpg
        ${CMAKE_SOURCE_DIR}/testimages/testimgint.jpg)
    add_test(jpegtran${suffix}-420-islow-ari-cmp
      ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_420_ISLOW_ARI}
        -DFILE=testout_420_islow_ari.jpg
        -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
    # CC: YCC->RGB  SAMP: fullsize  FDCT: islow  ENT: prog arith
    add_test(cjpeg${suffix}-444-islow-progari
      ${dir}cjpeg${suffix} -sample 1x1 -dct int -progressive -arithmetic
        -outfile testout_444_islow_progari.jpg
        ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
    add_test(cjpeg${suffix}-444-islow-progari-cmp
      ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_444_ISLOW_PROGARI}
        -DFILE=testout_444_islow_progari.jpg
        -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
  endif()
  if(WITH_ARITH_DEC)
    # CC: RGB->YCC  SAMP: h2v2 merged  IDCT: ifast  ENT: arith
    add_test(djpeg${suffix}-420m-ifast-ari
      ${dir}djpeg${suffix} -fast -ppm -outfile testout_420m_ifast_ari.ppm
        ${CMAKE_SOURCE_DIR}/testimages/testimgari.jpg)
    add_test(djpeg${suffix}-420m-ifast-ari-cmp
      ${CMAKE_COMMAND} -DMD5=${MD5_PPM_420M_IFAST_ARI}
        -DFILE=testout_420m_ifast_ari.ppm
        -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
    add_test(jpegtran${suffix}-420-islow
      ${dir}jpegtran${suffix} -outfile testout_420_islow.jpg
        ${CMAKE_SOURCE_DIR}/testimages/testimgari.jpg)
    add_test(jpegtran${suffix}-420-islow-cmp
      ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_420_ISLOW}
        -DFILE=testout_420_islow.jpg
        -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
  endif()

  # 2/1--   CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 16x16 islow  ENT: huff
  # 15/8--  CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 15x15 islow  ENT: huff
  # 13/8--  CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 13x13 islow  ENT: huff
  # 11/8--  CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 11x11 islow  ENT: huff
  # 9/8--   CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 9x9 islow  ENT: huff
  # 7/8--   CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 7x7 islow/14x14 islow
  #         ENT: huff
  # 3/4--   CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 6x6 islow/12x12 islow
  #         ENT: huff
  # 5/8--   CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 5x5 islow/10x10 islow
  #         ENT: huff
  # 1/2--   CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 4x4 islow/8x8 islow
  #         ENT: huff
  # 3/8--   CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 3x3 islow/6x6 islow
  #         ENT: huff
  # 1/4--   CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 2x2 islow/4x4 islow
  #         ENT: huff
  # 1/8--   CC: YCC->RGB  SAMP: h2v2 merged  IDCT: 1x1 islow/2x2 islow
  #         ENT: huff
  foreach(scale 2_1 15_8 13_8 11_8 9_8 7_8 3_4 5_8 1_2 3_8 1_4 1_8)
    string(REGEX REPLACE "_" "/" scalearg ${scale})
    add_test(djpeg${suffix}-420m-islow-${scale}
      ${dir}djpeg${suffix} -dct int -scale ${scalearg} -nosmooth -ppm
        -outfile testout_420m_islow_${scale}.ppm
        ${CMAKE_SOURCE_DIR}/testimages/${TESTORIG})
    add_test(djpeg${suffix}-420m-islow-${scale}-cmp
      ${CMAKE_COMMAND} -DMD5=${MD5_PPM_420M_ISLOW_${scale}}
        -DFILE=testout_420m_islow_${scale}.ppm
        -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
  endforeach()

  if(NOT WITH_12BIT)
    # CC: YCC->RGB (dithered)  SAMP: h2v2 fancy  IDCT: islow  ENT: huff
    add_test(djpeg${suffix}-420-islow-256
      ${dir}djpeg${suffix} -dct int -colors 256 -bmp
        -outfile testout_420_islow_256.bmp
        ${CMAKE_SOURCE_DIR}/testimages/${TESTORIG})
    add_test(djpeg${suffix}-420-islow-256-cmp
      ${CMAKE_COMMAND} -DMD5=${MD5_BMP_420_ISLOW_256}
        -DFILE=testout_420_islow_256.bmp
        -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
    # CC: YCC->RGB565  SAMP: h2v2 fancy  IDCT: islow  ENT: huff
    add_test(djpeg${suffix}-420-islow-565
      ${dir}djpeg${suffix} -dct int -rgb565 -dither none -bmp
        -outfile testout_420_islow_565.bmp
        ${CMAKE_SOURCE_DIR}/testimages/${TESTORIG})
    add_test(djpeg${suffix}-420-islow-565-cmp
      ${CMAKE_COMMAND} -DMD5=${MD5_BMP_420_ISLOW_565}
        -DFILE=testout_420_islow_565.bmp
        -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
    # CC: YCC->RGB565 (dithered)  SAMP: h2v2 fancy  IDCT: islow  ENT: huff
    add_test(djpeg${suffix}-420-islow-565D
      ${dir}djpeg${suffix} -dct int -rgb565 -bmp
        -outfile testout_420_islow_565D.bmp
        ${CMAKE_SOURCE_DIR}/testimages/${TESTORIG})
    add_test(djpeg${suffix}-420-islow-565D-cmp
      ${CMAKE_COMMAND} -DMD5=${MD5_BMP_420_ISLOW_565D}
        -DFILE=testout_420_islow_565D.bmp
        -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
    # CC: YCC->RGB565  SAMP: h2v2 merged  IDCT: islow  ENT: huff
    add_test(djpeg${suffix}-420m-islow-565
      ${dir}djpeg${suffix} -dct int -nosmooth -rgb565 -dither none -bmp
        -outfile testout_420m_islow_565.bmp
        ${CMAKE_SOURCE_DIR}/testimages/${TESTORIG})
    add_test(djpeg${suffix}-420m-islow-565-cmp
      ${CMAKE_COMMAND} -DMD5=${MD5_BMP_420M_ISLOW_565}
        -DFILE=testout_420m_islow_565.bmp
        -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
    # CC: YCC->RGB565 (dithered)  SAMP: h2v2 merged  IDCT: islow  ENT: huff
    add_test(djpeg${suffix}-420m-islow-565D
      ${dir}djpeg${suffix} -dct int -nosmooth -rgb565 -bmp
        -outfile testout_420m_islow_565D.bmp
        ${CMAKE_SOURCE_DIR}/testimages/${TESTORIG})
    add_test(djpeg${suffix}-420m-islow-565D-cmp
      ${CMAKE_COMMAND} -DMD5=${MD5_BMP_420M_ISLOW_565D}
        -DFILE=testout_420m_islow_565D.bmp
        -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
  endif()
  add_test(jpegtran${suffix}-crop
    ${dir}jpegtran${suffix} -crop 120x90+20+50 -transpose -perfect
      -outfile testout_crop.jpg ${CMAKE_SOURCE_DIR}/testimages/${TESTORIG})
  add_test(jpegtran${suffix}-crop-cmp
    ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_CROP} -DFILE=testout_crop.jpg
      -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)

endforeach()

add_custom_target(testclean COMMAND ${CMAKE_COMMAND} -P
  ${CMAKE_SOURCE_DIR}/cmakescripts/testclean.cmake)


#
# Installer
#

if(MSVC)
  set(INST_PLATFORM "Visual C++")
  set(INST_NAME ${CMAKE_PROJECT_NAME}-${VERSION}-vc)
  set(INST_REG_NAME ${CMAKE_PROJECT_NAME})
elseif(MINGW)
  set(INST_PLATFORM GCC)
  set(INST_NAME ${CMAKE_PROJECT_NAME}-${VERSION}-gcc)
  set(INST_REG_NAME ${CMAKE_PROJECT_NAME}-gcc)
  set(INST_DEFS -DGCC)
endif()

if(64BIT)
  set(INST_PLATFORM "${INST_PLATFORM} 64-bit")
  set(INST_NAME ${INST_NAME}64)
  set(INST_REG_NAME ${INST_DIR}64)
  set(INST_DEFS ${INST_DEFS} -DWIN64)
endif()

if(WITH_JAVA)
  set(INST_DEFS ${INST_DEFS} -DJAVA)
endif()

if(MSVC_IDE)
  set(INST_DEFS ${INST_DEFS} "-DBUILDDIR=${CMAKE_CFG_INTDIR}\\")
else()
  set(INST_DEFS ${INST_DEFS} "-DBUILDDIR=")
endif()

STRING(REGEX REPLACE "/" "\\\\" INST_DIR ${CMAKE_INSTALL_PREFIX})

configure_file(release/libjpeg-turbo.nsi.in libjpeg-turbo.nsi @ONLY)

if(WITH_JAVA)
  set(JAVA_DEPEND java)
endif()
add_custom_target(installer
  makensis -nocd ${INST_DEFS} libjpeg-turbo.nsi
  DEPENDS jpeg jpeg-static turbojpeg turbojpeg-static rdjpgcom wrjpgcom
    cjpeg djpeg jpegtran tjbench ${JAVA_DEPEND}
  SOURCES libjpeg-turbo.nsi)

if(WITH_TURBOJPEG)
  if(ENABLE_SHARED)
    install(TARGETS turbojpeg tjbench
      ARCHIVE DESTINATION lib
      LIBRARY DESTINATION lib
      RUNTIME DESTINATION bin)
  endif()
  if(ENABLE_STATIC)
    install(TARGETS turbojpeg-static ARCHIVE DESTINATION lib)
    if(NOT ENABLE_SHARED)
      install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/tjbench-static.exe
        DESTINATION bin RENAME tjbench.exe)
    endif()
  endif()
  install(FILES ${CMAKE_SOURCE_DIR}/turbojpeg.h DESTINATION include)
endif()

if(ENABLE_STATIC)
  install(TARGETS jpeg-static ARCHIVE DESTINATION lib)
  if(NOT ENABLE_SHARED)
    install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/cjpeg-static.exe
      DESTINATION bin RENAME cjpeg.exe)
    install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/djpeg-static.exe
      DESTINATION bin RENAME djpeg.exe)
    install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/jpegtran-static.exe
      DESTINATION bin RENAME jpegtran.exe)
  endif()
endif()

install(TARGETS rdjpgcom wrjpgcom RUNTIME DESTINATION bin)

install(FILES ${CMAKE_SOURCE_DIR}/README ${CMAKE_SOURCE_DIR}/README-turbo.txt
  ${CMAKE_SOURCE_DIR}/example.c ${CMAKE_SOURCE_DIR}/libjpeg.txt
  ${CMAKE_SOURCE_DIR}/structure.txt ${CMAKE_SOURCE_DIR}/usage.txt
  ${CMAKE_SOURCE_DIR}/wizard.txt
  DESTINATION doc)

install(FILES ${CMAKE_BINARY_DIR}/jconfig.h ${CMAKE_SOURCE_DIR}/jerror.h
  ${CMAKE_SOURCE_DIR}/jmorecfg.h ${CMAKE_SOURCE_DIR}/jpeglib.h
  DESTINATION include)
