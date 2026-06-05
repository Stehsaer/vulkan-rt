#pragma once

#include <vulkan/vulkan.hpp>

namespace vulkan
{
	///
	/// @brief Generic attachment reference struct
	///
	struct AttachmentView
	{
		vk::Format format = vk::Format::eUndefined;
		vk::Image image = nullptr;
		vk::ImageView view = nullptr;
	};
}
