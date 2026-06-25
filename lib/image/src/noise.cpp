#include "image/noise.hpp"
#include "common/util/error.hpp"
#include "image/common.hpp"
#include "image/image.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <glm/ext/vector_uint2_sized.hpp>
#include <mdspan>
#include <memory>
#include <mutex>
#include <span>
#include <utility>

extern "C"
{
	extern const std::byte _binary_blue_noise_png_start;
	extern const std::byte _binary_blue_noise_png_end;

	extern const glm::u8vec2 _binary_stbn_bin_start;
	extern const glm::u8vec2 _binary_stbn_bin_end;  // UNUSED
}

namespace image
{
	static std::span<const std::byte> blue_noise_data =
		{&_binary_blue_noise_png_start, &_binary_blue_noise_png_end};

	static std::unique_ptr<Image<Format::Unorm16, Layout::RGBA>> blue_noise_image = nullptr;
	static std::mutex blue_noise_mutex;

	std::expected<Image<Format::Unorm16, Layout::RGBA>, Error> get_blue_noise() noexcept
	{
		const std::scoped_lock lock(blue_noise_mutex);

		if (blue_noise_image) return *blue_noise_image;

		auto result = Image<Format::Unorm16, Layout::RGBA>::decode(blue_noise_data);
		if (!result) return result.error().forward("Decode embedded blue noise image failed");
		blue_noise_image = std::make_unique<Image<Format::Unorm16, Layout::RGBA>>(std::move(*result));

		return *blue_noise_image;
	}

	extern const std::mdspan<const glm::u8vec2, std::extents<uint32_t, 64, 128, 128>>
		stbn_data{&_binary_stbn_bin_start, 64, 128, 128};  // NOLINT
}
