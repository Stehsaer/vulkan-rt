#pragma once

#include "model/texture.hpp"

namespace render::impl
{
	///
	/// @brief Convert model's filter enum to Vulkan filter enum
	///
	/// @param filter Model filter enum value to convert
	/// @return Corresponding Vulkan filter enum value
	///
	[[nodiscard]]
	vk::Filter as_filter(model::Filter filter) noexcept;

	///
	/// @brief Convert model's filter enum to Vulkan mipmap mode enum
	///
	/// @param filter Model filter enum value to convert
	/// @return Corresponding Vulkan mipmap mode enum value
	///
	[[nodiscard]]
	vk::SamplerMipmapMode as_mipmap_mode(model::Filter filter) noexcept;

	///
	/// @brief Convert model's wrap enum to Vulkan address mode enum
	///
	/// @param wrap Model wrap enum value to convert
	/// @return Corresponding Vulkan address mode enum value
	///
	[[nodiscard]]
	vk::SamplerAddressMode as_address_mode(model::Wrap wrap) noexcept;
}
