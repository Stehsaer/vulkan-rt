#pragma once

#include "common/util/error.hpp"
#include "image/common.hpp"
#include "model/texture.hpp"
#include "vulkan/alloc/image.hpp"
#include "vulkan/util/static-resource-creator.hpp"

#include <vulkan/vulkan_raii.hpp>

namespace render
{
	///
	/// @brief GPU Texture class
	///
	///
	class Texture
	{
	  public:

		///
		/// @brief Format of the texture
		///
		///
		enum class Format
		{
			Rgba8Unorm,
			BC3,
			BC7,

			Rg8Unorm,
			Rg16Unorm,
			BC5,
		};

		///
		/// @brief Usage enum for texture loading
		///
		///
		enum class Usage
		{
			Color,
			Linear,
			Normal
		};

		///
		/// @brief Strategy for loading color texture
		///
		///
		enum class ColorLoadStrategy
		{
			Raw,        // Load raw Unorm8 only
			AllBC3,     // Load all files in BC3
			AllBC7,     // Load all files in BC7
			BalancedBC  // Load smaller images in BC7 (Max dim <= 1024px after resize)
		};

		static constexpr size_t BC7_THRESHOLD = 1024;

		///
		/// @brief Strategy for loading normal map texture
		///
		///
		enum class NormalLoadStrategy
		{
			AllUnorm8,         // Load all images as Unorm8
			AdaptiveUnorm,     // Load 16-bit images as Unorm16, 8-bit images as Unorm8
			AdaptiveUnormBC5,  // Load 16-bit images as Unorm16, 8-bit images as BC5
			AllBC5,            // Load all images as BC5
		};

		///
		/// @brief Reference to texture
		///
		///
		struct Ref
		{
			vk::Image image;
			Format format;
			uint32_t mipmap_levels;

			auto operator<=>(const Ref& other) const { return image <=> other.image; }
			bool operator==(const Ref& other) const { return image == other.image; };

			///
			/// @brief Get vulkan format for this texture
			///
			/// @param usage Usage of the texture
			/// @retval vk::Format::eUndefined if the texture usage isn't compatible with the texture format
			/// @retval vk::Format::(!eUndefined) if the texture format is compatible with the usage, the
			/// returned format is the optimal format for that usage
			///
			[[nodiscard]]
			vk::Format get_format(Usage usage) noexcept;
		};

		vulkan::Image image;
		Format format;
		uint32_t mipmap_levels;

		///
		/// @brief Load a color texture from a `lib.model` texture
		/// @warning Calling this function will add image upload tasks to the creator, but will not execute
		/// them. Call `vulkan::StaticResourceCreator::execute_uploads` to actually execute the upload tasks.
		///
		/// @param load_context Load context
		/// @param texture Input texture
		/// @param load_strategy Color image loading strategy
		/// @return Loaded texture, or error
		///
		[[nodiscard]]
		static std::expected<Texture, Error> load_color_texture(
			vulkan::StaticResourceCreator& resource_creator,
			const model::Texture& texture,
			ColorLoadStrategy load_strategy,
			vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eSampled
		) noexcept;

		///
		/// @brief Load a normal map texture from a `lib.model` texture
		/// @warning Calling this function will add image upload tasks to the creator, but will
		/// not execute them. Call `vulkan::StaticResourceCreator::execute_uploads` to actually execute the
		/// upload tasks.
		///
		/// @param load_context Load context
		/// @param texture Input texture
		/// @param load_strategy Normal map loading strategy
		/// @return Loaded texture, or error
		///
		[[nodiscard]]
		static std::expected<Texture, Error> load_normal_texture(
			vulkan::StaticResourceCreator& resource_creator,
			const model::Texture& texture,
			NormalLoadStrategy load_strategy,
			vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eSampled
		) noexcept;

		///
		/// @brief Get reference to this texture
		///
		/// @return Reference to this texture
		///
		[[nodiscard]]
		Ref ref() const noexcept
		{
			return {.image = image, .format = format, .mipmap_levels = mipmap_levels};
		}

	  private:

		/* Load Functions */

		// Load RGBA8_UNORM image, if `allow_resize` is set to true, NPOT image is resized as POT image and
		// mipmap is generated
		static std::expected<Texture, Error> load_rgba8_unorm(
			vulkan::StaticResourceCreator& resource_creator,
			const image::Image<image::Format::Unorm8, image::Layout::RGBA>& image,
			vk::ImageUsageFlags usage
		) noexcept;

		// Load BCn image, resize and generate mipmap
		static std::expected<Texture, Error> load_bcn(
			vulkan::StaticResourceCreator& resource_creator,
			const image::Image<image::Format::Unorm8, image::Layout::RGBA>& image,
			image::BCnFormat format,
			vk::ImageUsageFlags usage
		) noexcept;

		static std::expected<Texture, Error> load_rg8_unorm(
			vulkan::StaticResourceCreator& resource_creator,
			const image::Image<image::Format::Unorm8, image::Layout::RGBA>& image,
			vk::ImageUsageFlags usage
		) noexcept;

		static std::expected<Texture, Error> load_rg16_unorm(
			vulkan::StaticResourceCreator& resource_creator,
			const image::Image<image::Format::Unorm16, image::Layout::RGBA>& image,
			vk::ImageUsageFlags usage
		) noexcept;
	};
}
