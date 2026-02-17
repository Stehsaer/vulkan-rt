#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>

namespace vulkan
{
	///
	/// @brief Get the image subresource range for an image with 1 mip-level and 1 layer
	///
	/// @param aspect_flags The aspect flags of the image, e.g. `vk::ImageAspectFlagBits::eColor` for a color
	/// image
	/// @return The image subresource range for the given aspect flags
	///
	constexpr vk::ImageSubresourceRange base_level_image(vk::ImageAspectFlags aspect_flags) noexcept
	{
		return vk::ImageSubresourceRange{
			.aspectMask = aspect_flags,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		};
	}
}