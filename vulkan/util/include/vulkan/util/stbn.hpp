#pragma once

#include "common/util/error.hpp"
#include "vulkan/alloc/image.hpp"
#include "vulkan/interface/context.hpp"

#include <expected>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	///
	/// @brief Spatiotemporal noise image
	/// @note See `image::stbn_data` for relevant information
	///
	struct STBN
	{
		vulkan::Image image;
		vk::raii::ImageView view;

		static std::expected<STBN, Error> create(const vulkan::Context& context) noexcept;
	};
}
