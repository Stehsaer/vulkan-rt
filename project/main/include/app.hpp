#pragma once

#include "argument.hpp"
#include "frame-objects.hpp"
#include "model.hpp"
#include "pipeline.hpp"
#include "scene/camera.hpp"
#include "vulkan/context.hpp"
#include "vulkan/util/cycle.hpp"
#include "vulkan/util/extent-tracker.hpp"
#include <vulkan/vulkan_structs.hpp>

class App
{
  public:

	[[nodiscard]]
	static App create(const Argument& argument);

	void draw_frame();

	~App() noexcept;

  private:

	vulkan::Context context;
	vk::raii::CommandPool command_pool;
	vulkan::util::Cycle<vk::raii::CommandBuffer> command_buffers;

	ObjectRenderPipeline pipeline;
	ModelBuffer model_buffer;

	vulkan::util::ExtentTracker extent_tracker;
	vulkan::util::Cycle<FrameSyncPrimitive> sync_primitives;
	vulkan::util::Cycle<FrameRenderResource> render_resources;

	scene::camera::CenterView view{
		.center_position = {0.0, 0.0, 0.0},
		.distance = 3.0,
		.pitch_degrees = 30.0,
		.yaw_degrees = 45.0
	};
	scene::camera::PerspectiveProjection projection{.fov_degrees = 50.0, .near = 0.01, .far = 100.0};

	explicit App(
		vulkan::Context context,
		vk::raii::CommandPool command_pool,
		vulkan::util::Cycle<vk::raii::CommandBuffer> command_buffers,
		ObjectRenderPipeline pipeline,
		ModelBuffer model_buffer,
		vulkan::util::Cycle<FrameSyncPrimitive> sync_primitives,
		vulkan::util::Cycle<FrameRenderResource> render_resources
	) noexcept :
		context(std::move(context)),
		command_pool(std::move(command_pool)),
		command_buffers(std::move(command_buffers)),
		pipeline(std::move(pipeline)),
		model_buffer(std::move(model_buffer)),
		sync_primitives(std::move(sync_primitives)),
		render_resources(std::move(render_resources))
	{}

	struct FramePrepareResult
	{
		const vk::raii::CommandBuffer& command_buffer;
		const FrameSyncPrimitive& sync;
		const FrameRenderResource& frame;
		vulkan::SwapchainAcquireResult swapchain;
	};

	struct FrameSceneInfo
	{
		glm::mat4 view_projection;
	};

	FramePrepareResult prepare_frame();

	FrameSceneInfo update_scene_info(glm::u32vec2 swapchain_extent);

  public:

	App(const App&) = delete;
	App(App&&) = default;
	App& operator=(const App&) = delete;
	App& operator=(App&&) = default;
};