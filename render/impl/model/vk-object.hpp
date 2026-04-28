#pragma once

#include <vulkan/vulkan_raii.hpp>

#include "model/texture.hpp"
#include "render/model/texture.hpp"

namespace render::impl
{
	///
	/// @brief Create a sampler from sample mode
	///
	/// @param device Vulkan device
	/// @param sample_mode Sample mode
	/// @return Created sampler, or error
	///
	[[nodiscard]]
	std::expected<vk::raii::Sampler, Error> to_sampler(
		const vk::raii::Device& device,
		model::SampleMode sample_mode
	) noexcept;

	///
	/// @brief Create a image view based on texture reference and usage
	///
	/// @param device Vulkan device
	/// @param texture_ref Texture reference
	/// @param usage Texture usage
	/// @return Created image view, or error
	///
	[[nodiscard]]
	std::expected<vk::raii::ImageView, Error> to_image_view(
		const vk::raii::Device& device,
		Texture::Ref texture_ref,
		Texture::Usage usage
	) noexcept;
}
