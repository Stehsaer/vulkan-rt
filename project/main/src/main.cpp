#include <SDL3/SDL_events.h>
#include <common/formatter/vec.hpp>
#include <common/formatter/vulkan.hpp>
#include <common/util/error.hpp>
#include <iostream>
#include <optional>
#include <print>
#include <ranges>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "descriptor-layouts.hpp"
#include "pipeline.hpp"
#include "resources.hpp"
#include "vulkan/context.hpp"
#include "vulkan/util/cycle.hpp"
#include "vulkan/util/image-barrier.hpp"

struct PerFrameObject
{
	vk::raii::CommandBuffer command_buffer;

	vk::raii::Fence draw_fence;
	vk::raii::Semaphore render_finished_semaphore;
	vk::raii::Semaphore image_available_semaphore;

	static std::expected<PerFrameObject, Error> create(
		const vulkan::Context& vulkan,
		vk::raii::CommandBuffer command_buffer
	) noexcept
	{
		auto fence_expected = vulkan.device.createFence({.flags = vk::FenceCreateFlagBits::eSignaled});
		if (!fence_expected) return Error("Create draw fence failed");

		auto render_finished_semaphore_expected = vulkan.device.createSemaphore({});
		if (!render_finished_semaphore_expected) return Error("Create render finished semaphore failed");

		auto present_complete_semaphore_expected = vulkan.device.createSemaphore({});
		if (!present_complete_semaphore_expected) return Error("Create present complete semaphore failed");

		return PerFrameObject{
			.command_buffer = std::move(command_buffer),
			.draw_fence = std::move(*fence_expected),
			.render_finished_semaphore = std::move(*render_finished_semaphore_expected),
			.image_available_semaphore = std::move(*present_complete_semaphore_expected)
		};
	}
};

int main()
{
	try
	{
		/* Context */

		const auto create_info = vulkan::Context::CreateInfo{
			.window_info = {.title = "Vulkan Gltf RT", .initial_size = {800, 600}},
			.app_info =
				{.application_name = "Vulkan Gltf RT", .application_version = VK_MAKE_VERSION(0, 1, 0)},
			.features = {.validation = true},
		};

		auto vulkan_expected = vulkan::Context::create(create_info);
		if (!vulkan_expected) vulkan_expected.error().throw_self("Create context failed");
		auto vulkan = std::move(*vulkan_expected);

		/* Resources */

		auto descriptor_set_layout_expected = DescriptorLayouts::create(vulkan.device);
		if (!descriptor_set_layout_expected)
			descriptor_set_layout_expected.error().throw_self("Create descriptor set layout failed");
		auto descriptor_set_layout = std::move(*descriptor_set_layout_expected);

		auto resources_expected = Resources::create(vulkan, descriptor_set_layout.main_layout);
		if (!resources_expected) resources_expected.error().throw_self("Create resources failed");
		auto resources = std::move(*resources_expected);

		/* Pipeline */

		auto pipeline_expected = Pipeline::create(vulkan, descriptor_set_layout.main_layout);
		if (!pipeline_expected) pipeline_expected.error().throw_self("Create pipeline failed");
		auto pipeline = std::move(*pipeline_expected);

		/* Command Pool & Buffer */

		auto command_pool_expected = vulkan.device.createCommandPool(
			{.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
			 .queueFamilyIndex = vulkan.queues.graphics_index}
		);
		if (!command_pool_expected)
			Error(command_pool_expected.error(), "Create command pool failed").throw_self();
		auto command_pool = std::move(*command_pool_expected);

		auto command_buffers_expected = vulkan.device.allocateCommandBuffers({
			.commandPool = *command_pool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = 3,
		});
		if (!command_buffers_expected)
			Error(command_buffers_expected.error(), "Allocate command buffers failed").throw_self();
		auto command_buffers = std::move(*command_buffers_expected);

		/* Render Objects */

		auto per_frame_objects_expected =
			command_buffers
			| std::views::transform([&vulkan](auto& command_buffer) {
				  return PerFrameObject::create(vulkan, std::move(command_buffer));
			  })
			| std::ranges::to<std::vector>()
			| Error::collect_vec("Create frame objects failed");
		if (!per_frame_objects_expected) per_frame_objects_expected.error().throw_self();
		auto per_frame_objects =
			vulkan::util::Cycle<PerFrameObject>::create(std::move(*per_frame_objects_expected));

		bool quit = false;
		while (!quit)
		{
			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
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

			vulkan.queues.graphics->waitIdle();

			per_frame_objects.cycle();
			const auto& current_frame_object = per_frame_objects.current();

			/* Wait For Fence */

			if (const auto wait_result = vulkan.device.waitForFences(
					{*current_frame_object.draw_fence},
					vk::True,
					std::numeric_limits<uint64_t>::max()
				);
				wait_result != vk::Result::eSuccess)
				Error(wait_result, "Wait for draw fence failed").throw_self();
			const auto& command_buffer = current_frame_object.command_buffer;

			/* Acquire Swapchain */

			const auto acquire_expected =
				vulkan.acquire_swapchain_image(per_frame_objects.prev().image_available_semaphore);
			if (!acquire_expected) acquire_expected.error().throw_self("Acquire swapchain image failed");
			const auto& [extent, index, swapchain_image] = *acquire_expected;

			/* Actual Rendering */

			command_buffer.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
			{
				const auto acquire_image_barriers =
					std::to_array({vulkan::util::image_barrier::swapchain_acquire(swapchain_image.image)});
				command_buffer.pipelineBarrier2(
					vk::DependencyInfo{}.setImageMemoryBarriers(acquire_image_barriers)
				);

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
					.imageView = swapchain_image.view,
					.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
					.loadOp = vk::AttachmentLoadOp::eClear,
					.storeOp = vk::AttachmentStoreOp::eStore,
					.clearValue = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f),
				};

				const auto attachment_info_list = std::to_array({swapchain_attachment_info});
				const auto rendering_info =
					vk::RenderingInfo{
						.renderArea = scissor,
						.layerCount = 1,
					}
						.setColorAttachments(attachment_info_list);

				const auto vertex_buffer_lists = std::to_array<vk::Buffer>({resources.vertex_buffer});
				const auto vertex_buffer_offsets = std::to_array<vk::DeviceSize>({0});

				command_buffer.beginRendering(rendering_info);
				{
					command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline.pipeline);
					command_buffer.setViewport(0, {viewport});
					command_buffer.setScissor(0, {scissor});
					command_buffer.bindVertexBuffers(0, vertex_buffer_lists, vertex_buffer_offsets);
					command_buffer.bindDescriptorSets(
						vk::PipelineBindPoint::eGraphics,
						*pipeline.layout,
						0,
						{*resources.main_descriptor_set},
						{}
					);
					command_buffer.draw(6, 1, 0, 0);
				}
				command_buffer.endRendering();

				const auto present_image_barriers =
					std::to_array({vulkan::util::image_barrier::swapchain_present(swapchain_image.image)});
				command_buffer.pipelineBarrier2(
					vk::DependencyInfo{}.setImageMemoryBarriers(present_image_barriers)
				);
			}
			command_buffer.end();

			/* Submit */

			const auto graphic_submit_wait_semaphores =
				std::to_array({*per_frame_objects.prev().image_available_semaphore});
			const auto graphic_submit_signal_semaphores =
				std::to_array({*current_frame_object.render_finished_semaphore});
			const auto graphic_submit_command_buffers = std::to_array({*command_buffer});
			const auto graphic_submit_wait_stages =
				std::to_array<vk::PipelineStageFlags>({vk::PipelineStageFlagBits::eColorAttachmentOutput});
			const auto graphic_submit_info =
				vk::SubmitInfo{}
					.setCommandBuffers(graphic_submit_command_buffers)
					.setWaitSemaphores(graphic_submit_wait_semaphores)
					.setSignalSemaphores(graphic_submit_signal_semaphores)
					.setWaitDstStageMask(graphic_submit_wait_stages);

			vulkan.device.resetFences({*current_frame_object.draw_fence});
			vulkan.queues.graphics->submit(graphic_submit_info, *current_frame_object.draw_fence);

			/* Present */

			const auto present_result =
				vulkan.present_swapchain_image(index, current_frame_object.render_finished_semaphore);
			if (!present_result) present_result.error().throw_self("Present swapchain image failed");
		}

		vulkan.device.waitIdle();

		return 0;
	}
	catch (const Error& error)
	{
		std::println(std::cerr, "Error occurred: {:msg}", error.origin());
		for (const auto& [idx, entry] : std::views::enumerate(error.trace))
			std::println(std::cerr, "[#{}]: {}", idx, entry);

		return 1;
	}
}
