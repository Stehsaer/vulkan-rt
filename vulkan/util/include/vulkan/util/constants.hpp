#pragma once

#include <cstdint>
#include <vulkan/vulkan.hpp>

namespace vulkan::util::constant
{
	namespace subres
	{
		static constexpr auto color_attachment = vk::ImageSubresourceRange{
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		};

		static constexpr auto depth_only_attachment = vk::ImageSubresourceRange{
			.aspectMask = vk::ImageAspectFlagBits::eDepth,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		};

		static constexpr auto depth_stencil_attachment = vk::ImageSubresourceRange{
			.aspectMask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		};
	}
}