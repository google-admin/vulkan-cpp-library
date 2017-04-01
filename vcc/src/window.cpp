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
#define NOMINMAX
#include <algorithm>
#include <cassert>
#include <chrono>
#include <vcc/command.h>
#include <vcc/physical_device.h>
#include <vcc/surface.h>
#include <vcc/window.h>
#ifdef _WIN32
#include <windowsx.h>
#endif // WIN32

namespace vcc {
namespace window {

input_callbacks_type::input_callbacks_type()
	: mouse_down_callback([](mouse_button_type, int, int) {return false; }),
	  mouse_up_callback([](mouse_button_type, int, int) {return false; }),
	  mouse_move_callback([](int, int) {return false; }),
	  key_down_callback([](keycode_type) {return false; }),
	  key_up_callback([](keycode_type) {return false; }) {}

input_callbacks_type &input_callbacks_type::set_mouse_down_callback(
		const mouse_press_callback_type &callback) {
	mouse_down_callback = callback;
	return *this;
}

input_callbacks_type &input_callbacks_type::set_mouse_up_callback(
		const mouse_press_callback_type &callback) {
	mouse_up_callback = callback;
	return *this;
}

input_callbacks_type &input_callbacks_type::set_mouse_move_callback(
		const mouse_move_callback_type &callback) {
	mouse_move_callback = callback;
	return *this;
}

input_callbacks_type &input_callbacks_type::set_mouse_scroll_callback(
	const mouse_scroll_callback_type &callback) {
	mouse_scroll_callback = callback;
	return *this;
}

input_callbacks_type &input_callbacks_type::set_key_down_callback(
		const key_press_callback_type &callback) {
	key_down_callback = callback;
	return *this;
}

input_callbacks_type &input_callbacks_type::set_key_up_callback(
		const key_press_callback_type &callback) {
	key_up_callback = callback;
	return *this;
}

input_callbacks_type &input_callbacks_type::set_touch_down_callback(
		const touch_press_callback_type &callback) {
	touch_down_callback = callback;
	return *this;
}

input_callbacks_type &input_callbacks_type::set_touch_up_callback(
		const touch_press_callback_type &callback) {
	touch_up_callback = callback;
	return *this;
}

input_callbacks_type &input_callbacks_type::set_touch_move_callback(
		const touch_move_callback_type &callback) {
	touch_move_callback = callback;
	return *this;
}

#ifdef _WIN32
const char *class_name = "vcc-vulkan";
#endif // WIN32

swapchain::swapchain_type resize(window_type &window, VkExtent2D extent,
		const swapchain_create_callback_type &swapchain_create_callback,
		const swapchain_destroy_callback_type &swapchain_destroy_callback,
		swapchain::swapchain_type &old_swapchain) {
	// Check the surface capabilities and formats
	const VkPhysicalDevice physical_device(device::get_physical_device(*window.device));
	const VkSurfaceCapabilitiesKHR surfCapabilities(vcc::surface::physical_device_capabilities(physical_device, window.surface));
	// width and height are either both -1, or both not -1.
	extent = surfCapabilities.currentExtent.width == -1 ? extent : surfCapabilities.currentExtent;

	// If mailbox mode is available, use it, as is the lowest-latency non-
	// tearing mode.  If not, try IMMEDIATE which will usually be available,
	// and is fastest (though it tears).  If not, fall back to FIFO which is
	// always available.
	const std::vector<VkPresentModeKHR> presentModes(vcc::surface::physical_device_present_modes(physical_device, window.surface));
	VkPresentModeKHR swapchainPresentMode(*std::max_element(presentModes.begin(), presentModes.end(), [](VkPresentModeKHR mode1, VkPresentModeKHR mode2) {
		if (mode1 != mode2) {
			switch (mode2) {
			case VK_PRESENT_MODE_MAILBOX_KHR:
				return true;
			case VK_PRESENT_MODE_IMMEDIATE_KHR:
				return mode1 != VK_PRESENT_MODE_MAILBOX_KHR;
			case VK_PRESENT_MODE_FIFO_KHR:
				return mode1 != VK_PRESENT_MODE_MAILBOX_KHR && mode1 != VK_PRESENT_MODE_IMMEDIATE_KHR;
			}
		}
		return false;
	}));

	// Determine the number of VkImage's to use in the swap chain (we desire to
	// own only 1 image at a time, besides the images being displayed and
	// queued for display):
	uint32_t desiredNumberOfSwapchainImages = surfCapabilities.minImageCount + 1;
	if (surfCapabilities.maxImageCount > 0) {
		// Application might have to settle for fewer images than desired:
		desiredNumberOfSwapchainImages = std::min(surfCapabilities.maxImageCount, desiredNumberOfSwapchainImages);
	}
	const VkSurfaceTransformFlagBitsKHR preTransform(
	  surfCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
	    ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : surfCapabilities.currentTransform);
	swapchain_destroy_callback();
	swapchain::swapchain_type swapchain(vcc::swapchain::create(window.device,
		vcc::swapchain::create_info_type{ std::ref(window.surface), desiredNumberOfSwapchainImages,
		window.format, window.color_space, extent,
			1, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_SHARING_MODE_EXCLUSIVE,{}, preTransform,
			VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			swapchainPresentMode, VK_TRUE, type::make_supplier(old_swapchain) }));
	std::vector<vcc::image::image_type> images(vcc::swapchain::get_images(swapchain));
	std::vector<std::shared_ptr<vcc::image::image_type>> swapchain_images;
	swapchain_images.reserve(images.size());

	vcc::command_buffer::command_buffer_type command_buffer(std::move(
		vcc::command_buffer::allocate(window.device, std::ref(window.cmd_pool),
			VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1).front()));
	{
		vcc::command::build_type build(
			vcc::command::build(std::ref(command_buffer), 0, VK_FALSE, 0, 0));
		for (vcc::image::image_type &si : images) {
			std::shared_ptr<vcc::image::image_type> swapchain_image(
				std::make_shared<vcc::image::image_type>(std::move(si)));

			// Render loop will expect image to have been used before and in VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
			// layout and will change to COLOR_ATTACHMENT_OPTIMAL, so init the image to that state
			vcc::command::compile(build,
				vcc::command::pipeline_barrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, {}, {},
					{
						vcc::command::image_memory_barrier{ 0, 0,
							VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
							queue::get_family_index(*window.graphics_queue),
							queue::get_family_index(window.present_queue),
							swapchain_image, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
						}
					}));

			swapchain_images.push_back(std::move(swapchain_image));
		}
	}
	vcc::fence::fence_type fence(vcc::fence::create(window.device));
	vcc::queue::submit(window.present_queue, {}, { command_buffer }, {}, fence);
	vcc::fence::wait(*window.device, { fence }, true);

	swapchain_create_callback(extent, window.format, std::move(swapchain_images));
	return swapchain;
}

void draw(window_type &window, swapchain::swapchain_type &swapchain,
		vcc::semaphore::semaphore_type &image_acquired_semaphore,
		vcc::semaphore::semaphore_type &draw_semaphore,
		const draw_callback_type &draw_callback,
		const swapchain_create_callback_type &swapchain_create_callback,
		const swapchain_destroy_callback_type &swapchain_destroy_callback, VkExtent2D extent) {
	VkResult err;
	uint32_t current_buffer;
	do {
		std::tie(err, current_buffer) = vcc::swapchain::acquire_next_image(
			swapchain, image_acquired_semaphore);
		switch (err) {
		case VK_ERROR_OUT_OF_DATE_KHR:
			// swapchain is out of date (e.g. the window was resized) and
			// must be recreated:
			swapchain = resize(window, extent, swapchain_create_callback,
				swapchain_destroy_callback, swapchain);
			break;
		case VK_SUBOPTIMAL_KHR:
			VCC_PRINT("VK_SUBOPTIMAL_KHR");
			// swapchain is not as optimal as it could be, but the platform's
			// presentation engine will still present the image correctly.
			break;
		default:
			if (err) {
				VCC_PRINT("vcc::swapchain::acquire_next_image resulted in %u", err);
			}
			assert(!err);
			break;
		}
	} while (err == VK_ERROR_OUT_OF_DATE_KHR);

	draw_callback(current_buffer, image_acquired_semaphore, draw_semaphore);

	err = vcc::queue::present(window.present_queue, { draw_semaphore }, { swapchain },
		{ current_buffer });

	vcc::queue::wait_idle(window.present_queue);

	switch (err) {
	case VK_ERROR_OUT_OF_DATE_KHR:
		// swapchain is out of date (e.g. the window was resized) and
		// must be recreated:
		swapchain = resize(window, extent, swapchain_create_callback, swapchain_destroy_callback,
			swapchain);
		break;
	case VK_SUBOPTIMAL_KHR:
		// swapchain is not as optimal as it could be, but the platform's
		// presentation engine will still present the image correctly.
		VCC_PRINT("VK_SUBOPTIMAL_KHR");
		break;
	default:
		assert(!err);
	}
}

void initialize(window_type &window,
#ifdef _WIN32
		HINSTANCE connection,
		HWND window_handle
#elif defined(__ANDROID__)
		ANativeWindow *window_handle
#elif defined(VK_USE_PLATFORM_XCB_KHR)
		xcb_connection_t *connection
#endif
		) {
#if defined(_WIN32)
	window.window = window_type::window_handle_type(window_handle, &DestroyWindow);
#endif // _WIN32
	window.surface = vcc::surface::create(window.instance,
#if defined(_WIN32) || defined(VK_USE_PLATFORM_XCB_KHR)
		connection,
		window.window);
#elif defined(__ANDROID__)
		window_handle);
#endif

	window.present_queue = queue::get_present_queue(window.device, window.surface);

	window.cmd_pool = vcc::command_pool::create(window.device, 0,
		vcc::queue::get_family_index(window.present_queue));

	// Get the list of VkFormat's that are supported:
	const VkPhysicalDevice physical_device(device::get_physical_device(*window.device));
	const std::vector<VkSurfaceFormatKHR> surface_formats(
		vcc::surface::physical_device_formats(physical_device, window.surface));
	// If the format list includes just one entry of VK_FORMAT_UNDEFINED,
	// the surface has no preferred format.  Otherwise, at least one
	// supported format will be returned.
	// TODO(gardell): Pick a requested format
	assert(!surface_formats.empty());
	window.format = surface_formats.front().format == VK_FORMAT_UNDEFINED
		? VK_FORMAT_B8G8R8A8_UNORM : surface_formats.front().format;
	window.color_space = surface_formats.front().colorSpace;
}

#ifdef _WIN32
typedef std::function<LRESULT(HWND, UINT, WPARAM, LPARAM)> wnd_proc_type;

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_CREATE:
	case WM_CLOSE:
	case WM_SIZE:
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_XBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
	case WM_XBUTTONUP:
	case WM_MOUSEMOVE:
	case WM_MOUSEWHEEL:
	case WM_KEYDOWN:
	case WM_KEYUP: {
		wnd_proc_type &wnd_proc(*reinterpret_cast<wnd_proc_type *>(
			GetWindowLongPtr(hWnd, GWLP_USERDATA)));
		return wnd_proc(hWnd, uMsg, wParam, lParam);
		} break;
	case WM_NCCREATE:
	{
		wnd_proc_type &wnd_proc(*reinterpret_cast<wnd_proc_type *>(
			((LPCREATESTRUCT)lParam)->lpCreateParams));
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)&wnd_proc);
	}
	// no break
	default:
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
}
#elif defined(__ANDROID__)

template<typename CommandF, typename InputF>
struct callbacks_type {
	CommandF command_function;
	InputF input_function;
};

template<typename CommandF, typename InputF>
int32_t engine_handle_input(struct android_app* app, AInputEvent* event) {
	return reinterpret_cast<callbacks_type<CommandF, InputF> *>(app->userData)
		->input_function(*app, *event);
}

template<typename CommandF, typename InputF>
void engine_handle_cmd(struct android_app* app, int32_t cmd) {
	reinterpret_cast<callbacks_type<CommandF, InputF> *>(app->userData)->command_function(*app,
		cmd);
}

#endif // __ANDROID__

window_type create(
#ifdef _WIN32
	HINSTANCE connection,
#elif defined(VK_USE_PLATFORM_XCB_KHR)
	const char *displayname,
	int *screenp,
#elif defined(__ANDROID__)
	android_app* state,
#endif // __ANDROID__
	const type::supplier<const instance::instance_type> &instance,
	const type::supplier<const device::device_type> &device,
	const type::supplier<const queue::queue_type> &graphics_queue,
	VkExtent2D extent, VkFormat format, const std::string &title) {

#ifdef VK_USE_PLATFORM_XCB_KHR
	window_type::connection_type connection(xcb_connect(displayname, screenp), xcb_disconnect);
	window_type::window_handle_type window_handle(xcb_generate_id(connection.get()),
		std::bind(&xcb_destroy_window, (xcb_connection_t *) connection.get(), std::placeholders::_1));
#endif // VK_USE_PLATFORM_XCB_KHR

	window_type window(
#ifdef _WIN32
    connection,
#elif defined(VK_USE_PLATFORM_XCB_KHR)
		std::move(connection),
		std::move(window_handle),
		extent,
#elif defined(__ANDROID__)
		state,
#endif // _WIN32
		instance, device, graphics_queue);

#ifdef _WIN32
	WNDCLASSEX  win_class;
	win_class.cbSize = sizeof(WNDCLASSEX);

	if (!GetClassInfoEx(connection, class_name, &win_class)) {
		// Initialize the window class structure:
		win_class.style = CS_HREDRAW | CS_VREDRAW;
		win_class.lpfnWndProc = WndProc;
		win_class.cbClsExtra = 0;
		win_class.cbWndExtra = 0;
		win_class.hInstance = connection;
		win_class.hIcon = LoadIcon(NULL, IDI_APPLICATION);
		win_class.hCursor = LoadCursor(NULL, IDC_ARROW);
		win_class.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
		win_class.lpszMenuName = NULL;
		win_class.lpszClassName = class_name;
		win_class.hIconSm = LoadIcon(NULL, IDI_WINLOGO);
		// Register window class:
		if (!RegisterClassEx(&win_class)) {
			throw vcc::vcc_exception("RegisterClassEx");
		}
	}

	wnd_proc_type wnd_proc([&](HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
		switch (uMsg) {
		case WM_CREATE:
			initialize(window, connection, hWnd);
			break;
		case WM_CLOSE:
			PostQuitMessage(0);
			break;
		}
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	});

	// Create window with the registered class:
	RECT wr = { 0, 0, (LONG) extent.width, (LONG) extent.height };
	AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
	if (!CreateWindowEx(0, class_name, title.c_str(),
		WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_SYSMENU,
		100, 100, wr.right - wr.left, wr.bottom - wr.top, NULL, NULL, connection, &wnd_proc)) {
		throw vcc::vcc_exception("CreateWindowEx failed");
	}
#elif defined(VK_USE_PLATFORM_XCB_KHR)
	if (xcb_connection_has_error(window.connection.get())) {
	  throw vcc::vcc_exception("xcb_connect failed");
  }
	xcb_screen_t *screen(xcb_setup_roots_iterator(xcb_get_setup(window.connection.get())).data);
	uint32_t value_list[] = { XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE
		| XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY
		| XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_BUTTON_MOTION
		| XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_BUTTON_PRESS };
	xcb_create_window(window.connection.get(), screen->root_depth, window.window, screen->root,
		100, 100, extent.width, extent.height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual,
	  XCB_CW_EVENT_MASK, value_list);
	xcb_intern_atom_cookie_t cookie(xcb_intern_atom(window.connection.get(), 1, 12, "WM_PROTOCOLS"));
	auto reply(window_type::atom_reply_t(xcb_intern_atom_reply(window.connection.get(), cookie, 0),
		free));

	xcb_intern_atom_cookie_t cookie2(xcb_intern_atom(window.connection.get(), 0, 16,
		"WM_DELETE_WINDOW"));
	window.atom_wm_delete_window.reset(xcb_intern_atom_reply(window.connection.get(), cookie2, 0));
	window.key_symbols.reset(xcb_key_symbols_alloc(window.connection.get()));

	xcb_change_property(window.connection.get(), XCB_PROP_MODE_REPLACE, window.window, reply->atom,
		4, 32, 1, &window.atom_wm_delete_window->atom);

	xcb_map_window(window.connection.get(), window.window);
	if (!xcb_flush(window.connection.get())) {
	  throw vcc::vcc_exception("xcb_flush xcb_create_window xcb_map_window failed");
  }
  vcc::window::initialize(window, window.connection.get());
#endif

	return window;
}

int run(window_type &window, const swapchain_create_callback_type &swapchain_create_callback,
	const swapchain_destroy_callback_type &swapchain_destroy_callback,
	const draw_callback_type &draw_callback, const input_callbacks_type &input_callbacks) {
#ifdef _WIN32
	std::atomic<VkExtent2D> extent;
	{
		RECT rect;
		GetClientRect(window.window, &rect);
		extent = { uint32_t(rect.right), uint32_t(rect.bottom) };
	}
	wnd_proc_type wnd_proc([&](HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
		switch (uMsg) {
		case WM_CLOSE:
			PostQuitMessage(0);
			break;
		case WM_SIZE:
			extent = { uint32_t(lParam & 0xffff), uint32_t(lParam & 0xffff0000 >> 16) };
			break;
		case WM_LBUTTONDOWN:
			input_callbacks.mouse_down_callback(mouse_button_left,
				GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			break;
		case WM_MBUTTONDOWN:
			input_callbacks.mouse_down_callback(mouse_button_middle,
				GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			break;
		case WM_RBUTTONDOWN:
			input_callbacks.mouse_down_callback(mouse_button_right,
				GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			break;
		case WM_XBUTTONDOWN:
			switch (GET_XBUTTON_WPARAM(wParam)) {
			case XBUTTON1:
				input_callbacks.mouse_down_callback(mouse_button_4,
					GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
				break;
			case XBUTTON2:
				input_callbacks.mouse_down_callback(mouse_button_5,
					GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
				break;
			}
			break;
		case WM_LBUTTONUP:
			input_callbacks.mouse_up_callback(mouse_button_left,
				GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			break;
		case WM_MBUTTONUP:
			input_callbacks.mouse_up_callback(mouse_button_middle,
				GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			break;
		case WM_RBUTTONUP:
			input_callbacks.mouse_up_callback(mouse_button_right,
				GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			break;
		case WM_XBUTTONUP:
			switch (GET_XBUTTON_WPARAM(wParam)) {
			case XBUTTON1:
				input_callbacks.mouse_up_callback(mouse_button_4,
					GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
				break;
			case XBUTTON2:
				input_callbacks.mouse_up_callback(mouse_button_5,
					GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
				break;
			}
			break;
		case WM_MOUSEMOVE:
			input_callbacks.mouse_move_callback(
				GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			break;
		case WM_MOUSEWHEEL: {
			input_callbacks.mouse_scroll_callback(
				GET_WHEEL_DELTA_WPARAM(wParam));
		} break;
		case WM_KEYDOWN:
			input_callbacks.key_down_callback(keycode_type(wParam));
			break;
		case WM_KEYUP:
			input_callbacks.key_up_callback(keycode_type(wParam));
			break;
		default:
			break;
		}
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	});
	SetWindowLongPtr(window.window, GWLP_USERDATA, (LONG_PTR)&wnd_proc);

	swapchain::swapchain_type swapchain;
	vcc::semaphore::semaphore_type image_acquired_semaphore(vcc::semaphore::create(window.device)),
		draw_semaphore(vcc::semaphore::create(window.device));
	std::atomic_bool running(true);
	std::thread render_thread([&]() {
		VkExtent2D draw_extent{ 0, 0 };
		while (running) {
			VkExtent2D resize_extent(extent.exchange({ 0, 0 }));
			if (resize_extent.width != 0 && resize_extent.height != 0
				&& (resize_extent.width != draw_extent.width
					|| resize_extent.height != draw_extent.height)) {
				swapchain = resize(window, resize_extent, swapchain_create_callback,
					swapchain_destroy_callback, swapchain);
				draw_extent = resize_extent;
			}
			draw(window, swapchain, image_acquired_semaphore, draw_semaphore, draw_callback,
				swapchain_create_callback, swapchain_destroy_callback, draw_extent);
		}
	});

	MSG msg;
	for (;;) {
		GetMessage(&msg, NULL, 0, 0);
		if (msg.message == WM_QUIT) {
			break;
		} else {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	device::wait_idle(*window.device);
	running = false;
	render_thread.join();
	return (int) msg.wParam;

#elif defined(VK_USE_PLATFORM_XCB_KHR)

	swapchain::swapchain_type swapchain;

	std::atomic_bool running(true);
	std::atomic<VkExtent2D> extent(window.extent);

  for (;;) {
    std::unique_ptr<xcb_generic_event_t, decltype(&free)> event(
      xcb_wait_for_event(window.connection.get()), &free);
    uint8_t event_code = event->response_type & 0x7f;
    if (event_code == XCB_CONFIGURE_NOTIFY) {
      const xcb_configure_notify_event_t *cfg =
        (const xcb_configure_notify_event_t *) event.get();
      extent = VkExtent2D{ cfg->width, cfg->height };
      swapchain = resize(window, extent, swapchain_create_callback, swapchain_destroy_callback,
		  swapchain);
      break;
    }
  }

  vcc::semaphore::semaphore_type image_acquired_semaphore(vcc::semaphore::create(window.device)),
    draw_semaphore(vcc::semaphore::create(window.device));

  std::thread event_thread([&]() {
    while (running) {
      std::unique_ptr<xcb_generic_event_t, decltype(&free)> event(
        xcb_wait_for_event(window.connection.get()), &free);
      uint8_t event_code = event->response_type & 0x7f;
      switch (event_code) {
      case XCB_CONFIGURE_NOTIFY: {
        const xcb_configure_notify_event_t *cfg =
          (const xcb_configure_notify_event_t *) event.get();
        extent = VkExtent2D{ cfg->width, cfg->height };
        } break;
      case XCB_CLIENT_MESSAGE: {
          auto message = (xcb_client_message_event_t *) event.get();
          if (message->data.data32[0] == window.atom_wm_delete_window->atom) {
            vcc::device::wait_idle(*window.device);
            running = false;
          }
        } break;
      case XCB_BUTTON_PRESS: {
        auto bp = (xcb_button_press_event_t *) event.get();
        input_callbacks.mouse_down_callback(mouse_button_type(bp->detail - 1),
            bp->event_x, bp->event_y);
        } break;
      case XCB_BUTTON_RELEASE: {
        auto br = (xcb_button_release_event_t *) event.get();
        input_callbacks.mouse_up_callback(mouse_button_type(br->detail - 1),
            br->event_x, br->event_y);
        } break;
      case XCB_MOTION_NOTIFY: {
        auto motion = (xcb_motion_notify_event_t *) event.get();
        input_callbacks.mouse_move_callback(motion->event_x, motion->event_y);
        } break;
      case XCB_KEY_PRESS: {
        auto kp = (xcb_key_press_event_t *) event.get();
        input_callbacks.key_down_callback(keycode_type(xcb_key_symbols_get_keysym(
			window.key_symbols.get(), kp->detail, 0)));
        } break;
      case XCB_KEY_RELEASE: {
        auto kr = (xcb_key_release_event_t *) event.get();
        input_callbacks.key_up_callback(keycode_type(xcb_key_symbols_get_keysym(
			window.key_symbols.get(), kr->detail, 0)));
        } break;
      }
    }
  });

  VkExtent2D draw_extent{ 0, 0 };
  while (running) {
    VkExtent2D resize_extent(extent.exchange({ 0, 0 }));
    if (resize_extent.width != 0 && resize_extent.height != 0) {
      swapchain = resize(window, extent, swapchain_create_callback, swapchain_destroy_callback,
		  swapchain);
      draw_extent = resize_extent;
    }
    draw(window, swapchain, image_acquired_semaphore, draw_semaphore, draw_callback,
		swapchain_create_callback, swapchain_destroy_callback, draw_extent);
  }

  event_thread.join();
  device::wait_idle(*window.device);
  window.extent = extent;

#elif defined(__ANDROID__)
	swapchain::swapchain_type swapchain;
	vcc::semaphore::semaphore_type image_acquired_semaphore(vcc::semaphore::create(window.device)),
			draw_semaphore(vcc::semaphore::create(window.device));
	std::atomic_bool running;
	std::thread render_thread;
	VkExtent2D extent;
	auto handle_cmd([&](struct android_app &app, int32_t cmd) {
		switch (cmd) {
			case APP_CMD_SAVE_STATE:
				// The system has asked us to save our current state.  Do so.
				break;
			case APP_CMD_INIT_WINDOW:
				// The window is being shown, get it ready.
				extent = { (uint32_t) ANativeWindow_getWidth(app.window),
						(uint32_t) ANativeWindow_getHeight(app.window) };
				vcc::window::initialize(window, app.window);
				swapchain = resize(window, extent, swapchain_create_callback,
					swapchain_destroy_callback, swapchain);
				break;
			case APP_CMD_GAINED_FOCUS:
				running = true;
				render_thread = std::thread([&]() {
					while (running) {
						draw(window, swapchain, image_acquired_semaphore, draw_semaphore,
							draw_callback, swapchain_create_callback, swapchain_destroy_callback,
							extent);
					}
				});
				break;
			case APP_CMD_LOST_FOCUS:
				running = false;
				render_thread.join();
				break;
			case APP_CMD_TERM_WINDOW:
				running = false;
				if (render_thread.joinable()) {
					render_thread.join();
				}
				swapchain = swapchain::swapchain_type();
				break;
		}
	});
	auto handle_input([&](struct android_app &app, AInputEvent &event)->int32_t {
		switch (AInputEvent_getType(&event)) {
			case AINPUT_EVENT_TYPE_MOTION: {
				const size_t count(AMotionEvent_getPointerCount(&event));
				bool handled = false;
				switch (AMotionEvent_getAction(&event)) {
					case AMOTION_EVENT_ACTION_MOVE:
					for (size_t i = 0; i < count; ++i) {
						const int32_t id(AMotionEvent_getPointerId(&event, i));
						handled |= input_callbacks.touch_move_callback(
								id, AMotionEvent_getX(&event, i), AMotionEvent_getY(&event, i));
					}
					return !!handled;
					case AMOTION_EVENT_ACTION_DOWN:
						for (size_t i = 0; i < count; ++i) {
							const int32_t id(AMotionEvent_getPointerId(&event, i));
							handled |= input_callbacks.touch_down_callback(
									id, AMotionEvent_getX(&event, i), AMotionEvent_getY(&event, i));
						}
						return !!handled;
					case AMOTION_EVENT_ACTION_UP:
						for (size_t i = 0; i < count; ++i) {
							const int32_t id(AMotionEvent_getPointerId(&event, i));
							handled |= input_callbacks.touch_up_callback(
									id, AMotionEvent_getX(&event, i), AMotionEvent_getY(&event, i));
						}
						return !!handled;
				}
			} break;
			case AINPUT_EVENT_TYPE_KEY:
				switch (AKeyEvent_getAction(&event)) {
					case AKEY_EVENT_ACTION_DOWN:
						input_callbacks.key_down_callback(AKeyEvent_getKeyCode(&event));
						break;
					case AKEY_EVENT_ACTION_UP:
						input_callbacks.key_up_callback(AKeyEvent_getKeyCode(&event));
						break;
				}
				break;
		}
		return 0;
	});
	callbacks_type<decltype(handle_cmd), decltype(handle_input)> callbacks{
		handle_cmd, handle_input };
	window.state->userData = &callbacks;
	window.state->onAppCmd = engine_handle_cmd<decltype(handle_cmd), decltype(handle_input)>;
	window.state->onInputEvent = engine_handle_input<decltype(handle_cmd), decltype(handle_input)>;
	for (;;) {
		// Read all pending events.
		int events;
		android_poll_source *source;
		for (int ident;
				(ident = ALooper_pollAll(-1, NULL, &events, (void**) &source)) >= 0;) {

			if (source) {
				source->process(window.state, source);
			}

			if (window.state->destroyRequested != 0) {
				return 0;
			}
		}
	}
#endif // __ANDROID__
	return 0;
}

}  // namespace window
}  // namespace vcc
