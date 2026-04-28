#pragma once

#include "common/util/error.hpp"
#include "vulkan/alloc/image.hpp"
#include "vulkan/interface/common-context.hpp"

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

		///
		/// @brief Create auxiliary resource
		///
		/// @param context Vulkan context
		/// @return Created resource or error
		///
		[[nodiscard]]
		static std::expected<AuxResource, Error> create(const vulkan::DeviceContext& context) noexcept;
	};
}
