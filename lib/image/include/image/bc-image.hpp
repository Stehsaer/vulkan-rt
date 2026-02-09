#pragma once

#include "image/common.hpp"
#include "image/raw-image.hpp"

#include <expected>

namespace image
{
	///
	/// @brief Block for BCn compression formats
	/// @note 16 bytes in size, deliberately aligned to 16 bytes for optimal access
	///
	struct alignas(16) CompressionBlock
	{
		std::array<std::byte, 16> data;
	};

	template <>
	inline constexpr bool indexable_pixel_type_flag<CompressionBlock> = false;

	enum class BCnFormat
	{
		BC3,
		BC5,
		BC7
	};

	struct BlockCompressedImage : public Container<CompressionBlock>
	{
		BCnFormat format;
		glm::u32vec2 block_dim;

		///
		/// @brief Encode a raw image into a BCn compressed image
		///
		/// @param raw_image Raw image to encode
		/// @param format BCn compression format to use
		/// @return Encoded block compressed image or an error
		///
		[[nodiscard]]
		static std::expected<BlockCompressedImage, Error> encode(
			const RawImage<Precision::Uint8, Layout::RGBA>& raw_image,
			BCnFormat format
		) noexcept;

	  private:

		BlockCompressedImage(BCnFormat format, glm::u32vec2 block_dim) noexcept :
			Container<CompressionBlock>{
				.size = block_dim * uint32_t(4),
				.data = std::vector<CompressionBlock>(block_dim.x * block_dim.y)
			},
			format(format),
			block_dim(block_dim)
		{}

		// (Helper) Get block at specified block coordinates
		[[nodiscard]]
		auto& block_at(this auto& self, glm::u32vec2 block_coord) noexcept
		{
			assert(block_coord.x < self.block_dim.x && block_coord.y < self.block_dim.y);
			return self.data[block_coord.y * self.block_dim.x + block_coord.x];
		}

		// (Helper) Iterate over all blocks in the image and apply a function
		void iterate_blocks(const RawImage<Precision::Uint8, Layout::RGBA>& raw_image, auto func) noexcept;

		/// (Helper) Encode a single BC3 block
		[[nodiscard]]
		static std::expected<BlockCompressedImage, Error> encode_bc3(
			const RawImage<Precision::Uint8, Layout::RGBA>& raw_image
		) noexcept;

		// (Helper) Encode a single BC5 block
		[[nodiscard]]
		static std::expected<BlockCompressedImage, Error> encode_bc5(
			const RawImage<Precision::Uint8, Layout::RGBA>& raw_image
		) noexcept;

		// (Helper) Encode a single BC7 block
		[[nodiscard]]
		static std::expected<BlockCompressedImage, Error> encode_bc7(
			const RawImage<Precision::Uint8, Layout::RGBA>& raw_image
		) noexcept;

	  public:

		BlockCompressedImage(const BlockCompressedImage&) = default;
		BlockCompressedImage(BlockCompressedImage&&) = default;
		BlockCompressedImage& operator=(const BlockCompressedImage&) = default;
		BlockCompressedImage& operator=(BlockCompressedImage&&) = default;
	};
}
