#
# Copyright 2016 Google Inc. All Rights Reserved.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# http://www.apache.org/licenses/LICENSE-2.0
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

include_directories(include)
include_directories(../../types/include)
include_directories(../../vcc/include)
include_directories(${VULKAN_CPP_LIBRARY_BINARY_DIR}/vcc/include)
include_directories(../../vcc-image/include)
include_directories(${GLM_SRC_DIR})
include_directories(${OPENVR_SOURCE_DIR}/headers)
if(NOT VULKAN_SDK_DIR STREQUAL "")
  include_directories(${VULKAN_SDK_DIR}/include)
  link_directories(${VULKAN_SDK_DIR}/bin)
endif()

set(SAMPLE_OPENVR_INCLUDES
  "include/async_cache.h"
  "include/thread_pool.h"
  "include/vr.h"
)

set(SAMPLE_OPENVR_SRCS
  "src/openvr.cpp"
  "src/vr.cpp"
)

set(SAMPLE_OPENVR_SHADER_SRCS
  "src/openvr-frag.frag"
  "src/openvr-vert.vert"
)

if(UNIX)
  set(OPENVR_LIB_DIR ${OPENVR_SOURCE_DIR}/lib/linux64)
  set(OPENVR_LIBRARY_FILE ${OPENVR_SOURCE_DIR}/bin/linux64/libopenvr_api.so)
elseif(WIN32)
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(OPENVR_LIB_DIR ${OPENVR_SOURCE_DIR}/lib/win64)
    set(OPENVR_LIBRARY_FILE ${OPENVR_SOURCE_DIR}/bin/win64/openvr_api.dll)
  else()
    set(OPENVR_LIB_DIR ${OPENVR_SOURCE_DIR}/lib/win32)
    set(OPENVR_LIBRARY_FILE ${OPENVR_SOURCE_DIR}/bin/win32/openvr_api.dll)
  endif()
endif()

link_directories(${OPENVR_LIB_DIR})

add_executable(openvr ${SAMPLE_OPENVR_INCLUDES} ${SAMPLE_OPENVR_SRCS})
target_link_libraries(openvr vcc vcc-image openvr_api)

foreach(FILE ${SAMPLE_OPENVR_SHADER_SRCS})
  get_filename_component(FILEWE ${FILE} NAME_WE)
  set(FILE_OUTPUT ${FILEWE}.spv)
  add_custom_command(TARGET openvr POST_BUILD DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${FILE}
    COMMAND glslangValidator
    ARGS -V -o ${CMAKE_CURRENT_BINARY_DIR}/${FILE_OUTPUT} ${CMAKE_CURRENT_SOURCE_DIR}/${FILE}
    WORKING_DIRECTORY .)
endforeach()

add_custom_command(TARGET openvr POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  ${CMAKE_SOURCE_DIR}/textures $<TARGET_FILE_DIR:openvr>/textures)
add_custom_command(TARGET openvr POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy
  ${OPENVR_LIBRARY_FILE} $<TARGET_FILE_DIR:openvr>/)

