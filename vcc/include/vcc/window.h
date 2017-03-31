/*
* Copyright 2016 Google Inc. All Rights Reserved.

* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at

* http://www.apache.org/licenses/LICENSE-2.0

* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
#ifndef _VCC_WINDOW_H_
#define _VCC_WINDOW_H_

#ifdef __ANDROID__
#include <android_native_app_glue.h>
#endif // __ANDROID__
#include <atomic>
#include <condition_variable>
#include <thread>
#include <vector>
#include <vcc/command_pool.h>
#include <vcc/device.h>
#include <vcc/image.h>
#include <vcc/image_view.h>
#include <vcc/instance.h>
#include <vcc/queue.h>
#include <vcc/surface.h>
#ifdef _WIN32
#include <windows.h>
#elif defined(VK_USE_PLATFORM_XCB_KHR)
#include <xcb/xcb_keysyms.h>
#endif

namespace vcc {
namespace window {

typedef std::function<void(VkFormat)> initialize_callback_type;
typedef std::function<void(VkExtent2D, VkFormat,
	std::vector<std::shared_ptr<image::image_type>> &&)> swapchain_create_callback_type;
typedef std::function<void()> swapchain_destroy_callback_type;
typedef std::function<void(uint32_t, const vcc::semaphore::semaphore_type &,
	const vcc::semaphore::semaphore_type &)> draw_callback_type;

enum mouse_button_type {
	mouse_button_left = 0,
	mouse_button_middle = 1,
	mouse_button_right = 2,
	mouse_button_4 = 3,
	mouse_button_5 = 4,
	mouse_button_6 = 5,
	mouse_button_7 = 6,
	mouse_button_8 = 7
};

typedef int keycode_type;
typedef std::function<bool(mouse_button_type, int, int)> mouse_press_callback_type;
typedef std::function<bool(int, int)> mouse_move_callback_type;
typedef std::function<bool(int)> mouse_scroll_callback_type;
typedef std::function<bool(keycode_type)> key_press_callback_type;
// first argument is the identifier of the touch
typedef std::function<bool(int, int, int)> touch_press_callback_type;
typedef std::function<bool(int, int, int)> touch_move_callback_type;

struct input_callbacks_type {
	VCC_LIBRARY input_callbacks_type();
	mouse_press_callback_type mouse_down_callback, mouse_up_callback;
	mouse_move_callback_type mouse_move_callback;
	mouse_scroll_callback_type mouse_scroll_callback;
	key_press_callback_type key_down_callback, key_up_callback;
	touch_press_callback_type touch_down_callback, touch_up_callback;
	touch_move_callback_type touch_move_callback;

	VCC_LIBRARY input_callbacks_type &set_mouse_down_callback(
		const mouse_press_callback_type &callback);
	VCC_LIBRARY input_callbacks_type &set_mouse_up_callback(
		const mouse_press_callback_type &callback);
	VCC_LIBRARY input_callbacks_type &set_mouse_move_callback(
		const mouse_move_callback_type &callback);
	VCC_LIBRARY input_callbacks_type &set_mouse_scroll_callback(
		const mouse_scroll_callback_type &callback);
	VCC_LIBRARY input_callbacks_type &set_key_down_callback(
		const key_press_callback_type &callback);
	VCC_LIBRARY input_callbacks_type &set_key_up_callback(
		const key_press_callback_type &callback);
	VCC_LIBRARY input_callbacks_type &set_touch_down_callback(
		const touch_press_callback_type &callback);
	VCC_LIBRARY input_callbacks_type &set_touch_up_callback(
		const touch_press_callback_type &callback);
	VCC_LIBRARY input_callbacks_type &set_touch_move_callback(
		const touch_move_callback_type &callback);
};

struct window_type {
	friend window_type create(
#ifdef _WIN32
		HINSTANCE hinstance,
#elif defined(VK_USE_PLATFORM_XCB_KHR)
		const char *displayname,
		int *screenp,
#elif defined(__ANDROID__)
		android_app* state,
#endif // __ANDROID__
		const type::supplier<const instance::instance_type> &instance,
		const type::supplier<const device::device_type> &device,
		const type::supplier<const queue::queue_type> &graphics_queue,
		VkExtent2D extent, VkFormat format, const std::string &title);
	friend void initialize(window_type &,
#ifdef _WIN32
		HINSTANCE,
		HWND
#elif defined(__ANDROID__)
		ANativeWindow *
#elif defined(VK_USE_PLATFORM_XCB_KHR)
		xcb_connection_t *
#endif
		);
	friend swapchain::swapchain_type resize(window_type &, VkExtent2D,
			const swapchain_create_callback_type &, const swapchain_destroy_callback_type &,
			swapchain::swapchain_type &);
	friend void draw(window_type &, swapchain::swapchain_type &,
		vcc::semaphore::semaphore_type &, vcc::semaphore::semaphore_type &,
		const draw_callback_type &, const swapchain_create_callback_type &,
		const swapchain_destroy_callback_type &, VkExtent2D);
	friend int run(window_type &, const swapchain_create_callback_type &,
		const swapchain_destroy_callback_type &, const draw_callback_type &,
		const input_callbacks_type &);
	friend const queue::queue_type &get_present_queue(const window_type &);

	window_type() = default;
	window_type(const window_type&) = delete;
	window_type(window_type&&) = default;
	window_type &operator=(const window_type&) = delete;
	window_type &operator=(window_type&&) = default;

private:
#ifdef VK_USE_PLATFORM_XCB_KHR
	typedef std::unique_ptr<xcb_connection_t, decltype(&xcb_disconnect)> connection_type;
	typedef internal::managed_type<xcb_window_t,
	  decltype(std::bind(&xcb_destroy_window, (xcb_connection_t *) nullptr, std::placeholders::_1))>
		window_handle_type;
#endif // VK_USE_PLATFORM_XCB_KHR
	window_type(
#ifdef _WIN32
		HINSTANCE connection,
#elif defined(__ANDROID__)
		android_app* state,
#elif defined(VK_USE_PLATFORM_XCB_KHR)
		connection_type &&connection,
		window_handle_type &&window,
		VkExtent2D extent,
#endif // __ANDROID__
		type::supplier<const instance::instance_type> instance,
		const type::supplier<const device::device_type> &device,
		const type::supplier<const queue::queue_type> &graphics_queue)
		: instance(instance)
#if defined(__ANDROID__)
		, state(state)
#elif defined(VK_USE_PLATFORM_XCB_KHR)
		, connection(std::forward<connection_type>(connection))
		, window(std::forward<window_handle_type>(window))
		, extent(extent)
		, atom_wm_delete_window(nullptr, free)
		, key_symbols(nullptr, xcb_key_symbols_free)
#endif // VK_USE_PLATFORM_XCB_KHR
		, device(device), graphics_queue(graphics_queue) {}

#ifdef _WIN32
	typedef internal::managed_type<HWND, decltype(&DestroyWindow)> window_handle_type;
	window_handle_type window;
#elif defined(__ANDROID__)
	android_app* state;
#elif defined(VK_USE_PLATFORM_XCB_KHR)
	connection_type connection;
	window_handle_type window;
	typedef std::unique_ptr<xcb_intern_atom_reply_t, decltype(&free)> atom_reply_t;
	atom_reply_t atom_wm_delete_window;
	VkExtent2D extent;
	typedef std::unique_ptr<xcb_key_symbols_t, decltype(&xcb_key_symbols_free)> key_symbols_t;
	key_symbols_t key_symbols;
#endif // __ANDROID__
	type::supplier<const instance::instance_type> instance;
	surface::surface_type surface;
	type::supplier<const device::device_type> device;
	type::supplier<const queue::queue_type> graphics_queue;
	queue::queue_type present_queue;
	VkFormat format;
	VkColorSpaceKHR color_space;
	command_pool::command_pool_type cmd_pool;
};

VCC_LIBRARY window_type create(
#ifdef _WIN32
		HINSTANCE hinstance,
#elif defined(VK_USE_PLATFORM_XCB_KHR)
		const char *displayname,
		int *screenp,
#elif defined(__ANDROID__)
		android_app* state,
#endif // __ANDROID__
	const type::supplier<const instance::instance_type> &instance,
	const type::supplier<const device::device_type> &device,
	const type::supplier<const queue::queue_type> &graphics_queue,
	VkExtent2D extent, VkFormat format, const std::string &title);

VCC_LIBRARY int run(window_type &window,
	const swapchain_create_callback_type &swapchain_create_callback,
	const swapchain_destroy_callback_type &swapchain_destroy_callback,
	const draw_callback_type &draw_callback,
	const input_callbacks_type &input_callbacks = input_callbacks_type());

inline const queue::queue_type &get_present_queue(const window_type &window) {
	return window.present_queue;
}

}  // namespace window
}  // namespace vcc

#endif // _VCC_WINDOW_H_
