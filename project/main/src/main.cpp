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
		auto fence_result =
			vulkan.device.createFence({.flags = vk::FenceCreateFlagBits::eSignaled})
				.transform_error(Error::from<vk::Result>());
		if (!fence_result) return fence_result.error().forward("Create draw fence failed");
		auto fence = std::move(*fence_result);

		auto render_finished_semaphore_result =
			vulkan.device.createSemaphore({}).transform_error(Error::from<vk::Result>());
		if (!render_finished_semaphore_result)
			return render_finished_semaphore_result.error().forward(
				"Create render finished semaphore failed"
			);
		auto render_finished_semaphore = std::move(*render_finished_semaphore_result);

		auto present_complete_semaphore_result =
			vulkan.device.createSemaphore({}).transform_error(Error::from<vk::Result>());
		if (!present_complete_semaphore_result)
			return present_complete_semaphore_result.error().forward(
				"Create present complete semaphore failed"
			);
		auto present_complete_semaphore = std::move(*present_complete_semaphore_result);

		return PerFrameObject{
			.command_buffer = std::move(command_buffer),
			.draw_fence = std::move(fence),
			.render_finished_semaphore = std::move(render_finished_semaphore),
			.image_available_semaphore = std::move(present_complete_semaphore)
		};
	}
};

int main() noexcept
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

		auto vulkan = vulkan::Context::create(create_info) | Error::unwrap("Create context failed");

		/* Resources */

		auto descriptor_set_layout =
			DescriptorLayouts::create(vulkan.device) | Error::unwrap("Create descriptor set layout failed");

		auto resources = Resources::create(vulkan, descriptor_set_layout.main_layout)
			| Error::unwrap("Create resources failed");

		/* Pipeline */

		auto pipeline = Pipeline::create(vulkan, descriptor_set_layout.main_layout)
			| Error::unwrap("Create pipeline failed");

		/* Command Pool & Buffer */

		auto command_pool =
			vulkan.device
				.createCommandPool(
					{.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
					 .queueFamilyIndex = vulkan.queues.graphics_index}
				)
				.transform_error(Error::from<vk::Result>())
			| Error::unwrap("Create command pool failed");

		auto command_buffers =
			vulkan.device
				.allocateCommandBuffers({
					.commandPool = *command_pool,
					.level = vk::CommandBufferLevel::ePrimary,
					.commandBufferCount = 3,
				})
				.transform_error(Error::from<vk::Result>())
			| Error::unwrap("Allocate command buffers failed");

		/* Render Objects */

		auto per_frame_objects =
			command_buffers
			| std::views::transform([&vulkan](auto& command_buffer) {
				  return PerFrameObject::create(vulkan, std::move(command_buffer));
			  })
			| std::ranges::to<std::vector>()
			| Error::collect_vec()
			| Error::unwrap("Create frame objects failed")
			| vulkan::util::Cycle<PerFrameObject>::into;

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
				std::expected<void, Error>(Error("Wait for draw fence failed", vk::to_string(wait_result)))
					| Error::unwrap("Wait for draw fence failed");
			const auto& command_buffer = current_frame_object.command_buffer;

			/* Acquire Swapchain */

			const auto [extent, index, swapchain_image] =
				vulkan.acquire_swapchain_image(current_frame_object.image_available_semaphore)
				| Error::unwrap("Acquire swapchain image failed");

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
				std::to_array({*current_frame_object.image_available_semaphore});
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

			vulkan.present_swapchain_image(index, current_frame_object.render_finished_semaphore)
				| Error::unwrap("Present swapchain image failed");
		}

		vulkan.device.waitIdle();

		return 0;
	}
	catch (const Error& error)
	{
		try
		{
			std::println(std::cerr, "Error occurred: {:msg}", error);
			for (const auto& [idx, entry] : std::views::enumerate(error.chain()))
				std::println(std::cerr, "[#{}]: {}", idx, entry);
		}
		catch (const std::exception& err)
		{
			std::cerr
				<< "Error occurred: an error occurred, but formatting the error has failed: "
				<< err.what()
				<< '\n';
		}

		return 1;
	}
}
