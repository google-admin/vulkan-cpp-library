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
include(GenerateExportHeader)
include_directories(include)
include_directories(${VULKAN_CPP_LIBRARY_BINARY_DIR}/vcc/include)
include_directories(../types/include)
include_directories(${GLM_SRC_DIR})
if(NOT VULKAN_SDK_DIR STREQUAL "")
  include_directories(${VULKAN_SDK_DIR}/include)
  link_directories(${VULKAN_SDK_DIR}/bin)
endif()
if(DEFINED ANDROID_NDK)
  include_directories(${ANDROID_NDK}/sources/android/native_app_glue/)
endif()

set(VCC_INCLUDES
  "include/vcc/command.h"
  "include/vcc/descriptor_set.h"
  "include/vcc/framebuffer.h"
  "include/vcc/pipeline_layout.h"
  "include/vcc/physical_device.h"
  "include/vcc/event.h"
  "include/vcc/util.h"
  "include/vcc/fence.h"
  "include/vcc/query_pool.h"
  "include/vcc/command_buffer.h"
  "include/vcc/image.h"
  "include/vcc/swapchain.h"
  "include/vcc/device.h"
  "include/vcc/render_pass.h"
  "include/vcc/sampler.h"
  "include/vcc/buffer.h"
  "include/vcc/image_view.h"
  "include/vcc/memory.h"
  "include/vcc/command_pool.h"
  "include/vcc/input_buffer.h"
  "include/vcc/pipeline_cache.h"
  "include/vcc/descriptor_set_layout.h"
  "include/vcc/semaphore.h"
  "include/vcc/enumerate.h"
  "include/vcc/surface.h"
  "include/vcc/pipeline.h"
  "include/vcc/shader_module.h"
  "include/vcc/window.h"
  "include/vcc/internal/raii.h"
  "include/vcc/internal/hook.h"
  "include/vcc/descriptor_pool.h"
  "include/vcc/instance.h"
  "include/vcc/queue.h"
  "include/vcc/debug.h"
  "include/vcc/buffer_view.h"
)

if(DEFINED ANDROID_NDK)
  list(APPEND VCC_INCLUDES "include/vcc/android_asset_istream.h")
endif()

set(VCC_SRCS
  "src/physical_device.cpp"
  "src/swapchain.cpp"
  "src/image.cpp"
  "src/util.cpp"
  "src/pipeline_layout.cpp"
  "src/memory.cpp"
  "src/buffer.cpp"
  "src/surface.cpp"
  "src/fence.cpp"
  "src/debug.cpp"
  "src/main.cpp"
  "src/sampler.cpp"
  "src/descriptor_set_layout.cpp"
  "src/queue.cpp"
  "src/input_buffer.cpp"
  "src/buffer_view.cpp"
  "src/device.cpp"
  "src/command.cpp"
  "src/event.cpp"
  "src/framebuffer.cpp"
  "src/descriptor_set.cpp"
  "src/pipeline_cache.cpp"
  "src/pipeline.cpp"
  "src/semaphore.cpp"
  "src/shader_module.cpp"
  "src/image_view.cpp"
  "src/command_pool.cpp"
  "src/enumerate.cpp"
  "src/descriptor_pool.cpp"
  "src/render_pass.cpp"
  "src/command_buffer.cpp"
  "src/window.cpp"
  "src/instance.cpp"
)

if(DEFINED ANDROID_NDK)
  list(APPEND VCC_SRCS "src/android_asset_istream.cpp")
endif()

add_library(vcc ${VCC_INCLUDES} ${VCC_SRCS})
add_definitions(-D_ENABLE_ATOMIC_ALIGNMENT_FIX)
generate_export_header(vcc
  BASE_NAME vcc
  EXPORT_MACRO_NAME VCC_LIBRARY
  EXPORT_FILE_NAME include/vcc/export.h
  STATIC_DEFINE VCC_LIBRARY_BUILT_AS_STATIC)

target_link_libraries(vcc types ${VULKAN_LIBRARY})

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  target_link_libraries(vcc pthread xcb xcb-keysyms atomic)
endif()
