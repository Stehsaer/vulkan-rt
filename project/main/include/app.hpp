#pragma once

#include "argument.hpp"
#include "model.hpp"
#include "pipeline.hpp"
#include "resource.hpp"
#include "scene/camera.hpp"

#include "vulkan/context/device.hpp"
#include "vulkan/context/imgui.hpp"
#include "vulkan/context/instance.hpp"
#include "vulkan/context/swapchain.hpp"
#include "vulkan/util/cycle.hpp"
#include <vulkan/vulkan_structs.hpp>

class App
{
  public:

	[[nodiscard]]
	static App create(const Argument& argument);

	///
	/// @brief Run one frame of the app
	///
	/// @retval true Keep running
	/// @retval false Quit app
	///
	[[nodiscard]]
	bool draw_frame();

	~App() noexcept;

  private:

	vulkan::InstanceContext instance_context;
	vulkan::DeviceContext device_context;
	vulkan::SwapchainContext swapchain_context;
	vulkan::ImGuiContext imgui_context;

	vk::raii::CommandPool command_pool;
	vulkan::Cycle<vk::raii::CommandBuffer> command_buffers;

	ModelBuffer model_buffer;

	resource::Layout layout;
	ObjectRenderPipeline pipeline;

	resource::DescriptorPool descriptor_pool;
	vulkan::Cycle<resource::FrameDescriptorSet> frame_descriptor_sets;
	vulkan::Cycle<resource::FrameResource> frame_resources;
	vulkan::Cycle<resource::SyncPrimitive> sync_primitives;

	scene::camera::CenterView view{
		.center_position = {0.0, 0.0, 0.0},
		.distance = 3.0,
		.pitch_degrees = 30.0,
		.yaw_degrees = 45.0
	};
	scene::camera::PerspectiveProjection projection{.fov_degrees = 50.0, .near = 0.01, .far = 100.0};

	explicit App(
		vulkan::InstanceContext instance_context,
		vulkan::DeviceContext device_context,
		vulkan::SwapchainContext swapchain_context,
		vulkan::ImGuiContext imgui_context,
		vk::raii::CommandPool command_pool,
		vulkan::Cycle<vk::raii::CommandBuffer> command_buffers,
		ModelBuffer model_buffer,
		resource::Layout resource_layout,
		ObjectRenderPipeline pipeline,
		resource::DescriptorPool descriptor_pool,
		vulkan::Cycle<resource::FrameDescriptorSet> frame_descriptor_sets,
		vulkan::Cycle<resource::SyncPrimitive> sync_primitives
	) noexcept :
		instance_context(std::move(instance_context)),
		device_context(std::move(device_context)),
		swapchain_context(std::move(swapchain_context)),
		imgui_context(std::move(imgui_context)),
		command_pool(std::move(command_pool)),
		command_buffers(std::move(command_buffers)),
		model_buffer(std::move(model_buffer)),
		layout(std::move(resource_layout)),
		pipeline(std::move(pipeline)),
		descriptor_pool(std::move(descriptor_pool)),
		frame_descriptor_sets(std::move(frame_descriptor_sets)),
		frame_resources(std::nullopt),  // Not ready yet
		sync_primitives(std::move(sync_primitives))
	{}

	struct FramePrepareResult
	{
		const vk::raii::CommandBuffer& command_buffer;
		const resource::SyncPrimitive& sync;
		const resource::FrameResource& resource;
		const resource::FrameDescriptorSet& descriptor_set;
		vulkan::SwapchainContext::Frame swapchain;
	};

	struct FrameSceneInfo
	{
		glm::mat4 view_projection;
		glm::vec3 view_pos;
	};

	void draw_ui();

	FramePrepareResult prepare_frame();

	FrameSceneInfo update_scene_info(glm::u32vec2 swapchain_extent);

  public:

	App(const App&) = delete;
	App(App&&) = default;
	App& operator=(const App&) = delete;
	App& operator=(App&&) = default;
};
