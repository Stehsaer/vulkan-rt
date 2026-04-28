#pragma once

#include "model/texture.hpp"
#include "render/model/material.hpp"

#include <cstdint>
#include <expected>
#include <map>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace render::impl
{
	///
	/// @brief Material collector
	/// @details Collect materials, arrange its image views and samplers as linear array
	///
	class MaterialCollector
	{
	  public:

		///
		/// @brief Result of material collection
		/// @details Call `std::move(collector).collect()` to obtain the result
		///
		struct Result
		{
			std::vector<vk::raii::ImageView> textures;
			std::vector<vk::raii::Sampler> samplers;
			std::vector<std::pair<vk::ImageView, vk::Sampler>> combined;
		};

		///
		/// @brief Collect textures, samplers, and combined descriptors
		/// @note Call with `std::move` (rvalue) to get the collected resources
		///
		/// @return Collected textures, samplers, and combined descriptors, or error
		///
		[[nodiscard]]
		Result collect() && noexcept;

		///
		/// @brief Add a material's textures to the collector and get the corresponding texture index
		///
		/// @param device Vulkan device
		/// @param texture_list Texture list to get texture references from
		/// @param texture_set CPU-side texture set to collect
		/// @return Texture index for the collected material, or error
		///
		[[nodiscard]]
		std::expected<TextureIndex, Error> add_material(
			const vk::raii::Device& device,
			const TextureList& texture_list,
			const model::TextureSet& texture_set
		) noexcept;

	  private:

		// Array of textures
		std::vector<vk::raii::ImageView> textures;

		// Maps texture reference and usage to texture index in `textures`, for caching
		std::map<std::pair<Texture::Ref, Texture::Usage>, uint32_t> texture_indices;

		// Array of samplers
		std::vector<vk::raii::Sampler> samplers;

		// Maps sample mode to sampler index in `samplers`, for caching
		std::map<model::SampleMode, uint32_t> sampler_indices;

		// References to textures and samplers, for constructing combined descriptors
		std::vector<std::pair<vk::ImageView, vk::Sampler>> combined;

		// Maps texture index and sampler index to combined index in `combined`, for caching
		std::map<std::pair<uint32_t, uint32_t>, uint32_t> combined_indices;

		std::expected<uint32_t, Error> add_texture(
			const vk::raii::Device& device,
			Texture::Ref texture_ref,
			Texture::Usage usage,
			model::SampleMode sample_mode
		) noexcept;
	};
}
