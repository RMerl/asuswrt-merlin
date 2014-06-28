cmake_minimum_required(VERSION 2.6)

PROJECT(ubox-examples C)
ADD_DEFINITIONS(-O1 -Wall -Werror --std=gnu99 -g3)

IF(APPLE)
  INCLUDE_DIRECTORIES(/opt/local/include)
  LINK_DIRECTORIES(/opt/local/lib)
ENDIF()

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/..)
LINK_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/..)

ADD_EXECUTABLE(blobmsg-example blobmsg-example.c)
TARGET_LINK_LIBRARIES(blobmsg-example ubox blobmsg_json json)

ADD_EXECUTABLE(ustream-example ustream-example.c)
TARGET_LINK_LIBRARIES(ustream-example ubox)

ADD_EXECUTABLE(runqueue-example runqueue-example.c)
TARGET_LINK_LIBRARIES(runqueue-example ubox)

