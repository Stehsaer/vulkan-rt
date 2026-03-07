#include "image/noise.hpp"
#include "common/test-macro.hpp"

#include <doctest.h>

TEST_CASE("Blue noise")
{
	auto blue_noise_result = image::get_blue_noise();
	EXPECT_SUCCESS(blue_noise_result);

	const auto& blue_noise = blue_noise_result.value();
	REQUIRE_VEC2_EQ(blue_noise.size, 128, 128);
}
