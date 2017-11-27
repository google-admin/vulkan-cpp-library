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

include_directories(../../types/include)
include_directories(../../vcc/include)
include_directories(${CMAKE_BINARY_DIR}/vcc/include)
include_directories(../../vcc-image/include)
include_directories(${GLM_SRC_DIR})
if(NOT VULKAN_SDK_DIR STREQUAL "")
  include_directories(${VULKAN_SDK_DIR}/include)
  link_directories(${VULKAN_SDK_DIR}/lib)
endif()

set(SAMPLE_NORMAL_MAPPING_AND_CUBE_TEXTURE_SRCS
  "src/normal-mapping-and-cube-texture.cpp"
)

set(SAMPLE_NORMAL_MAPPING_AND_CUBE_TEXTURE_SHADER_SRCS
  "src/normal-mapping-and-cube-texture-frag.frag"
  "src/normal-mapping-and-cube-texture-vert.vert"
)

if(DEFINED ANDROID_NDK)
  include_directories(${ANDROID_NDK}/sources/android/native_app_glue/)

  add_library(normal-mapping-and-cube-texture SHARED ${SAMPLE_NORMAL_MAPPING_AND_CUBE_TEXTURE_SRCS}
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)

  find_library(log-lib log)
  find_library(android-lib android)
  find_library(vulkan-lib vulkan)
  find_library(jnigraphics-lib jnigraphics)
  target_link_libraries(normal-mapping-and-cube-texture ${log-lib} ${android-lib} ${vulkan-lib} ${jnigraphics-lib})
else()
  add_executable(normal-mapping-and-cube-texture ${SAMPLE_NORMAL_MAPPING_AND_CUBE_TEXTURE_SRCS})
endif()

target_link_libraries(normal-mapping-and-cube-texture vcc vcc-image)

if(NOT DEFINED ANDROID_NDK)
  foreach(FILE ${SAMPLE_NORMAL_MAPPING_AND_CUBE_TEXTURE_SHADER_SRCS})
    get_filename_component(FILEWE ${FILE} NAME_WE)
    set(FILE_OUTPUT ${FILEWE}.spv)
    add_custom_command(TARGET normal-mapping-and-cube-texture POST_BUILD
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${FILE}
      COMMAND glslangValidator
      ARGS -V -o ${CMAKE_CURRENT_BINARY_DIR}/${FILE_OUTPUT} ${CMAKE_CURRENT_SOURCE_DIR}/${FILE}
      WORKING_DIRECTORY .)
  endforeach()

  add_custom_command(TARGET normal-mapping-and-cube-texture POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/textures ${CMAKE_CURRENT_BINARY_DIR}/textures)
  add_custom_command(TARGET normal-mapping-and-cube-texture POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:png> $<TARGET_FILE_DIR:normal-mapping-and-cube-texture>/)
  add_custom_command(TARGET normal-mapping-and-cube-texture POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:zlib> $<TARGET_FILE_DIR:normal-mapping-and-cube-texture>/)
endif()
