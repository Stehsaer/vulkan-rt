#include "app.hpp"
#include "frame-objects.hpp"
#include "pipeline.hpp"
#include "vulkan/context/imgui.hpp"
#include "vulkan/context/instance.hpp"
#include "vulkan/context/swapchain.hpp"
#include "vulkan/util/constants.hpp"
#include "vulkan/util/image-barrier.hpp"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>
#include <imgui.h>
#include <ranges>
#include <vulkan/vulkan_raii.hpp>

App App::create(const Argument& argument)
{
	const auto instance_context_config = vulkan::InstanceContext::Config{};
	auto instance_context = vulkan::InstanceContext::create(instance_context_config)
		| Error::unwrap("Create instance context failed");

	const auto device_context_config = vulkan::DeviceContext::Config{};
	auto device_context = vulkan::DeviceContext::create(instance_context, device_context_config)
		| Error::unwrap("Create device context failed");

	const auto swapchain_context_config =
		vulkan::SwapchainContext::Config{.format = vulkan::SwapchainContext::Format::Linear_8bit};
	auto swapchain_context =
		vulkan::SwapchainContext::create(instance_context, device_context, swapchain_context_config)
		| Error::unwrap("Create swapchain context failed");

	const auto attachment_formats = std::to_array({swapchain_context->surface_format.format});
	const auto pipeline_rendering_info =
		vk::PipelineRenderingCreateInfo()
			.setColorAttachmentFormats(attachment_formats)
			.setDepthAttachmentFormat(vk::Format::eD32Sfloat);

	const auto render_scheme =
		vulkan::ImGuiContext::Config::DynamicRendering{.rendering_info = pipeline_rendering_info};
	auto imgui_context =
		vulkan::ImGuiContext::create(instance_context, device_context, {.render_scheme = render_scheme})
		| Error::unwrap("Create ImGui context failed");

	auto command_pool =
		device_context.device
			.createCommandPool(
				{.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
				 .queueFamilyIndex = device_context.graphics_queue.family_index}
			)
			.transform_error(Error::from<vk::Result>())
		| Error::unwrap("Create command pool failed");
	auto command_buffers =
		device_context.device
			.allocateCommandBuffers({
				.commandPool = *command_pool,
				.level = vk::CommandBufferLevel::ePrimary,
				.commandBufferCount = 3,
			})
			.transform_error(Error::from<vk::Result>())
		| Error::unwrap("Allocate command buffers failed")
		| vulkan::util::Cycle<vk::raii::CommandBuffer>::into;

	auto pipeline = ObjectRenderPipeline::create(device_context, pipeline_rendering_info);
	auto model = Model::load_from_file(argument.model_path) | Error::unwrap("Load model failed");
	auto model_buffer = ModelBuffer::create(device_context, model);

	auto frame_sync_primitives =
		std::views::iota(0zu, 3zu)
		| std::views::transform([&device_context](size_t) {
			  return FrameSyncPrimitive::create(device_context);
		  })
		| std::ranges::to<std::vector>()
		| vulkan::util::Cycle<FrameSyncPrimitive>::into;

	auto render_resources =
		std::views::iota(0zu, 3zu)
		| std::views::transform([&device_context, &instance_context_config](size_t) {
			  return FrameRenderResource::create(device_context, instance_context_config.initial_size);
		  })
		| std::ranges::to<std::vector>()
		| vulkan::util::Cycle<FrameRenderResource>::into;

	return App(
		std::move(instance_context),
		std::move(device_context),
		std::move(swapchain_context),
		std::move(imgui_context),
		std::move(command_pool),
		std::move(command_buffers),
		std::move(pipeline),
		std::move(model_buffer),
		std::move(frame_sync_primitives),
		std::move(render_resources)
	);
}

App::FramePrepareResult App::prepare_frame()
{
	command_buffers.cycle();
	sync_primitives.cycle();

	/* Wait for command buffer */

	if (const auto wait_result = device_context.device.waitForFences(
			{sync_primitives.current().draw_fence},
			vk::True,
			std::numeric_limits<uint64_t>::max()
		);
		wait_result != vk::Result::eSuccess)
		throw Error::from(wait_result).forward("Wait for draw fence failed");

	/* Acquire swapchain image */

	auto swapchain_result =
		swapchain_context.acquire_next(
			instance_context,
			device_context,
			sync_primitives.current().image_available_semaphore
		)
		| Error::unwrap("Acquire swapchain image failed");

	if (swapchain_result.extent_changed)
	{
		device_context.device.waitIdle();

		render_resources =
			std::views::iota(0zu, 3zu)
			| std::views::transform([this, &swapchain_result](size_t) {
				  return FrameRenderResource::create(device_context, swapchain_result.extent);
			  })
			| std::ranges::to<std::vector>()
			| vulkan::util::Cycle<FrameRenderResource>::into;
	}
	else
	{
		render_resources.cycle();
	}

	/* Calculate camera matrix */

	return FramePrepareResult{
		.command_buffer = command_buffers.current(),
		.sync = sync_primitives.current(),
		.frame = render_resources.current(),
		.swapchain = swapchain_result,
	};
}

App::FrameSceneInfo App::update_scene_info(glm::u32vec2 swapchain_extent)
{
	const auto& io = ImGui::GetIO();
	if (!io.WantCaptureMouse)
	{
		const auto mouse_delta = glm::vec2(io.MouseDelta.x, io.MouseDelta.y) / glm::vec2(swapchain_extent);
		const auto mouse_scroll = io.MouseWheel;

		if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) view = view.mouse_rotate(mouse_delta);

		if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
			view = view.mouse_pan(mouse_delta, swapchain_extent.x / double(swapchain_extent.y), 1.0);

		view = view.mouse_scroll(mouse_scroll);
	}

	const glm::mat4 camera_matrix = scene::camera::reverse_z(true)
		* projection.matrix(swapchain_extent.x / static_cast<float>(swapchain_extent.y))
		* view.matrix();

	return {.view_projection = camera_matrix};
}

bool App::draw_frame()
{
	bool quit = false;

	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		imgui_context.process_event(event);

		switch (event.type)
		{
		case SDL_EVENT_QUIT:
			quit = true;
			break;
		case SDL_EVENT_WINDOW_MINIMIZED:
			continue;
		default:
			break;
		}
	}

	const auto& [command_buffer, sync, frame, swapchain] = prepare_frame();

	imgui_context.new_frame() | Error::unwrap("Start new ImGui frame failed");
	draw_ui();
	const auto scene_info = update_scene_info(swapchain.extent);
	imgui_context.render() | Error::unwrap("Render ImGui frame failed");

	/* Actual Rendering */

	command_buffer.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
	{
		const auto depth_buffer_image_barrier = vk::ImageMemoryBarrier2{
			.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
			.srcAccessMask = {},
			.dstStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests,
			.dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			.oldLayout = vk::ImageLayout::eUndefined,
			.newLayout = vk::ImageLayout::eDepthAttachmentOptimal,
			.image = frame.depth_buffer,
			.subresourceRange = vulkan::util::constant::subres::depth_only_attachment
		};
		const auto acquire_image_barriers = std::to_array(
			{vulkan::util::image_barrier::swapchain_acquire(swapchain.image), depth_buffer_image_barrier}
		);
		command_buffer.pipelineBarrier2(vk::DependencyInfo{}.setImageMemoryBarriers(acquire_image_barriers));

		const auto viewport = vk::Viewport{
			.x = 0.0f,
			.y = 0.0f,
			.width = static_cast<float>(swapchain.extent.x),
			.height = static_cast<float>(swapchain.extent.y),
			.minDepth = 0.0f,
			.maxDepth = 1.0f,
		};
		const auto scissor = vk::Rect2D{
			.offset = vk::Offset2D{.x = 0,                      .y = 0                      },
			.extent = vk::Extent2D{.width = swapchain.extent.x, .height = swapchain.extent.y},
		};

		const auto swapchain_attachment_info = vk::RenderingAttachmentInfo{
			.imageView = swapchain.image_view,
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f),
		};
		const auto depth_attachment_info = vk::RenderingAttachmentInfo{
			.imageView = frame.depth_buffer_view,
			.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eDontCare,
			.clearValue = vk::ClearDepthStencilValue{.depth = 0.0f, .stencil = 0},
		};

		const auto attachment_info_list = std::to_array({swapchain_attachment_info});
		const auto rendering_info =
			vk::RenderingInfo()
				.setRenderArea(scissor)
				.setLayerCount(1)
				.setColorAttachments(attachment_info_list)
				.setPDepthAttachment(&depth_attachment_info);

		const auto vertex_buffer_lists = std::to_array<vk::Buffer>({model_buffer.vertex_buffer});
		const auto vertex_buffer_offsets = std::to_array<vk::DeviceSize>({0});

		const auto push_constant =
			ObjectRenderPipeline::PushConstant{.view_projection = scene_info.view_projection};

		command_buffer.beginRendering(rendering_info);
		{
			command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline.pipeline);
			command_buffer.setViewport(0, {viewport});
			command_buffer.setScissor(0, {scissor});
			command_buffer.bindVertexBuffers(0, vertex_buffer_lists, vertex_buffer_offsets);
			command_buffer.bindIndexBuffer(model_buffer.index_buffer, 0, vk::IndexType::eUint32);
			command_buffer.pushConstants(
				pipeline.layout,
				vk::ShaderStageFlagBits::eVertex,
				0,
				vk::ArrayProxy<const ObjectRenderPipeline::PushConstant>(push_constant)
			);
			command_buffer.drawIndexed(model_buffer.vertex_count, 1, 0, 0, 0);

			imgui_context.draw(command_buffer) | Error::unwrap("Draw ImGui failed");
		}
		command_buffer.endRendering();

		const auto present_image_barriers =
			std::to_array({vulkan::util::image_barrier::swapchain_present(swapchain.image)});
		command_buffer.pipelineBarrier2(vk::DependencyInfo{}.setImageMemoryBarriers(present_image_barriers));
	}
	command_buffer.end();

	/* Submit */

	{
		const auto wait_semaphores = std::to_array<vk::Semaphore>({sync.image_available_semaphore});
		const auto signal_semaphores = std::to_array<vk::Semaphore>({sync.render_finished_semaphore});
		const auto submit_buffers = std::to_array({*command_buffer});
		const auto wait_stages =
			std::to_array<vk::PipelineStageFlags>({vk::PipelineStageFlagBits::eColorAttachmentOutput});
		const auto graphic_submit_info =
			vk::SubmitInfo{}
				.setCommandBuffers(submit_buffers)
				.setWaitSemaphores(wait_semaphores)
				.setSignalSemaphores(signal_semaphores)
				.setWaitDstStageMask(wait_stages);

		device_context.device.resetFences({*sync.draw_fence});
		device_context.graphics_queue.queue->submit(graphic_submit_info, *sync.draw_fence);
	}

	/* Present */

	swapchain_context.present(device_context, swapchain, sync.render_finished_semaphore)
		| Error::unwrap("Present swapchain image failed");

	return !quit;
}

void App::draw_ui()
{
	ImGui::ShowDemoWindow();

	if (ImGui::Begin("Info"))
	{
		ImGui::Text(
			"Center: (%.2f, %.2f, %.2f)",
			view.center_position.x,
			view.center_position.y,
			view.center_position.z
		);
		ImGui::Text("Distance: %.2f", view.distance);
		ImGui::Text("Pitch: %.2f", view.pitch_degrees);
		ImGui::Text("Yaw: %.2f", view.yaw_degrees);

		ImGui::Separator();

		ImGui::Text("FPS: %.2f", ImGui::GetIO().Framerate);
	}
	ImGui::End();
}

App::~App() noexcept
{
	device_context.device.waitIdle();
}
