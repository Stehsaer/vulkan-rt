#pragma once

#include "common/util/error.hpp"
#include "vulkan/alloc/image.hpp"
#include "vulkan/interface/context.hpp"

#include <expected>
#include <vulkan/vulkan_raii.hpp>

namespace resource
{
	///
	/// @brief Auxiliary resources
	///
	///
	struct AuxResource
	{
		vulkan::Image exposure_mask;             // Exposure mask
		vk::raii::ImageView exposure_mask_view;  // Image view of exposure mask

		vulkan::Image noise;
		vk::raii::ImageView noise_view;

		///
		/// @brief Create auxiliary resource
		///
		/// @param context Vulkan context
		/// @return Created resource or error
		///
		[[nodiscard]]
		static std::expected<AuxResource, Error> create(const vulkan::Context& context) noexcept;
	};
}
