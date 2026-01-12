#include "vulkan-util/image-barrier.hpp"
#include "vulkan-util/constants.hpp"

namespace vkutil::image_barrier
{
	vk::ImageMemoryBarrier2 swapchain_acquire(vk::Image image) noexcept
	{
		return {
			.srcStageMask = vk::PipelineStageFlagBits2::eNone,
			.srcAccessMask = vk::AccessFlagBits2::eNone,
			.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
			.oldLayout = vk::ImageLayout::eUndefined,
			.newLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = image,
			.subresourceRange = constant::subres::color_attachment,
		};
	}

	vk::ImageMemoryBarrier2 swapchain_present(vk::Image image) noexcept
	{
		return {
			.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
			.dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,
			.dstAccessMask = vk::AccessFlagBits2::eNone,
			.oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.newLayout = vk::ImageLayout::ePresentSrcKHR,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = image,
			.subresourceRange = constant::subres::color_attachment,
		};
	}
}