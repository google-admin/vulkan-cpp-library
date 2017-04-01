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
include_directories(../types/include)
include_directories(../vcc/include)
include_directories(${VULKAN_CPP_LIBRARY_BINARY_DIR}/vcc/include)
include_directories(${GLI_SRC_DIR})
include_directories(${GLI_SRC_DIR}/external)
if(NOT PNG_INCLUDE_DIR STREQUAL "")
  include_directories(${PNG_INCLUDE_DIR})
endif()
if(NOT VULKAN_SDK_DIR STREQUAL "")
  include_directories(${VULKAN_SDK_DIR}/include)
endif()
remove_definitions(-DGLM_ENABLE_EXPERIMENTAL)

set(VCC_IMAGE_INCLUDES
  "include/vcc/image_loader.h"
  "include/vcc/internal/loader.h"
)

set(VCC_IMAGE_SRCS
  "src/image.cpp"
  "src/png_loader.cpp"
  "src/gli_loader.cpp"
)

if(DEFINED ANDROID_NDK)
  list(APPEND VCC_IMAGE_SRCS "src/android_loader.cpp")
endif()

add_library(vcc-image ${VCC_IMAGE_INCLUDES} ${VCC_IMAGE_SRCS})

if(DEFINED ANDROID_NDK)
  target_link_libraries(vcc-image vcc)
else()
  target_link_libraries(vcc-image vcc png)
endif()
