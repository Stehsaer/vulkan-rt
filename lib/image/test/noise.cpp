#include "image/noise.hpp"

#include <doctest.h>

TEST_CASE("Blue noise")
{
	auto blue_noise_result = image::get_blue_noise();
	REQUIRE(blue_noise_result.has_value());

	const auto& blue_noise = blue_noise_result.value();
	REQUIRE_EQ(blue_noise.size, glm::u32vec2(128, 128));
}