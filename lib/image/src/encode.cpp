#include "image/impl/encode.hpp"
#include "common/util/overload.hpp"

#include <stb_image_write.h>

namespace image::impl
{
	static void write_func(void* context, void* data, int size) noexcept
	{
		auto& vec = reinterpret_cast<std::reference_wrapper<std::vector<std::byte>>*>(context)->get();
		vec.append_range(std::span(reinterpret_cast<const std::byte*>(data), size));
	}

	std::expected<std::vector<std::byte>, Error> encode(
		EncodeFormat format,
		glm::u32vec2 size,
		Layout layout,
		std::span<const std::byte> data
	) noexcept
	{
		std::vector<std::byte> encoded_data;
		auto encoded_data_ref = std::ref(encoded_data);

		const auto stbi_write_result = std::visit(
			util::Overload(
				[&size, &layout, &data, &encoded_data_ref](const encode_format::Png& png_option) {
					stbi_write_png_compression_level = png_option.compression_level;
					return stbi_write_png_to_func(
						write_func,
						&encoded_data_ref,
						size.x,
						size.y,
						std::to_underlying(layout),
						data.data(),
						sizeof(FormatType<Format::Unorm8>) * std::to_underlying(layout) * size.x
					);
				},
				[&size, &layout, &data, &encoded_data_ref](const encode_format::Jpg& jpg_option) {
					return stbi_write_jpg_to_func(
						write_func,
						&encoded_data_ref,
						size.x,
						size.y,
						std::to_underlying(layout),
						data.data(),
						jpg_option.quality
					);
				},
				[&size, &layout, &data, &encoded_data_ref](
					const encode_format::Bmp& bmp_option [[maybe_unused]]
				) {
					return stbi_write_bmp_to_func(
						write_func,
						&encoded_data_ref,
						size.x,
						size.y,
						std::to_underlying(layout),
						data.data()
					);
				}
			),
			format
		);

		if (stbi_write_result == 0) return Error("Encode image failed");

		return encoded_data;
	}
}
