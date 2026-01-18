#include "image/bc-image.hpp"

#include <algorithm>
#include <bc7enc.h>
#include <mutex>
#include <rgbcx.h>
#include <stb_dxt/stb_dxt.h>

namespace image
{
	static std::array<RawPixel<Precision::Uint8, Layout::RGBA>, 16> slice_block(
		const RawImage<Precision::Uint8, Layout::RGBA>& raw_image,
		glm::u32vec2 block
	) noexcept
	{
		std::array<RawPixel<Precision::Uint8, Layout::RGBA>, 16> pixel_block;
		const std::span<RawPixel<Precision::Uint8, Layout::RGBA>> pixel_block_span(pixel_block);

		const auto dst_row0 = pixel_block_span.subspan(0, 4);
		const auto dst_row1 = pixel_block_span.subspan(4, 4);
		const auto dst_row2 = pixel_block_span.subspan(8, 4);
		const auto dst_row3 = pixel_block_span.subspan(12, 4);

		const auto src_row0 =
			std::span(raw_image.data).subspan(((block.y * 4 + 0) * raw_image.size.x + block.x * 4), 4);
		const auto src_row1 =
			std::span(raw_image.data).subspan(((block.y * 4 + 1) * raw_image.size.x + block.x * 4), 4);
		const auto src_row2 =
			std::span(raw_image.data).subspan(((block.y * 4 + 2) * raw_image.size.x + block.x * 4), 4);
		const auto src_row3 =
			std::span(raw_image.data).subspan(((block.y * 4 + 3) * raw_image.size.x + block.x * 4), 4);

		std::ranges::copy(src_row0, dst_row0.begin());
		std::ranges::copy(src_row1, dst_row1.begin());
		std::ranges::copy(src_row2, dst_row2.begin());
		std::ranges::copy(src_row3, dst_row3.begin());

		return pixel_block;
	}

	void BlockCompressedImage::iterate_blocks(
		const RawImage<Precision::Uint8, Layout::RGBA>& raw_image,
		auto process_func
	) noexcept
	{
		for (const auto [y, x] : std::views::cartesian_product(
				 std::views::iota(uint32_t(0), block_dim.y),
				 std::views::iota(uint32_t(0), block_dim.x)
			 ))
		{
			const glm::u32vec2 coord{x, y};
			const auto pixel_block = slice_block(raw_image, coord);
			std::invoke(process_func, this->block_at(coord), pixel_block);
		}
	}

	static void encode_bc3_block(
		CompressionBlock& block,
		const std::array<RawPixel<Precision::Uint8, Layout::RGBA>, 16>& pixel_block
	)
	{
		stb_compress_dxt_block(
			reinterpret_cast<uint8_t*>(block.data.data()),
			reinterpret_cast<const uint8_t*>(pixel_block.data()),
			1,
			10
		);
	}

	static void encode_bc5_block(
		CompressionBlock& block,
		const std::array<RawPixel<Precision::Uint8, Layout::RGBA>, 16>& pixel_block
	)
	{
		rgbcx::encode_bc5(
			reinterpret_cast<uint8_t*>(block.data.data()),
			reinterpret_cast<const uint8_t*>(pixel_block.data())
		);
	}

	std::expected<BlockCompressedImage, Error> BlockCompressedImage::encode_bc3(
		const RawImage<Precision::Uint8, Layout::RGBA>& raw_image
	) noexcept
	{
		BlockCompressedImage bc3_image(BCnFormat::BC3, raw_image.size / uint32_t(4));
		bc3_image.iterate_blocks(raw_image, encode_bc3_block);
		return bc3_image;
	}

	std::expected<BlockCompressedImage, Error> BlockCompressedImage::encode_bc5(
		const RawImage<Precision::Uint8, Layout::RGBA>& raw_image
	) noexcept
	{
		BlockCompressedImage bc5_image(BCnFormat::BC5, raw_image.size / uint32_t(4));
		bc5_image.iterate_blocks(raw_image, encode_bc5_block);
		return bc5_image;
	}

	std::expected<BlockCompressedImage, Error> BlockCompressedImage::encode_bc7(
		const RawImage<Precision::Uint8, Layout::RGBA>& raw_image
	) noexcept
	{
		static std::once_flag bc7_init_flag;

		BlockCompressedImage bc7_image(BCnFormat::BC7, raw_image.size / uint32_t(4));

		std::call_once(bc7_init_flag, [] { bc7enc_compress_block_init(); });

		bc7enc_compress_block_params params{};
		bc7enc_compress_block_params_init(&params);
		bc7enc_compress_block_params_init_perceptual_weights(&params);

		const auto encode_bc7_block =
			[&params](
				CompressionBlock& block,
				const std::array<RawPixel<Precision::Uint8, Layout::RGBA>, 16>& pixel_block
			) {
				bc7enc_compress_block(
					reinterpret_cast<uint8_t*>(block.data.data()),
					reinterpret_cast<const uint8_t*>(pixel_block.data()),
					&params
				);
			};

		bc7_image.iterate_blocks(raw_image, encode_bc7_block);
		return bc7_image;
	}

	std::expected<BlockCompressedImage, Error> BlockCompressedImage::encode(
		const RawImage<Precision::Uint8, Layout::RGBA>& raw_image,
		BCnFormat format
	) noexcept
	{
		if (raw_image.size.x % 4 != 0 || raw_image.size.y % 4 != 0)
			return Error(
				std::format(
					"Raw image dimensions must be multiples of 4 for BCn compression, got {}",
					raw_image.size
				)
			);

		switch (format)
		{
		case BCnFormat::BC3:
			return encode_bc3(raw_image);
		case BCnFormat::BC5:
			return encode_bc5(raw_image);
		case BCnFormat::BC7:
			return encode_bc7(raw_image);
		default:
			return Error(std::format("Invalid argument: {}", static_cast<int>(format)));
		}
	}
}