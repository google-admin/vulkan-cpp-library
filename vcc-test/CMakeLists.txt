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

include_directories(../vcc/include)
include_directories(${VULKAN_CPP_LIBRARY_BINARY_DIR}/vcc/include)
include_directories(../types/include)
include_directories(${gtest_SOURCE_DIR}/include)
include_directories(${GLM_SRC_DIR})
if(NOT VULKAN_SDK_DIR STREQUAL "")
  include_directories(${VULKAN_SDK_DIR}/include)
  link_directories(${VULKAN_SDK_DIR}/bin)
endif()

set(VCC_TEST_SRCS
  "src/compute_shader_integration_test.cpp"
)

set(VCC_TEST_SHADER_SRCS
  "src/integration-test-1.comp"
)

add_executable(vcc-test ${VCC_TEST_SRCS})

target_link_libraries(vcc-test vcc types ${VULKAN_LIBRARY} gtest gtest_main)

set(VCC_TEST_COMPILED_SHADER_BINARIES)
foreach(FILE ${VCC_TEST_SHADER_SRCS})
  get_filename_component(FILEWE ${FILE} NAME_WE)
  set(FILE_OUTPUT ${FILEWE}.spv)
  add_custom_command(TARGET vcc-test POST_BUILD DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${FILE}
    COMMAND glslangValidator
    ARGS -V -o ${CMAKE_CURRENT_BINARY_DIR}/${FILE_OUTPUT} ${CMAKE_CURRENT_SOURCE_DIR}/${FILE}
    WORKING_DIRECTORY .)
  list(APPEND VCC_TEST_COMPILED_SHADER_BINARIES ${FILE_OUTPUT})
endforeach()

add_test(vcc-tests vcc-test)

