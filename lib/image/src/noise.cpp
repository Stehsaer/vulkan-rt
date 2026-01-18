#include "image/noise.hpp"
#include <mutex>

extern const std::byte _binary_blue_noise_png_start;
extern const std::byte _binary_blue_noise_png_end;

namespace image
{
	static std::span<const std::byte> blue_noise_data =
		{&_binary_blue_noise_png_start, &_binary_blue_noise_png_end};

	static std::unique_ptr<RawImage<Precision::Uint16, Layout::RGBA>> blue_noise_image = nullptr;
	static std::mutex blue_noise_mutex;

	std::expected<RawImage<Precision::Uint16, Layout::RGBA>, Error> get_blue_noise() noexcept
	{
		const std::scoped_lock lock(blue_noise_mutex);

		if (blue_noise_image) return *blue_noise_image;

		auto result = RawImage<Precision::Uint16, Layout::RGBA>::decode(blue_noise_data);
		if (!result) return result.error().forward("Decode embedded blue noise image failed");
		blue_noise_image = std::make_unique<RawImage<Precision::Uint16, Layout::RGBA>>(std::move(*result));

		return *blue_noise_image;
	}
}