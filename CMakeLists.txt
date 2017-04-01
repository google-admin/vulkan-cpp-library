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
cmake_minimum_required(VERSION 3.1)
set(CMAKE_CXX_STANDARD 11)

set(VULKAN_SDK_DIR "" CACHE PATH "Path to the Vulkan SDK")

set(VULKAN_CPP_LIBRARY_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(VULKAN_CPP_LIBRARY_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

#
# SPIRV-Headers
#

set(SPIRV-Headers_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/spirv-headers-src)

# Download and unpack SPIRV-Headers at configure time
configure_file(CMakeLists.txt.spirv-headers.in
               spirv-headers-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/spirv-headers-download)
execute_process(COMMAND ${CMAKE_COMMAND} --build .
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/spirv-headers-download)

# Add SPIRV-Headers directly to our build.
add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/spirv-headers-src
                 ${CMAKE_CURRENT_BINARY_DIR}/spirv-headers-build)

#
# SPIRV-Tools
#

set(SPIRV_TOOLS_SRC ${CMAKE_CURRENT_BINARY_DIR}/spirv-tools-src)

SET(SPIRV_SKIP_EXECUTABLES OFF CACHE BOOL "" FORCE)
SET(SPIRV_SKIP_TESTS OFF CACHE BOOL "" FORCE)

# Download and unpack SPIRV-Tools at configure time
configure_file(CMakeLists.txt.spirv-tools.in
               spirv-tools-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/spirv-tools-download)
execute_process(COMMAND ${CMAKE_COMMAND} --build .
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/spirv-tools-download)

# Add SPIRV-Tools directly to our build.
add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/spirv-tools-src
                 ${CMAKE_CURRENT_BINARY_DIR}/spirv-tools-build)

#
# glm
#

add_definitions(-DGLM_ENABLE_EXPERIMENTAL)

set(GLM_SRC_DIR ${CMAKE_CURRENT_BINARY_DIR}/glm-src)

# Download and unpack glm at configure time
configure_file(CMakeLists.txt.glm.in
               glm-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/glm-download)
execute_process(COMMAND ${CMAKE_COMMAND} --build .
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/glm-download)

#
# gli
#

set(GLI_SRC_DIR ${CMAKE_CURRENT_BINARY_DIR}/gli-src)

# Download and unpack gli at configure time
configure_file(CMakeLists.txt.gli.in
               gli-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/gli-download)
execute_process(COMMAND ${CMAKE_COMMAND} --build .
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/gli-download)

if(NOT DEFINED ANDROID_NDK)

#
# Googletest
#

# Download and unpack googletest at configure time
configure_file(CMakeLists.txt.googletest.in
               googletest-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download)
execute_process(COMMAND ${CMAKE_COMMAND} --build .
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download)

# Prevent GoogleTest from overriding our compiler/linker options
# when building with Visual Studio
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add googletest directly to our build. This adds
# the following targets: gtest, gtest_main, gmock
# and gmock_main
add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/googletest-src
                 ${CMAKE_CURRENT_BINARY_DIR}/googletest-build)

#
# Glslang
#

# Download and unpack glslang at configure time
configure_file(CMakeLists.txt.glslang.in
               glslang-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/glslang-download)
execute_process(COMMAND ${CMAKE_COMMAND} --build .
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/glslang-download)

# Add glslang directly to our build.
add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/glslang-src
                 ${CMAKE_CURRENT_BINARY_DIR}/glslang-build)


#
# OpenVR
#

set(OPENVR_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/openvr)

# Download and unpack OpenVR at configure time
configure_file(CMakeLists.txt.openvr.in
               openvr-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/openvr-download)
execute_process(COMMAND ${CMAKE_COMMAND} --build .
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/openvr-download)

#
# zlib
#

set(SKIP_INSTALL_ALL true)
set(ZLIB_SRC_DIR ${CMAKE_CURRENT_BINARY_DIR}/zlib-src)

# Download and unpack zlib at configure time
configure_file(CMakeLists.txt.zlib.in
               zlib-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/zlib-download)
execute_process(COMMAND ${CMAKE_COMMAND} --build .
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/zlib-download)

# Add zlib directly to our build.
add_subdirectory(${ZLIB_SRC_DIR}
                 ${CMAKE_CURRENT_BINARY_DIR}/zlib-build)

set(ZLIB_LIBRARY zlib)
set(ZLIB_INCLUDE_DIR ${ZLIB_SRC_DIR} ${CMAKE_CURRENT_BINARY_DIR}/zlib-build)

#
# png
#

set(PNG_SRC_DIR ${CMAKE_CURRENT_BINARY_DIR}/png-src)
set(PNG_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/png-build)
set(PNG_INCLUDE_DIR ${PNG_SRC_DIR} ${PNG_BUILD_DIR})

# Download and unpack png at configure time
configure_file(CMakeLists.txt.png.in
               png-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/png-download)
execute_process(COMMAND ${CMAKE_COMMAND} --build .
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/png-download)

# Add png directly to our build.
add_subdirectory(${PNG_SRC_DIR} ${PNG_BUILD_DIR})

endif()

enable_testing()

project (vulkan-cpp)

if(DEFINED ANDROID_NDK)
  add_definitions(-DVK_USE_PLATFORM_ANDROID_KHR)
elseif(UNIX)
  add_definitions(-DVK_USE_PLATFORM_XCB_KHR)
endif()

set(VULKAN_LIBRARY "vulkan")
if(WIN32)
  if(NOT EXISTS ${VULKAN_SDK_DIR}/Include/vulkan/vulkan.h)
    message(SEND_ERROR "On Windows VULKAN_SDK_DIR must be set.")
  endif()

  set(VULKAN_LIBRARY "vulkan-1")
endif()

add_subdirectory(spirv-reflection)
add_subdirectory(types)
add_subdirectory(vcc)
add_subdirectory(vcc-image)
add_subdirectory(sample/cube)
add_subdirectory(sample/heightmap)
add_subdirectory(sample/lighting)
add_subdirectory(sample/normal-mapping-and-cube-texture)
add_subdirectory(sample/teapot)

set(SAMPLES cube heightmap lighting normal-mapping-and-cube-texture teapot)

if(NOT DEFINED ANDROID_NDK)

add_subdirectory(spirv-reflection-test)
add_subdirectory(types-test)
add_subdirectory(vcc-test)

add_subdirectory(sample/openvr)
list(APPEND SAMPLES openvr)

set_property(TARGET ${SAMPLES} PROPERTY FOLDER "samples")

endif()
