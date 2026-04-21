#pragma once

#include <cstddef>
#include <filesystem>
#include <glm/glm.hpp>
#include <variant>
#include <vulkan/vulkan.hpp>

#include "common/util/error.hpp"
#include "image/image.hpp"

namespace model
{
	enum class Filter
	{
		Nearest,
		Linear,
	};

	enum class Wrap
	{
		Repeat,
		MirroredRepeat,
		ClampToEdge
	};

	///
	/// @brief Sample mode for a texture
	///
	struct SampleMode
	{
		Filter min_filter = Filter::Linear;
		Filter mag_filter = Filter::Linear;
		Filter mipmap_filter = Filter::Linear;

		Wrap wrap_u = Wrap::Repeat;
		Wrap wrap_v = Wrap::Repeat;

		float max_mipmap_level = vk::LodClampNone;

		[[nodiscard]]
		auto operator<=>(const SampleMode&) const noexcept = default;

		[[nodiscard]]
		bool operator==(const SampleMode&) const noexcept = default;
	};

	///
	/// @brief Texture class, supports deferred loading from various sources
	/// @details Stores the source data in one of these types:
	/// - `std::filesystem::path`: Load the texture from a file path
	/// - `std::vector<std::byte>`: Load the texture from memory data
	/// - `image::Image`: Load the texture from an already decoded image (8-bit or 16-bit)
	///
	/// Additionally, the flip options ( @p flip_x and @p flip_y) can be used to specify whether to flip the
	/// texture on load, which is useful for textures that are stored in different coordinate systems.
	///
	/// To load the texture data, call `load()` or `load_xbit()`
	///
	struct Texture
	{
		using SourceType = std::variant<
			std::filesystem::path,
			std::vector<std::byte>,
			image::Image<image::Format::Unorm8, image::Layout::RGBA>,
			image::Image<image::Format::Unorm16, image::Layout::RGBA>
		>;

		using ImageVariant = std::variant<
			image::Image<image::Format::Unorm8, image::Layout::RGBA>,
			image::Image<image::Format::Unorm16, image::Layout::RGBA>
		>;

		SourceType source;                      // Source data
		SampleMode sample_mode = SampleMode();  // Sample mode of the texture
		bool flip_x = false;                    // Flip x axis on load
		bool flip_y = false;                    // Flilp y axis on load

		///
		/// @brief Load the texture data
		///
		/// @retval image::Image<image::Format::Unorm8, image::Layout::RGBA> if the texture is in 8-bit
		/// format
		/// @retval image::Image<image::Format::Unorm16, image::Layout::RGBA> if the texture is in
		/// 16-bit format
		/// @retval Error if the texture data is invalid or failed to decode
		///
		[[nodiscard]]
		std::expected<ImageVariant, Error> load() const noexcept;

		///
		/// @brief Load the texture data in 8bit format
		///
		/// @return Loaded 8bit image or error
		///
		[[nodiscard]]
		std::expected<
			image::Image<image::Format::Unorm8, image::Layout::RGBA>,
			Error
		> load_8bit() const noexcept;

		///
		/// @brief Load the texture data in 16bit format
		///
		/// @return Loaded 16bit image or error
		///
		[[nodiscard]]
		std::expected<
			image::Image<image::Format::Unorm16, image::Layout::RGBA>,
			Error
		> load_16bit() const noexcept;
	};
}
