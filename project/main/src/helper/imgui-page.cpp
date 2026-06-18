#include "helper/imgui-page.hpp"
#include "common/number-literals.hpp"
#include "common/util/error.hpp"
#include "config.hpp"
#include "resource/context.hpp"
#include "resource/sync-primitive.hpp"
#include "vulkan/container/host/cycle.hpp"
#include "vulkan/context/swapchain.hpp"
#include "vulkan/interface/context.hpp"
#include "vulkan/numeric/base-level.hpp"

#include <SDL3/SDL_events.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <mutex>
#include <optional>
#include <ranges>
#include <utility>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace helper
{
	std::expected<ImGuiPage, Error> ImGuiPage::create(
		const vulkan::Context& context,
		size_t swapchain_image_count
	) noexcept
	{
		auto command_pool_result = context.device.createCommandPool(
			vk::CommandPoolCreateInfo{
				.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
				.queueFamilyIndex = context.family,
			}
		);
		if (!command_pool_result) return Error::from(command_pool_result);
		auto command_pool = std::move(*command_pool_result);

		auto command_buffer_result = context.device.allocateCommandBuffers({
			.commandPool = command_pool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = config::INFLIGHT_FRAMES,
		});
		if (!command_buffer_result) return Error::from(command_buffer_result);
		auto command_buffers =
			std::move(*command_buffer_result) | vulkan::Cycle<vk::raii::CommandBuffer>::into;

		auto sync_primitives_result = std::views::iota(0_u32, config::INFLIGHT_FRAMES)
			| std::views::transform([&](auto) { return resource::FrameSyncPrimitive::create(context); })
			| Error::collect();
		if (!sync_primitives_result)
			return sync_primitives_result.error().forward("Create frame sync primitives failed");
		auto sync_primitives =
			std::move(*sync_primitives_result) | vulkan::Cycle<resource::FrameSyncPrimitive>::into;

		auto render_complete_semaphores_result =
			std::views::repeat(
				[&context] { return context.device.createSemaphore({}); },
				swapchain_image_count
			)
			| std::views::transform([](const auto& lambda) {
				  return lambda().transform_error([](vk::Result result) { return Error::from(result); });
			  })
			| Error::collect();
		if (!render_complete_semaphores_result)
			return render_complete_semaphores_result.error().forward(
				"Create swapchain image semaphores failed"
			);
		auto render_complete_semaphores = std::move(*render_complete_semaphores_result);

		return ImGuiPage(
			std::move(command_pool),
			std::move(command_buffers),
			std::move(sync_primitives),
			std::move(render_complete_semaphores)
		);
	}

	bool ImGuiPage::poll_events(resource::Context& context) const noexcept
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			context.imgui.process_event(event);

			switch (event.type)
			{
			case SDL_EVENT_QUIT:
				return false;
			default:
				break;
			}
		}

		return true;
	}

	std::expected<std::optional<ImGuiPage::FrameContext>, Error> ImGuiPage::prepare_frame(
		resource::Context& context
	) noexcept
	{
		frame_sync_primitives.cycle();
		command_buffers.cycle();

		if (const auto wait_result = context.device->waitForFences(
				{frame_sync_primitives.current().draw_fence},
				vk::True,
				std::numeric_limits<uint64_t>::max()
			);
			wait_result != vk::Result::eSuccess)
			return Error::from(wait_result);

		const auto swapchain_result = context.swapchain.acquire_next(
			context.instance,
			context.device,
			frame_sync_primitives.current().image_available_semaphore
		);
		if (!swapchain_result) return swapchain_result.error().forward("Acquire swapchain failed");
		return swapchain_result->transform([this](vulkan::SwapchainContext::Frame frame) {
			return FrameContext{
				.command_buffer = command_buffers.current(),
				.sync = frame_sync_primitives.current(),
				.render_complete_semaphore = render_complete_semaphores[frame.index],
				.swapchain = frame,
			};
		});
	}

	std::expected<void, Error> ImGuiPage::draw_frame(
		resource::Context& context,
		const FrameContext& frame_context
	) noexcept
	{
		if (const auto result = frame_context.command_buffer.begin({
				.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
			});
			!result)
			return Error::from(result);

		const auto swapchain_acquire_image_barrier = vk::ImageMemoryBarrier2{
			.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
			.srcAccessMask = {},
			.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
			.oldLayout = vk::ImageLayout::eUndefined,
			.newLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
			.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
			.image = frame_context.swapchain.attachment.image,
			.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
		};
		frame_context.command_buffer.pipelineBarrier2(
			vk::DependencyInfo().setImageMemoryBarriers(swapchain_acquire_image_barrier)
		);

		const auto swapchain_attachment_info = vk::RenderingAttachmentInfo{
			.imageView = frame_context.swapchain.attachment.view,
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f),
		};
		const auto render_area = vk::Rect2D{
			.offset = {.x = 0,									.y = 0                                    },
			.extent = {.width = frame_context.swapchain.extent.x, .height = frame_context.swapchain.extent.y},
		};
		const auto rendering_info =
			vk::RenderingInfo()
				.setRenderArea(render_area)
				.setLayerCount(1)
				.setColorAttachments(swapchain_attachment_info);

		frame_context.command_buffer.beginRendering(rendering_info);
		{
			if (const auto draw_result = context.imgui.draw(frame_context.command_buffer); !draw_result)
				return draw_result.error().forward("Draw ImGui failed");
		}
		frame_context.command_buffer.endRendering();

		const auto swapchain_image_present_barrier = vk::ImageMemoryBarrier2{
			.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
			.dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,
			.dstAccessMask = {},
			.oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.newLayout = vk::ImageLayout::ePresentSrcKHR,
			.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
			.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
			.image = frame_context.swapchain.attachment.image,
			.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
		};
		frame_context.command_buffer.pipelineBarrier2(
			vk::DependencyInfo{}.setImageMemoryBarriers(swapchain_image_present_barrier)
		);

		if (const auto result = frame_context.command_buffer.end(); !result) return Error::from(result);

		{
			const auto wait_semaphores =
				std::to_array<vk::Semaphore>({frame_context.sync.image_available_semaphore});
			const auto signal_semaphores =
				std::to_array<vk::Semaphore>({frame_context.render_complete_semaphore});
			const auto submit_buffers = std::to_array<vk::CommandBuffer>({frame_context.command_buffer});
			const auto wait_stages =
				std::to_array<vk::PipelineStageFlags>({vk::PipelineStageFlagBits::eColorAttachmentOutput});
			const auto graphic_submit_info =
				vk::SubmitInfo{}
					.setCommandBuffers(submit_buffers)
					.setWaitSemaphores(wait_semaphores)
					.setSignalSemaphores(signal_semaphores)
					.setWaitDstStageMask(wait_stages);

			if (const auto result = context.device->resetFences({frame_context.sync.draw_fence}); !result)
				return Error::from(result);

			{
				const std::scoped_lock lock(context.device.get().submit_mutex);

				if (const auto result =
						context.device.get().queue.submit(graphic_submit_info, frame_context.sync.draw_fence);
					!result)
					return Error::from(result);

				if (const auto present_result = context.swapchain.present(
						context.device,
						frame_context.swapchain,
						frame_context.render_complete_semaphore
					);
					!present_result)
					return present_result.error().forward("Present frame failed");
			}
		}

		return {};
	}
}
