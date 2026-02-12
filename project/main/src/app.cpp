#include "app.hpp"
#include "frame-objects.hpp"
#include "pipeline.hpp"
#include "vulkan/util/constants.hpp"
#include "vulkan/util/image-barrier.hpp"

#include <SDL3/SDL_mouse.h>
#include <ranges>
#include <vulkan/vulkan_raii.hpp>

App App::create(const Argument& argument)
{
	const auto create_info = vulkan::Context::CreateInfo{
		.window_info = {.title = "Context Gltf RT", .initial_size = {800, 600}},
		.app_info = {.application_name = "Context Gltf RT", .application_version = VK_MAKE_VERSION(0, 1, 0)},
		.features = {.validation = true},
	};

	auto context = vulkan::Context::create(create_info) | Error::unwrap("Create context failed");

	auto command_pool =
		context.device
			.createCommandPool(
				{.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
				 .queueFamilyIndex = context.queues.graphics_index}
			)
			.transform_error(Error::from<vk::Result>())
		| Error::unwrap("Create command pool failed");
	auto command_buffers =
		context.device
			.allocateCommandBuffers({
				.commandPool = *command_pool,
				.level = vk::CommandBufferLevel::ePrimary,
				.commandBufferCount = 3,
			})
			.transform_error(Error::from<vk::Result>())
		| Error::unwrap("Allocate command buffers failed")
		| vulkan::util::Cycle<vk::raii::CommandBuffer>::into;

	auto pipeline = ObjectRenderPipeline::create(context);
	auto model = Model::load_from_file(argument.model_path) | Error::unwrap("Load model failed");
	auto model_buffer = ModelBuffer::create(context, model);

	auto frame_sync_primitives = std::views::iota(0zu, 3zu)
		| std::views::transform([&context](size_t) { return FrameSyncPrimitive::create(context); })
		| std::ranges::to<std::vector>()
		| vulkan::util::Cycle<FrameSyncPrimitive>::into;

	auto render_resources =
		std::views::iota(0zu, 3zu)
		| std::views::transform([&context, &create_info](size_t) {
			  return FrameRenderResource::create(context, create_info.window_info.initial_size);
		  })
		| std::ranges::to<std::vector>()
		| vulkan::util::Cycle<FrameRenderResource>::into;

	return App(
		std::move(context),
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

	if (const auto wait_result = context.device.waitForFences(
			{sync_primitives.current().draw_fence},
			vk::True,
			std::numeric_limits<uint64_t>::max()
		);
		wait_result != vk::Result::eSuccess)
		throw Error::from(wait_result).forward("Wait for draw fence failed");

	/* Acquire swapchain image */

	auto swapchain_result =
		context.acquire_swapchain_image(sync_primitives.current().image_available_semaphore)
		| Error::unwrap("Acquire swapchain image failed");

	extent_tracker.update(swapchain_result.extent);
	if (extent_tracker.is_changed())
	{
		context.device.waitIdle();

		render_resources =
			std::views::iota(0zu, 3zu)
			| std::views::transform([this, &swapchain_result](size_t) {
				  return FrameRenderResource::create(context, swapchain_result.extent);
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
	glm::vec2 mouse_delta;
	const auto mouse_state = SDL_GetRelativeMouseState(&mouse_delta.x, &mouse_delta.y);

	if ((mouse_state & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT)) != 0)
		view = view.mouse_rotate(mouse_delta / glm::vec2(swapchain_extent));
	if ((mouse_state & SDL_BUTTON_MASK(SDL_BUTTON_MIDDLE)) != 0)
		view = view.mouse_scroll(mouse_delta.y / swapchain_extent.y, 3.0);
	if ((mouse_state & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) != 0)
		view = view.mouse_pan(
			mouse_delta / glm::vec2(swapchain_extent),
			swapchain_extent.x / double(swapchain_extent.y),
			1.0
		);

	const glm::mat4 camera_matrix = scene::camera::reverse_z(true)
		* projection.matrix(swapchain_extent.x / static_cast<float>(swapchain_extent.y))
		* view.matrix();

	return {.view_projection = camera_matrix};
}

void App::draw_frame()
{
	const auto& [command_buffer, sync, frame, swapchain] = prepare_frame();
	const auto& [extent, index, swapchain_image, swapchain_imageview] = swapchain;

	const auto scene_info = update_scene_info(extent);

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
			{vulkan::util::image_barrier::swapchain_acquire(swapchain_image), depth_buffer_image_barrier}
		);
		command_buffer.pipelineBarrier2(vk::DependencyInfo{}.setImageMemoryBarriers(acquire_image_barriers));

		const auto viewport = vk::Viewport{
			.x = 0.0f,
			.y = 0.0f,
			.width = static_cast<float>(extent.x),
			.height = static_cast<float>(extent.y),
			.minDepth = 0.0f,
			.maxDepth = 1.0f,
		};
		const auto scissor = vk::Rect2D{
			.offset = vk::Offset2D{.x = 0,            .y = 0            },
			.extent = vk::Extent2D{.width = extent.x, .height = extent.y},
		};

		const auto swapchain_attachment_info = vk::RenderingAttachmentInfo{
			.imageView = swapchain_imageview,
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
		}
		command_buffer.endRendering();

		const auto present_image_barriers =
			std::to_array({vulkan::util::image_barrier::swapchain_present(swapchain_image)});
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

		context.device.resetFences({*sync.draw_fence});
		context.queues.graphics->submit(graphic_submit_info, *sync.draw_fence);
	}

	/* Present */

	context.present_swapchain_image(index, sync.render_finished_semaphore)
		| Error::unwrap("Present swapchain image failed");
}

App::~App() noexcept
{
	context.device.waitIdle();
}
