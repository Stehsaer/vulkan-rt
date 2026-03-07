#pragma once

#include "image/common.hpp"
#include "image/image.hpp"

#include <expected>

namespace image
{
	///
	/// @brief Block for BCn compression formats
	/// @note 16 bytes in size, deliberately aligned to 16 bytes for optimal access
	///
	struct alignas(16) BCnBlock
	{
		std::array<std::byte, 16> data;
	};

	template <>
	inline constexpr bool indexable_pixel_type_flag<BCnBlock> = false;

	enum class BCnFormat
	{
		BC3,
		BC5,
		BC7
	};

	///
	/// @brief Block-compress format image, 8 bits per pixel
	/// @note The @p size member contains the dimensions in 4x4 BCn Block, not the actual pixels
	///
	struct BCnImage : public Container<BCnBlock>
	{
		BCnFormat format;

		///
		/// @brief Encode a raw image into a BCn compressed image
		///
		/// @param raw_image Raw image to encode
		/// @param format BCn compression format to use
		/// @return Encoded block compressed image or an error
		///
		[[nodiscard]]
		static std::expected<BCnImage, Error> encode(
			const Image<Format::Unorm8, Layout::RGBA>& raw_image,
			BCnFormat format
		) noexcept;

	  private:

		BCnImage(BCnFormat format, glm::u32vec2 block_dim) noexcept :
			Container<BCnBlock>{.size = block_dim, .data = std::vector<BCnBlock>(block_dim.x * block_dim.y)},
			format(format)
		{}

		// (Helper) Get block at specified block coordinates
		[[nodiscard]]
		auto& block_at(this auto& self, glm::u32vec2 block_coord) noexcept
		{
			assert(block_coord.x < self.size.x && block_coord.y < self.size.y);
			return self.data[block_coord.y * self.size.x + block_coord.x];
		}

		// (Helper) Iterate over all blocks in the image and apply a function
		void iterate_blocks(const Image<Format::Unorm8, Layout::RGBA>& raw_image, auto func) noexcept;

		/// (Helper) Encode a single BC3 block
		[[nodiscard]]
		static std::expected<BCnImage, Error> encode_bc3(
			const Image<Format::Unorm8, Layout::RGBA>& raw_image
		) noexcept;

		// (Helper) Encode a single BC5 block
		[[nodiscard]]
		static std::expected<BCnImage, Error> encode_bc5(
			const Image<Format::Unorm8, Layout::RGBA>& raw_image
		) noexcept;

		// (Helper) Encode a single BC7 block
		[[nodiscard]]
		static std::expected<BCnImage, Error> encode_bc7(
			const Image<Format::Unorm8, Layout::RGBA>& raw_image
		) noexcept;

	  public:

		BCnImage(const BCnImage&) = default;
		BCnImage(BCnImage&&) = default;
		BCnImage& operator=(const BCnImage&) = default;
		BCnImage& operator=(BCnImage&&) = default;
	};
}
