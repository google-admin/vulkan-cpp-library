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
/*
 * Simple OpenVR rendering example.
 *
 * Renders the devices using models provided by OpenVR.
 * Loads models and regenerates command buffers asynchronously.
 *
 * Notice that as of writing this, OpenVR does not support Vulkan.
 * This sample is using GL_NV_draw_vulkan_image to blit
 * Vulkan images to OpenGL.
 */
#include <algorithm>
#include <async_cache.h>
#include <cassert>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <iterator>
#include <queue>
#include <sstream>
#include <thread_pool.h>
#include <type/types.h>
#include <vcc/android_asset_istream.h>
#include <vcc/buffer.h>
#include <vcc/command.h>
#include <vcc/command_buffer.h>
#include <vcc/command_pool.h>
#include <vcc/descriptor_pool.h>
#include <vcc/descriptor_set.h>
#include <vcc/descriptor_set_layout.h>
#include <vcc/device.h>
#include <vcc/enumerate.h>
#include <vcc/framebuffer.h>
#include <vcc/image.h>
#include <vcc/image_view.h>
#include <vcc/image_loader.h>
#include <vcc/input_buffer.h>
#include <vcc/internal/loader.h>
#include <vcc/instance.h>
#include <vcc/memory.h>
#include <vcc/physical_device.h>
#include <vcc/pipeline.h>
#include <vcc/pipeline_cache.h>
#include <vcc/pipeline_layout.h>
#include <vcc/queue.h>
#include <vcc/render_pass.h>
#include <vcc/sampler.h>
#include <vcc/semaphore.h>
#include <vcc/shader_module.h>
#include <vr.h>

template<typename KeyT, typename ValueT>
struct thread_safe_map_with_callback {
private:
	typedef std::unordered_map<KeyT, ValueT> map_type;
public:
	typedef typename map_type::iterator iterator;
	typedef typename map_type::value_type value_type;
	typedef std::function<void(iterator &&, iterator &&)> callback_type;

	explicit thread_safe_map_with_callback(callback_type &&callback)
		: callback(std::forward<callback_type>(callback)) {}

	void put(const KeyT &key, ValueT &&value) {
		{
			std::lock_guard<std::mutex> lock(map_mutex);
			map.emplace(key, std::forward<ValueT>(value));
			callback(map.begin(), map.end());
		}
	}

	void erase(const KeyT &key) {
		{
			std::lock_guard<std::mutex> lock(map_mutex);
			map.erase(key);
			callback(map.begin(), map.end());
		}
	}

	map_type map;
	std::mutex map_mutex;
	callback_type callback;
};

typedef std::array<glm::mat4x3, vr::k_unMaxTrackedDeviceCount>
	device_poses_container_type;
typedef std::function<void(const glm::mat4 &,
	const device_poses_container_type &)> update_matrix_callback_type;

template<typename IteratorT>
update_matrix_callback_type recalculate_update_matrix_callback(
		const glm::mat4 mat4Projection[2], IteratorT begin, IteratorT end) {
	std::vector<update_matrix_callback_type> update_matrix_callbacks;
	std::transform(begin, end, std::back_inserter(update_matrix_callbacks),
		[mat4Projection](typename IteratorT::value_type &value) {
		std::shared_ptr<type::mat4_array> projection_modelview_matrix(
			value.second.projection_modelview_matrix);
		const uint32_t index(value.first);
		return [projection_modelview_matrix, mat4Projection, index](
			const glm::mat4 &mat,
			const device_poses_container_type &mat4DevicePose) {
			const glm::mat4 mvp(mat * glm::mat4(mat4DevicePose[index]));
			auto mutable_mvp(type::write(*projection_modelview_matrix));
			for (int i = 0; i < 2; ++i) {
				mutable_mvp[i] = mat4Projection[i] * mvp;
			}
		};
	});
	return std::bind([](
			std::vector<update_matrix_callback_type> &callbacks,
			const glm::mat4 &mat,
			const device_poses_container_type &mat4DevicePose) {
		for (auto &callback : callbacks) {
			callback(mat, mat4DevicePose);
		}
	}, std::move(update_matrix_callbacks), std::placeholders::_1,
		std::placeholders::_2);
}

struct model_type {
	const type::supplier<const vcc::input_buffer::input_buffer_type> vertex_buffer, index_buffer;
	type::supplier<const vcc::image_view::image_view_type> image_view;
	uint32_t indices_size;
};

model_type load_model(vcc::device::device_type &device, vr_model &&model) {
	const uint32_t indices_size((uint32_t)model.indices.size());
	vcc::input_buffer::input_buffer_type index_buffer(
		vcc::input_buffer::create<type::linear>(std::ref(device), 0,
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, {},
			std::move(model.indices)));
	// TODO(gardell): normals
	vcc::input_buffer::input_buffer_type vertex_buffer(
		vcc::input_buffer::create<type::interleaved_std140>(std::ref(device),
			0, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE,
			{}, std::move(model.vertices), std::move(model.texcoords)));
	vcc::memory::bind(std::ref(device), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		index_buffer, vertex_buffer);

	vcc::image_view::image_view_type image_view(vcc::image_view::create(
		std::move(model.image), { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }));

	return { std::move(vertex_buffer), std::move(index_buffer), std::move(image_view),
		indices_size };
}

struct instance_type {
	std::shared_ptr<model_type> model;
	std::shared_ptr<type::mat4_array> projection_modelview_matrix;
	vcc::command_buffer::command_buffer_type command_buffer;
};

instance_type load_instance(vcc::device::device_type &device,
		vcc::descriptor_pool::descriptor_pool_type &desc_pool,
		vcc::descriptor_set_layout::descriptor_set_layout_type &desc_layout,
		vcc::command_pool::command_pool_type &cmd_pool,
		vcc::render_pass::render_pass_type &render_pass,
		vcc::framebuffer::framebuffer_type &framebuffer,
		vcc::pipeline::pipeline_type &pipeline,
		vcc::pipeline_layout::pipeline_layout_type &pipeline_layout,
		VkExtent2D extent, const VkViewport viewports[2],
		const std::string &value, const std::shared_ptr<model_type> &model) {

	vcc::sampler::sampler_type sampler(vcc::sampler::create(
		std::ref(device), VK_FILTER_NEAREST, VK_FILTER_NEAREST,
		VK_SAMPLER_MIPMAP_MODE_NEAREST,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		0, VK_TRUE, 1, VK_FALSE, VK_COMPARE_OP_NEVER, 0, 0,
		VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE, VK_FALSE));

	std::shared_ptr<type::mat4_array> projection_modelview_matrix(
		std::make_shared<type::mat4_array>(2));

	vcc::input_buffer::input_buffer_type matrix_uniform_buffer(
		vcc::input_buffer::create<type::linear>(std::ref(device), 0,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, {},
			projection_modelview_matrix));
	vcc::memory::bind(std::ref(device), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		matrix_uniform_buffer);

	vcc::descriptor_set::descriptor_set_type desc_set(std::move(
		vcc::descriptor_set::create(std::ref(device),
			std::ref(desc_pool),
			{ std::ref(desc_layout) }).front()));
	vcc::descriptor_set::update(device,
		vcc::descriptor_set::write_buffer(desc_set, 0, 0,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			{ vcc::descriptor_set::buffer_info(std::move(matrix_uniform_buffer)) }),
		vcc::descriptor_set::write_image{ desc_set, 1, 0,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		{
			vcc::descriptor_set::image_info{ std::move(sampler), model->image_view,
				VK_IMAGE_LAYOUT_GENERAL }
		} });

	vcc::command_buffer::command_buffer_type command_buffer(
		std::move(vcc::command_buffer::allocate(std::ref(device),
			std::ref(cmd_pool), VK_COMMAND_BUFFER_LEVEL_SECONDARY, 1).front()));
	vcc::command::compile(
		vcc::command::build(std::ref(command_buffer),
			VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
			| VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
			std::ref(render_pass), 0, std::ref(framebuffer), VK_FALSE, 0, 0),
		vcc::command::bind_pipeline{
		VK_PIPELINE_BIND_POINT_GRAPHICS, std::ref(pipeline) },
		vcc::command::bind_descriptor_sets{ VK_PIPELINE_BIND_POINT_GRAPHICS,
			std::ref(pipeline_layout), 0, { std::move(desc_set) },{} },
		vcc::command::set_scissor{
		0,{ { { 0, 0 }, extent } } },
		vcc::command::bind_vertex_buffers(0, { std::ref(model->vertex_buffer) }, { 0, 0 }),
		vcc::command::bind_index_data_buffer(
			model->index_buffer, 0, VK_INDEX_TYPE_UINT16),
		vcc::command::set_viewport{ 0,{ viewports[0] } },
		vcc::command::draw_indexed{
		model->indices_size, 1, 0, 0, 0 },
		vcc::command::set_viewport{ 0,{ viewports[1] } },
		vcc::command::draw_indexed{
		model->indices_size, 1, 0, 0, 1 });

	return instance_type{ model, projection_modelview_matrix,
		std::move(command_buffer) };
};

vcc::command_buffer::command_buffer_type recalculate_command_buffer(
		vcc::device::device_type &device,
		vcc::render_pass::render_pass_type &render_pass,
		vcc::framebuffer::framebuffer_type &framebuffer,
		vcc::command_pool::command_pool_type &cmd_pool,
		VkExtent2D extent,
		const std::vector<type::supplier<
			const vcc::command_buffer::command_buffer_type >> &command_buffers) {

	vcc::command_buffer::command_buffer_type command_buffer(
		std::move(vcc::command_buffer::allocate(std::ref(device),
			std::ref(cmd_pool), VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1).front()));
	vcc::command::compile(vcc::command::build(std::ref(command_buffer), 0, VK_FALSE, 0, 0),
		vcc::command::render_pass(std::ref(render_pass),
			std::ref(framebuffer), VkRect2D{ { 0, 0 }, extent },
			{
				vcc::command::clear_color({ { .2f, .2f, .2f, .2f } }),
				vcc::command::clear_depth_stencil({ 1, 0 })
			},
			VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS,
			vcc::command::execute_commands{ command_buffers }
	));

	return std::move(command_buffer);
}

int main(int argc, char **argv) {

	hmd_type hmd;

	vcc::instance::instance_type instance;
	{
		std::set<std::string> extensions = {
			VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef WIN32
			VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#else
      VK_KHR_XCB_SURFACE_EXTENSION_NAME
#endif
		};
		auto vr_extensions(hmd.get_vulkan_instance_extensions_required());
		std::copy(std::begin(vr_extensions), std::end(vr_extensions),
			std::inserter(extensions, std::end(extensions)));
		assert(vcc::enumerate::contains_all(
			vcc::enumerate::instance_extension_properties(""),
			extensions));
		instance = vcc::instance::create({}, extensions);
	}

	vcc::device::device_type device;
	{
		const VkPhysicalDevice physical_device(
			vcc::physical_device::enumerate(instance).front());

		std::set<std::string> extensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
		auto vr_extensions(hmd.get_vulkan_device_extensions_required(physical_device));
		std::copy(std::begin(vr_extensions), std::end(vr_extensions),
			std::inserter(extensions, std::end(extensions)));

		assert(vcc::enumerate::contains_all(
			vcc::enumerate::device_extension_properties(physical_device, ""),
			extensions));

		device = vcc::device::create(physical_device,
		{ vcc::device::queue_create_info_type{
			vcc::physical_device::get_queue_family_properties_with_flag(
			vcc::physical_device::queue_famility_properties(physical_device),
				VK_QUEUE_GRAPHICS_BIT),
				{ 0 } }
		},
		{}, extensions, {});
	}

	vcc::descriptor_set_layout::descriptor_set_layout_type desc_layout(
		vcc::descriptor_set_layout::create(std::ref(device),
		{
			vcc::descriptor_set_layout::descriptor_set_layout_binding{ 0,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
			VK_SHADER_STAGE_VERTEX_BIT,{} },
			vcc::descriptor_set_layout::descriptor_set_layout_binding{ 1,
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
			VK_SHADER_STAGE_FRAGMENT_BIT,{} },
			vcc::descriptor_set_layout::descriptor_set_layout_binding{ 2,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
			VK_SHADER_STAGE_FRAGMENT_BIT,{} }
		}));

	vcc::pipeline_layout::pipeline_layout_type pipeline_layout(vcc::pipeline_layout::create(
		std::ref(device), { std::ref(desc_layout) }));

	vcc::descriptor_pool::descriptor_pool_type desc_pool(
		vcc::descriptor_pool::create(std::ref(device), 0, vr::k_unMaxTrackedDeviceCount - 1,
		{ { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 } }));

	vcc::queue::queue_type queue(vcc::queue::get_graphics_queue(
		std::ref(device)));

	vcc::render_pass::render_pass_type render_pass(vcc::render_pass::create(
		std::ref(device),
		{
			VkAttachmentDescription{ 0, VK_FORMAT_R8G8B8A8_UNORM,
				VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR,
				VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				VK_ATTACHMENT_STORE_OP_DONT_CARE,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
			VkAttachmentDescription{ 0, VK_FORMAT_D16_UNORM,
			VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL }
		},
		{
			vcc::render_pass::subpass_description_type{
				{},{ { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL } },{},
				{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL },{}
			}
		}, {}));

	vcc::shader_module::shader_module_type vert_shader_module(
		vcc::shader_module::create(std::ref(device),
			std::ifstream("openvr-vert.spv",
				std::ios_base::binary | std::ios_base::in)
			));
	vcc::shader_module::shader_module_type frag_shader_module(
		vcc::shader_module::create(std::ref(device),
			std::ifstream("openvr-frag.spv",
				std::ios_base::binary | std::ios_base::in)
			));

	vcc::pipeline_cache::pipeline_cache_type pipeline_cache(
		vcc::pipeline_cache::create(std::ref(device)));
	vcc::pipeline::pipeline_type pipeline(
		vcc::pipeline::create_graphics(std::ref(device), pipeline_cache, 0,
		{
			vcc::pipeline::shader_stage(VK_SHADER_STAGE_VERTEX_BIT,
			std::ref(vert_shader_module), "main"),
			vcc::pipeline::shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT,
				std::ref(frag_shader_module), "main")
		},
		vcc::pipeline::vertex_input_state{
			{
				VkVertexInputBindingDescription{ 0, sizeof(glm::vec4) * 2,
				VK_VERTEX_INPUT_RATE_VERTEX }
			},
			{
				VkVertexInputAttributeDescription{ 0, 0,
					VK_FORMAT_R32G32B32_SFLOAT, 0 },
				VkVertexInputAttributeDescription{ 1, 0,
				VK_FORMAT_R32G32_SFLOAT, sizeof(glm::vec4) }
			}
		},
		vcc::pipeline::input_assembly_state
			{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE },
		vcc::pipeline::viewport_state(1, 1),
		vcc::pipeline::rasterization_state{ VK_FALSE, VK_FALSE,
		VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE,
		VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE, 0, 0, 0, 0 },
		vcc::pipeline::multisample_state{ VK_SAMPLE_COUNT_1_BIT,
		VK_FALSE, 0,{}, VK_FALSE, VK_FALSE },
		vcc::pipeline::depth_stencil_state{
			VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL, VK_FALSE, VK_FALSE,
			{ VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP,
				VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0 },
			{ VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP,
				VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0 },
			0, 0 },
		vcc::pipeline::color_blend_state{ VK_FALSE, VK_LOGIC_OP_CLEAR,
		{ VkPipelineColorBlendAttachmentState{ VK_FALSE, VK_BLEND_FACTOR_ZERO,
		VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
		VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO,
		VK_BLEND_OP_ADD, 0xF } },
		{ 0, 0, 0, 0 } },
		vcc::pipeline::dynamic_state{ { VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR } },
		std::ref(pipeline_layout), std::ref(render_pass), 0));

	vcc::command_pool::command_pool_type cmd_pool(vcc::command_pool::create(
		std::ref(device), 0, vcc::queue::get_family_index(queue)));

	const glm::vec2 angle(1, 0);

	vr_type vr_instance(std::move(hmd), std::ref(instance), std::ref(queue));
	const VkExtent2D extent(vr_instance.get_recommended_render_target_size());

	const float near_z(.1f), far_z(100);

	auto depth_image(std::make_shared<vcc::image::image_type>(
		vcc::image::create(
			std::ref(device), 0, VK_IMAGE_TYPE_2D, VK_FORMAT_D16_UNORM,
			{ extent.width, extent.height, 1 }, 1, 1,
			VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			VK_SHARING_MODE_EXCLUSIVE, {}, VK_IMAGE_LAYOUT_UNDEFINED)));
	vcc::memory::bind(std::ref(device),
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, *depth_image);

	{
		vcc::command_buffer::command_buffer_type command_buffer(
			std::move(vcc::command_buffer::allocate(std::ref(device),
				std::ref(cmd_pool), VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1).front()));
		vcc::command::compile(vcc::command::build(std::ref(command_buffer), 0, VK_FALSE, 0, 0),
			vcc::command::pipeline_barrier(
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, {}, {},
				{ vcc::command::image_memory_barrier{ 0,
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
				depth_image,{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 } } }));

		vcc::queue::submit(queue, {}, { command_buffer }, {});
	}
	vcc::queue::wait_idle(queue);

	vcc::framebuffer::framebuffer_type framebuffer(vcc::framebuffer::create(
		std::ref(device), std::ref(render_pass),
		{
			std::ref(vr_instance.get_image_view()),
			vcc::image_view::create(depth_image, { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 })
		}, extent, 1));

	const glm::mat4 mat4Projection[] = {
		vr_instance.get_projection_matrix(vr::Eye_Left, near_z, far_z)
		* vr_instance.get_head_to_eye_transform(vr::Eye_Left),
		vr_instance.get_projection_matrix(vr::Eye_Right, near_z, far_z)
		* vr_instance.get_head_to_eye_transform(vr::Eye_Right) };

	async_cache_type<std::string, model_type, thread_pool_type> model_cache(
			thread_pool_type(4), [&vr_instance, &device](
				const std::string &model_name) {
		return load_model(device, vr_model(vr_instance.load_model(model_name.c_str())));
	});

	std::shared_ptr<vcc::command_buffer::command_buffer_type> shared_command_buffer;
	update_matrix_callback_type update_matrix_callback([](const glm::mat4 &,
		const std::array<glm::mat4x3, vr::k_unMaxTrackedDeviceCount> &) {});

	typedef thread_safe_map_with_callback<uint32_t, instance_type>
		instance_map_type;
	auto callback = [&](const instance_map_type::iterator &begin,
		const instance_map_type::iterator &end) {
		std::vector<type::supplier<const vcc::command_buffer::command_buffer_type>>
			command_buffers;
		command_buffers.reserve(std::distance(begin, end));
		std::transform(begin, end, std::back_inserter(command_buffers),
			[](instance_map_type::value_type &value) {
			return std::ref(value.second.command_buffer);
		});
		shared_command_buffer =
			std::make_shared<vcc::command_buffer::command_buffer_type>(
				recalculate_command_buffer(device, render_pass, framebuffer,
					cmd_pool, extent, command_buffers));
		update_matrix_callback = recalculate_update_matrix_callback(
			mat4Projection, begin, end);

	};
	instance_map_type instances(callback);

	const VkViewport viewports[] = {
		vr_instance.get_viewport(0, 0, 1), vr_instance.get_viewport(1, 0, 1) };
	const auto track_device_callback = [&](
			uint32_t unTrackedDevice) {
		std::string model_name(
			vr_instance.get_string_tracked_device_property(unTrackedDevice,
				vr::Prop_RenderModelName_String));
		model_cache.put(std::move(model_name), [&, unTrackedDevice](
				const std::string &value,
				std::shared_ptr<model_type> &&model) {
			instances.put(unTrackedDevice, load_instance(device, desc_pool,
				desc_layout, cmd_pool, render_pass, framebuffer, pipeline,
				pipeline_layout, extent, viewports, value, model));
		});
	};

	for (uint32_t unTrackedDevice = vr::k_unTrackedDeviceIndex_Hmd + 1;
			unTrackedDevice < vr::k_unMaxTrackedDeviceCount;
			unTrackedDevice++) {
		if (vr_instance.hmd.get_hmd().IsTrackedDeviceConnected(unTrackedDevice)) {
			track_device_callback(unTrackedDevice);
		}
	}

	return vr_instance.run([&](const device_poses_container_type &mat4DevicePose) {
			const glm::mat4 hmd_matrix(glm::inverse(glm::mat4(
				mat4DevicePose[vr::k_unTrackedDeviceIndex_Hmd])));
			update_matrix_callback(hmd_matrix, mat4DevicePose);
			std::shared_ptr<const vcc::command_buffer::command_buffer_type>
				command_buffer(shared_command_buffer);
			if (command_buffer) {
				vcc::queue::submit(queue, {}, { *command_buffer }, {});
			}
		},
		[&](const vr::VREvent_t &event) {
			VCC_PRINT("event: %s", vr_instance.hmd.get_hmd().GetEventTypeNameFromEnum(
				vr::EVREventType(event.eventType)));
			switch (event.eventType) {
			case vr::VREvent_TrackedDeviceActivated:
				track_device_callback(event.trackedDeviceIndex);
				break;
			case vr::VREvent_TrackedDeviceDeactivated:
				instances.erase(event.trackedDeviceIndex);
				break;
			}
		});
}